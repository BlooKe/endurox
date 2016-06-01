/* 
** Tmsrv server - transaction monitor
** TODO: Create framework for error processing.
** After that log transaction to hash & to disk for tracking the stuff...
** TODO: We should have similar control like "TP_COMMIT_CONTROL" -
** either return after stuff logged or after really commit completed.
** TODO: Also we have to think about conversations, how xa will act there.
** Error handling:
** - System errors we will track via ATMI interface error functions
** - XA errors will be tracked via XA error interface
** TODO: We need a periodical callback from checking for transaction time-outs!
** TODO: Should we call xa_end for joined transactions? See:
** https://www-01.ibm.com/support/knowledgecenter/SSFKSJ_7.0.1/com.ibm.mq.amqzag.doc/fa13870_.htm
**
** @file tmsrv.c
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
#include <getopt.h>

#include <ndebug.h>
#include <atmi.h>
#include <atmi_int.h>
#include <typed_buf.h>
#include <ndrstandard.h>
#include <ubf.h>
#include <Exfields.h>

#include <exnet.h>
#include <ndrxdcmn.h>

#include "tmsrv.h"
#include "../libatmisrv/srv_int.h"
#include "tperror.h"
#include "userlog.h"
#include <xa_cmn.h>
#include "thpool.h"
/*---------------------------Externs------------------------------------*/
extern int optind, optopt, opterr;
extern char *optarg;
/*---------------------------Macros-------------------------------------*/
/*---------------------------Enums--------------------------------------*/
/*---------------------------Typedefs-----------------------------------*/
/*---------------------------Globals------------------------------------*/
public tmsrv_cfg_t G_tmsrv_cfg;
/*---------------------------Statics------------------------------------*/
private int M_init_ok = FALSE;
/* Wait for one free thread: */
pthread_mutex_t M_wait_th_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t M_wait_th_cond = PTHREAD_COND_INITIALIZER;

/*---------------------------Prototypes---------------------------------*/
private int tx_tout_check(void);
private void tm_chk_one_free_thread(void *ptr, int *p_finish_off);

/**
 * Tmsrv service entry (working thread)
 * @param p_svc - data & len used only...!
 */
void TPTMSRV_TH (void *ptr, int *p_finish_off)
{
    /* Ok we should not handle the commands 
     * TPBEGIN...
     */
    int ret=SUCCEED;
    static __thread int first = TRUE;
    thread_server_t *thread_data = (thread_server_t *)ptr;
    char cmd = EOS;
    int cd;
    
    /**************************************************************************/
    /*                        THREAD CONTEXT RESTORE                          */
    /**************************************************************************/
    UBFH *p_ub = (UBFH *)thread_data->buffer;
    
    /* Do the ATMI init */
    if (first)
    {
        first = FALSE;
        if (SUCCEED!=tpinit(NULL))
        {
            NDRX_LOG(log_error, "Failed to init worker client");
            userlog("tmsrv: Failed to init worker client");
            exit(1);
        }
    }
    
    /* restore context. */
    if (SUCCEED!=tpsrvsetctxdata(thread_data->context_data, SYS_SRV_THREAD))
    {
        userlog("tmsrv: Failed to set context");
        NDRX_LOG(log_error, "Failed to set context");
        exit(1);
    }
    
    cd = thread_data->cd;
    /* free up the transport data.*/
    free(thread_data->context_data);
    free(thread_data);
    /**************************************************************************/
    
    /* get some more stuff! */
    if (Bunused (p_ub) < 4096)
    {
        p_ub = (UBFH *)tprealloc ((char *)p_ub, Bsizeof (p_ub) + 4096);
    }
    
    ndrx_debug_dump_UBF(log_info, "TPTMSRV call buffer:", p_ub);
    
    if (Bget(p_ub, TMCMD, 0, (char *)&cmd, 0L))
    {
        NDRX_LOG(log_error, "Failed to read command code!");
        ret=FAIL;
        goto out;
    }
    NDRX_LOG(log_info, "Got command code: [%c]", cmd);
    
    switch(cmd)
    {
        case ATMI_XA_TPBEGIN:
            
            /* start new tran... */
            if (SUCCEED!=tm_tpbegin(p_ub))
            {
                ret=FAIL;
                goto out;
            }
            break;
        case ATMI_XA_TPCOMMIT:
            
            /* start new tran... */
            if (SUCCEED!=tm_tpcommit(p_ub))
            {
                ret=FAIL;
                goto out;
            }
            break;
        case ATMI_XA_TPABORT:
            
            /* start new tran... */
            if (SUCCEED!=tm_tpabort(p_ub))
            {
                ret=FAIL;
                goto out;
            }
            break;
        case ATMI_XA_PRINTTRANS:
            
            /* request for printing active transactions */
            if (SUCCEED!=tm_tpprinttrans(p_ub, cd))
            {
                ret=FAIL;
                goto out;
            }
            break;
        case ATMI_XA_ABORTTRANS:
            
            /* request for printing active transactions */
            if (SUCCEED!=tm_aborttrans(p_ub))
            {
                ret=FAIL;
                goto out;
            }
            break;
        case ATMI_XA_COMMITTRANS:
            
            /* request for printing active transactions */
            if (SUCCEED!=tm_committrans(p_ub))
            {
                ret=FAIL;
                goto out;
            }
            break;
        case ATMI_XA_TMPREPARE:
            
            /* prepare the stuff locally */
            if (SUCCEED!=tm_tmprepare(p_ub))
            {
                ret=FAIL;
                goto out;
            }
            break;
        case ATMI_XA_TMCOMMIT:
            
            /* prepare the stuff locally */
            if (SUCCEED!=tm_tmcommit(p_ub))
            {
                ret=FAIL;
                goto out;
            }
            break;
        case ATMI_XA_TMABORT:
            
            /* abort the stuff locally */
            if (SUCCEED!=tm_tmabort(p_ub))
            {
                ret=FAIL;
                goto out;
            }
            break;
        case ATMI_XA_TMREGISTER:
            /* Some binary is telling as the different RM is involved
             * in transaction.
             */
            if (SUCCEED!=tm_tmregister(p_ub))
            {
                ret=FAIL;
                goto out;
            }
            break;
        default:
            NDRX_LOG(log_error, "Unsupported command code: [%c]", cmd);
            ret=FAIL;
            break;
    }
    
out:
            
    /* Approve the request if all ok */
    if (SUCCEED==ret)
    {
        atmi_xa_approve(p_ub);
    }

    if (SUCCEED!=ret && XA_RDONLY==atmi_xa_get_reason())
    {
        NDRX_LOG(log_debug, "Marking READ ONLY = SUCCEED");
        ret=SUCCEED;
    }

    ndrx_debug_dump_UBF(log_info, "TPTMSRV return buffer:", p_ub);

    tpreturn(  ret==SUCCEED?TPSUCCESS:TPFAIL,
                0L,
                (char *)p_ub,
                0L,
                0L);
}

