/* 
** Queue background processing
** 
** @file background.c
** 
** -----------------------------------------------------------------------------
** Enduro/X Middleware Platform for Distributed Transaction Processing
** Copyright (C) 2015, ATR Baltic, SIA. All Rights Reserved.
** This software is released under one of the following licenses:
** GPL or ATR Baltic's license for commercial use.
** -----------------------------------------------------------------------------
** GPL license:
** 
** This program is free software; you can redistribute it and/or modify it under
** the terms of the GNU General Public License as published by the Free Software
** Foundation; either version 2 of the License, or (at your option) any later
** version.
**
** This program is distributed in the hope that it will be useful, but WITHOUT ANY
** WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
** PARTICULAR PURPOSE. See the GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License along with
** this program; if not, write to the Free Software Foundation, Inc., 59 Temple
** Place, Suite 330, Boston, MA 02111-1307 USA
**
** -----------------------------------------------------------------------------
** A commercial use license is available from ATR Baltic, SIA
** contact@atrbaltic.com
** -----------------------------------------------------------------------------
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <regex.h>
#include <utlist.h>
#include <dirent.h>
#include <pthread.h>
#include <signal.h>

#include <ndebug.h>
#include <atmi.h>
#include <atmi_int.h>
#include <typed_buf.h>
#include <ndrstandard.h>
#include <ubf.h>
#include <Exfields.h>
#include <tperror.h>
#include <exnet.h>
#include <ndrxdcmn.h>

#include "tmqueue.h"
#include "../libatmisrv/srv_int.h"
#include <xa_cmn.h>
#include <atmi_int.h>
/*---------------------------Externs------------------------------------*/
/*---------------------------Macros-------------------------------------*/
/*---------------------------Enums--------------------------------------*/
/*---------------------------Typedefs-----------------------------------*/
/*---------------------------Globals------------------------------------*/
public pthread_t G_bacground_thread;
public int G_bacground_req_shutdown = FALSE;    /* Is shutdown request? */


private pthread_mutex_t M_wait_mutex = PTHREAD_MUTEX_INITIALIZER;
private pthread_cond_t M_wait_cond = PTHREAD_COND_INITIALIZER;


MUTEX_LOCKDECL(M_background_lock); /* Background operations sync        */

/*---------------------------Statics------------------------------------*/
/*---------------------------Prototypes---------------------------------*/

/**
 * Lock background operations
 */
public void background_lock(void)
{
    MUTEX_LOCK_V(M_background_lock);
}

/**
 * Un-lock background operations
 */
public void background_unlock(void)
{
    MUTEX_UNLOCK_V(M_background_lock);
}

/**
 * Read the logfiles from the disk (if any we have there...)
 * @return 
 */
public int background_read_q(void)
{
    int ret=SUCCEED;
    
#if 0
    struct dirent **namelist;
    int n;
    int len;
    char tranmask[256];
    char fnamefull[PATH_MAX+1];
    atmi_xa_log_t *pp_tl = NULL;
    
    sprintf(tranmask, "TRN-%ld-%hd-%d-", tpgetnodeid(), G_atmi_env.xa_rmid, 
            G_server_conf.srv_id);
    len = strlen(tranmask);
    /* List the files here. */
    n = scandir(G_tmqueue_cfg.tlog_dir, &namelist, 0, alphasort);
    if (n < 0)
    {
       NDRX_LOG(log_error, "Transaction directory [%s: %s", 
               G_tmqueue_cfg.tlog_dir, strerror(errno));
       ret=FAIL;
       goto out;
    }
    else 
    {
       while (n--)
       {
           if (0==strcmp(namelist[n]->d_name, ".") || 
                       0==strcmp(namelist[n]->d_name, ".."))
               continue;

           /* If it is transaction then parse & load */
           
           /*
           NDRX_LOG(log_debug, "[%s] vs [%s] %d", 
                       namelist[n]->d_name, tranmask, len);
           */
           
           if (0==strncmp(namelist[n]->d_name, tranmask, len))
           {
               sprintf(fnamefull, "%s/%s", G_tmqueue_cfg.tlog_dir, namelist[n]->d_name);
               NDRX_LOG(log_warn, "Resuming transaction: [%s]", 
                       fnamefull);
               if (SUCCEED!=tmq_load_logfile(fnamefull, 
                       namelist[n]->d_name+len, &pp_tl))
               {
                   NDRX_LOG(log_warn, "Faled to resume transaction: [%s]", 
                       fnamefull);
                   FAIL_OUT(ret);
               }
           }
           free(namelist[n]);
       }
       free(namelist);
    }
#endif
out:
    return ret;
}

/**
 * Sleep the thread, with option to wake up (by shutdown).
 * @param sleep_sec
 */
private void thread_sleep(int sleep_sec)
{
    struct timespec wait_time;
    struct timeval now;
    int rt;

    gettimeofday(&now,NULL);

    wait_time.tv_sec = now.tv_sec+sleep_sec;
    wait_time.tv_nsec = now.tv_usec;

    pthread_mutex_lock(&M_wait_mutex);
    rt = pthread_cond_timedwait(&M_wait_cond, &M_wait_mutex, &wait_time);
    pthread_mutex_unlock(&M_wait_mutex);
}

/**
 * Wake up the sleeping thread.
 */
public void background_wakeup(void)
{
    pthread_mutex_lock(&M_wait_mutex);
    pthread_cond_signal(&M_wait_cond);
    pthread_mutex_unlock(&M_wait_mutex);
}

/**
 * Continues transaction background loop..
 * Try to complete the transactions.
 * @return  SUCCEED/FAIL
 */
public int background_loop(void)
{
    int ret = SUCCEED;
    atmi_xa_log_list_t *tx_list;
    atmi_xa_log_list_t *el, *tmp;
    atmi_xa_tx_info_t xai;
    atmi_xa_log_t *p_tl;
    
    memset(&xai, 0, sizeof(xai));
    
    while(!G_bacground_req_shutdown)
    {
        
        background_lock();
        
        background_unlock();
        NDRX_LOG(log_debug, "background - sleep %d", 
                G_tmqueue_cfg.scan_time);
        
        if (!G_bacground_req_shutdown)
            thread_sleep(G_tmqueue_cfg.scan_time);
    }
    
out:
    return ret;
}

/**
 * Background processing of the transactions (Complete them).
 * @return 
 */
public void * background_process(void *arg)
{
    NDRX_LOG(log_error, "***********BACKGROUND PROCESS START ********");
    
   /* 1. Read the transaction records from disk */ 
    /* background_read_log(); */
    
   /* 2. Loop over the transactions and:
    * - Check for in-progress timeouts
    * - Try to abort abortable
    * - Try co commit commitable
    * - Use timers counters from the cli params. 
    */
    
    background_loop();
    
    NDRX_LOG(log_error, "***********BACKGROUND PROCESS END **********");
}

/**
 * Initialize background process
 * @return
 */
public void background_process_init(void)
{
    struct sigaction        actions;
    
    pthread_attr_t pthread_custom_attr;
    pthread_attr_init(&pthread_custom_attr);
    
    /* set some small stacks size, 1M should be fine! */
    pthread_attr_setstacksize(&pthread_custom_attr, 2048*1024);
    pthread_create(&G_bacground_thread, &pthread_custom_attr, 
            background_process, NULL);  
}