/**
 * Entry point for service (main thread)
 * @param p_svc
 */
void TPTMSRV (TPSVCINFO *p_svc)
{
    int ret=SUCCEED;
    UBFH *p_ub = (UBFH *)p_svc->data; /* this is auto-buffer */
    long size;
    char btype[16];
    char stype[16];
    thread_server_t *thread_data = malloc(sizeof(thread_server_t));
    
    if (NULL==thread_data)
    {
        userlog("Failed to malloc memory - %s!", strerror(errno));
        NDRX_LOG(log_error, "Failed to malloc memory");
        FAIL_OUT(ret);
    }
    
    if (0==(size = tptypes (p_svc->data, btype, stype)))
    {
        NDRX_LOG(log_error, "Zero buffer received!");
        userlog("Zero buffer received!");
        FAIL_OUT(ret);
    }
    
    /* not using sub-type - on tpreturn/forward for thread it will be auto-free */
    thread_data->buffer =  tpalloc(btype, NULL, size);
    
    if (NULL==thread_data->buffer)
    {
        NDRX_LOG(log_error, "tpalloc failed of type %s size %ld", btype, size);
        FAIL_OUT(ret);
    }
    
    /* copy off the data */
    memcpy(thread_data->buffer, p_svc->data, size);
    thread_data->cd = p_svc->cd;
    thread_data->context_data = tpsrvgetctxdata();
    
    /* submit the job to thread pool: */
    thpool_add_work(G_tmsrv_cfg.thpool, (void*)TPTMSRV_TH, (void *)thread_data);
    
out:
    if (SUCCEED==ret)
    {
        /* serve next.. 
         * At this point we should know that at least one thread is free
         */
        pthread_mutex_lock(&M_wait_th_mutex);
        
        /* submit the job to verify free thread */
        
        thpool_add_work(G_tmsrv_cfg.thpool, (void*)tm_chk_one_free_thread, NULL);
        pthread_cond_wait(&M_wait_th_cond, &M_wait_th_mutex);
        pthread_mutex_unlock(&M_wait_th_mutex);
        
        tpcontinue();
    }
    else
    {
        /* return error back */
        tpreturn(  TPFAIL,
                0L,
                (char *)p_ub,
                0L,
                0L);
    }

}

/*
 * Do initialization
 */
int tpsvrinit(int argc, char **argv)
{
    int ret=SUCCEED;
    signed char c;
    char svcnm[MAXTIDENT+1];
    NDRX_LOG(log_debug, "tpsvrinit called");
    int nodeid;
    
    /* Parse command line  */
    while ((c = getopt(argc, argv, "t:s:l:c:m:p:")) != -1)
    {
        NDRX_LOG(log_debug, "%c = [%s]", c, optarg);
        switch(c)
        {
            case 't': 
                G_tmsrv_cfg.dflt_timeout = atol(optarg);
                NDRX_LOG(log_debug, "Default transaction time-out "
                            "set to: [%ld]", G_tmsrv_cfg.dflt_timeout);
                break;
                /* status directory: */
            case 'l': 
                strcpy(G_tmsrv_cfg.tlog_dir, optarg);
                NDRX_LOG(log_debug, "Status directory "
                            "set to: [%s]", G_tmsrv_cfg.tlog_dir);
                break;
            case 's': 
                G_tmsrv_cfg.scan_time = atoi(optarg);
                break;
            case 'c': 
                /* Time for time-out checking... */
                G_tmsrv_cfg.tout_check_time = atoi(optarg);
                break;
            case 'm': 
                G_tmsrv_cfg.max_tries = atol(optarg);
                break;
            case 'p': 
                G_tmsrv_cfg.threadpoolsize = atol(optarg);
                break;
            default:
                /*return FAIL;*/
                break;
        }
    }
    
    /* Check the parameters & default them if needed */
    if (0>=G_tmsrv_cfg.scan_time)
    {
        G_tmsrv_cfg.scan_time = SCAN_TIME_DFLT;
    }
    
    if (0>=G_tmsrv_cfg.max_tries)
    {
        G_tmsrv_cfg.max_tries = MAX_TRIES_DFTL;
    }
    
    if (0>=G_tmsrv_cfg.tout_check_time)
    {
        G_tmsrv_cfg.tout_check_time = TOUT_CHECK_TIME;
    }
    
    if (0>=G_tmsrv_cfg.threadpoolsize)
    {
        G_tmsrv_cfg.threadpoolsize = THREADPOOL_DFLT;
    }
    
    if (EOS==G_tmsrv_cfg.tlog_dir[0])
    {
        userlog("TMS log dir not set!");
        NDRX_LOG(log_error, "TMS log dir not set!");
        FAIL_OUT(ret);
    }
    NDRX_LOG(log_debug, "Recovery scan time set to [%d]",
                            G_tmsrv_cfg.scan_time);
    
    NDRX_LOG(log_debug, "Tx max tries set to [%d]",
                            G_tmsrv_cfg.max_tries);
    
    NDRX_LOG(log_debug, "Worker pool size [%d] threads",
                            G_tmsrv_cfg.threadpoolsize);
    
    NDRX_LOG(log_debug, "About to initialize XA!");
    if (SUCCEED!=atmi_xa_init()) /* will open next... */
    {
        NDRX_LOG(log_error, "Failed to initialize XA driver!");
        FAIL_OUT(ret);
    }
    
    /* we should open the XA  */
    
    NDRX_LOG(log_debug, "About to Open XA Entry!");
    ret = atmi_xa_open_entry();
    if( XA_OK != ret )
    {
        userlog("xa_open failed error %d", ret);
        NDRX_LOG(log_error, "xa_open failed");
    }
    else
    {
        NDRX_LOG(log_error, "xa_open ok");
        ret = SUCCEED;
    }
                
    /* All OK, about to advertise services */
    nodeid = tpgetnodeid();
    if (nodeid<1)
    {
        NDRX_LOG(log_error, "Failed to get current node_id");
        FAIL_OUT(ret);
    }
    
    /* very generic version/only Resource ID known */
    
    sprintf(svcnm, NDRX_SVC_RM, G_atmi_env.xa_rmid);
    
    if (SUCCEED!=tpadvertise(svcnm, TPTMSRV))
    {
        NDRX_LOG(log_error, "Failed to advertise %s service!", svcnm);
        FAIL_OUT(ret);
    }
    
    /* generic instance: */
    sprintf(svcnm, NDRX_SVC_TM, nodeid, G_atmi_env.xa_rmid);
    
    if (SUCCEED!=tpadvertise(svcnm, TPTMSRV))
    {
        NDRX_LOG(log_error, "Failed to advertise %s service!", svcnm);
        FAIL_OUT(ret);
    }
    
    /* specific instance */
    sprintf(svcnm, NDRX_SVC_TM_I, nodeid, G_atmi_env.xa_rmid, G_server_conf.srv_id);
    if (SUCCEED!=tpadvertise(svcnm, TPTMSRV))
    {
        NDRX_LOG(log_error, "Failed to advertise %s service!", svcnm);
        FAIL_OUT(ret);
    }
    
    if (NULL==(G_tmsrv_cfg.thpool = thpool_init(G_tmsrv_cfg.threadpoolsize)))
    {
        NDRX_LOG(log_error, "Failed to initialize thread pool (cnt: %d)!", 
                G_tmsrv_cfg.threadpoolsize);
        FAIL_OUT(ret);
    }
    
    /* Start the background processing */
    background_process_init();
    
    
    /* Register timer check (needed for time-out detection) */
    if (SUCCEED!=tpext_addperiodcb(G_tmsrv_cfg.tout_check_time, tx_tout_check))
    {
            ret=FAIL;
            NDRX_LOG(log_error, "tpext_addperiodcb failed: %s",
                            tpstrerror(tperrno));
    }
    
    M_init_ok = TRUE;
    
out:
    return ret;
}

/**
 * Do de-initialization
 */
void tpsvrdone(void)
{
    int i;
    NDRX_LOG(log_debug, "tpsvrdone called - requesting "
            "background thread shutdown...");
    
    G_bacground_req_shutdown = TRUE;
    
    if (M_init_ok)
    {
        background_wakeup();

        /* Terminate the threads */
        for (i=0; i<G_tmsrv_cfg.threadpoolsize; i++)
        {
            thpool_add_work(G_tmsrv_cfg.thpool, (void *)tp_thread_shutdown, NULL);
        }
        
        /* Wait to complete */
        pthread_join(G_bacground_thread, NULL);

        /* Wait for threads to finish */
        thpool_wait(G_tmsrv_cfg.thpool);
        thpool_destroy(G_tmsrv_cfg.thpool);
    }
    atmi_xa_close_entry();
    
}

/**
 * Periodic main thread callback for 
 * (will be done by threadpoll)
 * @return 
 */
private void tx_tout_check_th(void *ptr)
{
    long tspent;
    atmi_xa_log_list_t *tx_list;
    atmi_xa_log_list_t *el, *tmp;
    atmi_xa_tx_info_t xai;
    atmi_xa_log_t *p_tl;
    
    /* Create a copy of hash, iterate and check each tx for timeout condition
     * If so then initiate internal abort call
     */
    NDRX_LOG(log_dump, "Timeout check (processing...)");
    tx_list = tms_copy_hash2list(COPY_MODE_FOREGROUND | COPY_MODE_ACQLOCK);
        
    LL_FOREACH_SAFE(tx_list,el,tmp)
    {
        NDRX_LOG(log_debug, "Checking [%s]...", el->p_tl.tmxid);
        if ((tspent = n_timer_get_delta_sec(&el->p_tl.ttimer)) > 
                el->p_tl.txtout && XA_TX_STAGE_ACTIVE==el->p_tl.txstage)
        {
            NDRX_LOG(log_error, "XID [%s] timed out "
                    "(spent %ld, limit: %ld sec) - aborting...!", 
                    el->p_tl.tmxid, tspent, 
                    el->p_tl.txtout);
            
            userlog("XID [%s] timed out "
                    "(spent %ld, limit: %ld sec) - aborting...!", 
                    el->p_tl.tmxid, tspent, 
                    el->p_tl.txtout);
            
            if (NULL!=(p_tl = tms_log_get_entry(el->p_tl.tmxid)))
            {
                XA_TX_COPY((&xai), p_tl);

                tms_log_stage(p_tl, XA_TX_STAGE_ABORTING);
                /* NOTE: We migth want to move this to background processing
                 * because for example, oracle in some cases does long aborts...
                 * thus it slows down general processing
                 * BUT: if we want to move it to background, we should protect
                 * transaction log from concurent access, e.g.
                 * - background does the abort()
                 * - meanwhile forground calls commit()
                 * This can be reached with per transaction locking...
                 */
                tm_drive(&xai, p_tl, XA_OP_ROLLBACK, FAIL);
            }
        }
        LL_DELETE(tx_list,el);
        free(el);
    }
out:    
    return;
}

/**
 * Callback routine for scheduled timeout checks.
 * @return 
 */
private int tx_tout_check(void)
{
    NDRX_LOG(log_dump, "Timeout check (submit job...)");
    thpool_add_work(G_tmsrv_cfg.thpool, (void*)tx_tout_check_th, NULL);
    /* return SUCCEED; */
    
    return SUCCEED;
}

/**
 * Just run down one task via pool, to ensure that at least one
 * thread is free, before we are going to mail poll.
 * @param ptr
 */
private void tm_chk_one_free_thread(void *ptr, int *p_finish_off)
{
    pthread_mutex_lock(&M_wait_th_mutex);
    pthread_cond_signal(&M_wait_th_cond);
    pthread_mutex_unlock(&M_wait_th_mutex);
}
