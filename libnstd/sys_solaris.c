/* 
** Solaris Abstraction Layer (SAL)
**
** @file sys_linux.c
** 
** -----------------------------------------------------------------------------
** Enduro/X Middleware Platform for Distributed Transaction Processing
** Copyright (C) 2015, Mavimax, Ltd. All Rights Reserved.
** This software is released under one of the following licenses:
** GPL or Mavimax's license for commercial use.
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
** A commercial use license is available from Mavimax, Ltd
** contact@mavimax.com
** -----------------------------------------------------------------------------
*/

/*---------------------------Includes-----------------------------------*/
#include <stdio.h>
#include <stdlib.h>


#include <unistd.h>
#include <stdarg.h>
#include <ctype.h>
#include <memory.h>
#include <errno.h>
#include <signal.h>
#include <limits.h>
#include <pthread.h>
#include <string.h>
#include <dirent.h>
#include <stdarg.h>

#include <ndrstandard.h>
#include <ndebug.h>
#include <nstdutil.h>
#include <limits.h>

#include <sys_unix.h>

#include <utlist.h>


/*---------------------------Externs------------------------------------*/
/*---------------------------Macros-------------------------------------*/
#define SOL_RND_SLEEP	1000
/*---------------------------Enums--------------------------------------*/
/*---------------------------Typedefs-----------------------------------*/
/*---------------------------Globals------------------------------------*/
/*---------------------------Statics------------------------------------*/
/*---------------------------Prototypes---------------------------------*/

/**
 * Return the list of queues (build the list according to /tmp/.MQD files.
 * e.g ".MQPn00b,srv,admin,atmi.sv1,123,2229" translates as
 * "/n00b,srv,admin,atmi.sv1,123,2229")
 * the qpath must point to /tmp
 */
expublic string_list_t* ndrx_sys_mqueue_list_make(char *qpath, int *return_status)
{
    string_list_t* ret = NULL;
    struct dirent **namelist;
    int n;
    string_list_t* tmp;
    int len;
    
    *return_status = EXSUCCEED;
    
    n = scandir(qpath, &namelist, 0, alphasort);
    if (n < 0)
    {
        NDRX_LOG(log_error, "Failed to open queue directory: %s", 
                strerror(errno));
        goto exit_fail;
    }
    else 
    {
        while (n--)
        {
            if (0==strcmp(namelist[n]->d_name, ".") || 
                        0==strcmp(namelist[n]->d_name, "..") ||
                        0!=strncmp(namelist[n]->d_name, ".MQP", 4))
            {
                NDRX_FREE(namelist[n]);
                continue;
            }
            
            len = strlen(namelist[n]->d_name) -3 /*.MQP*/ + 1 /* EOS */;
            
            if (NULL==(tmp = NDRX_CALLOC(1, sizeof(string_list_t))))
            {
                NDRX_LOG(log_always, "alloc of string_list_t (%d) failed: %s", 
                        sizeof(string_list_t), strerror(errno));
                
                
                goto exit_fail;
            }
            
            if (NULL==(tmp->qname = NDRX_MALLOC(len)))
            {
                NDRX_LOG(log_always, "alloc of %d bytes failed: %s", 
                        len, strerror(errno));
                NDRX_FREE(tmp);
                goto exit_fail;
            }
            
            strcpy(tmp->qname, "/");
            strcat(tmp->qname, namelist[n]->d_name+4); /* strip off .MQP */
            
            /* Add to LL */
            LL_APPEND(ret, tmp);
            
            NDRX_FREE(namelist[n]);
        }
        NDRX_FREE(namelist);
    }
    
    return ret;
    
exit_fail:

    *return_status = EXFAIL;

    if (NULL!=ret)
    {
        ndrx_string_list_free(ret);
        ret = NULL;
    }

    return ret;   
}

expublic int     sol_mq_close(mqd_t __mqdes)
{
	int ret;
	
	while (EXSUCCEED!=(ret = mq_close(__mqdes)) && errno==EBUSY)
	{
/*		NDRX_LOG(log_warn, "%s: got EBUSY - restarting call...", __func__);*/
		usleep(SOL_RND_SLEEP);
	}
	return ret;	
}

expublic int     sol_mq_getattr(mqd_t __mqdes, struct mq_attr * __mqstat)
{
	int ret;
	
	while (EXSUCCEED!=(ret = mq_getattr(__mqdes, __mqstat)) && errno==EBUSY)
	{
	/*	NDRX_LOG(log_warn, "%s: got EBUSY - restarting call...", __func__); */
		usleep(SOL_RND_SLEEP);
	}
	return ret;
}

expublic int     sol_mq_notify(mqd_t __mqdes, const struct sigevent * __notification)
{
	int ret;
	
	while (EXSUCCEED!=(ret =mq_notify(__mqdes, __notification)) && errno==EBUSY)
	{
/*		NDRX_LOG(log_warn, "%s: got EBUSY - restarting call...", __func__); */
		usleep(SOL_RND_SLEEP);
	}
	return ret;
}

expublic mqd_t   sol_mq_open(const char * __name, int __oflag, ...)
{
	mqd_t  ret;
	va_list argp;
	
restart:
	va_start(argp, __oflag);
	ret = mq_open(__name, __oflag, argp);
	va_end(argp);
	
	if (EXFAIL==(int)ret && EBUSY==errno)
	{
/*		NDRX_LOG(log_warn, "%s: got EBUSY - restarting call...", __func__); */
		usleep(SOL_RND_SLEEP);
		goto restart;
	}

	return ret;	
}

expublic int sol_mq_receive(mqd_t __mqdes, char *__msg_ptr, size_t __msg_len, 
				unsigned int *__msg_prio)
{
	int ret;
	
	while (EXFAIL==(ret =mq_receive(__mqdes, __msg_ptr, __msg_len, __msg_prio)) && 
		errno==EBUSY)
	{
/*		NDRX_LOG(log_warn, "%s: got EBUSY - restarting call...", __func__); */
		usleep(SOL_RND_SLEEP);
	}
	return ret;
}

expublic int     sol_mq_send(mqd_t __mqdes, const char *__msg_ptr, size_t __msg_len,
                    unsigned int __msg_prio)
{
	int ret;
	
	while (EXSUCCEED!=(ret =mq_send(__mqdes, __msg_ptr, __msg_len, __msg_prio)) &&
		errno==EBUSY)
	{
/*		NDRX_LOG(log_warn, "%s: got EBUSY - restarting call...", __func__); */
		usleep(SOL_RND_SLEEP);
	}
	return ret;
}

expublic int     sol_mq_setattr(mqd_t __mqdes,
                       const struct mq_attr * __mqstat,
                       struct mq_attr * __omqstat)
{
	int ret;
	
	while (EXSUCCEED!=(ret =mq_setattr(__mqdes, __mqstat, __omqstat)) &&
		errno==EBUSY)
	{
/*		NDRX_LOG(log_warn, "%s: got EBUSY - restarting call...", __func__); */
		usleep(SOL_RND_SLEEP);
	}
	return ret;
}

expublic int     sol_mq_unlink(const char *__name)
{
	int ret;
	
	while (EXSUCCEED!=(ret =mq_unlink(__name)) &&
		errno==EBUSY)
	{
/*		NDRX_LOG(log_warn, "%s: got EBUSY - restarting call...", __func__); */
		usleep(SOL_RND_SLEEP);
	}
	return ret;
}	
expublic int sol_mq_timedsend(mqd_t __mqdes, const char *ptr, size_t len, 
			      unsigned int prio, const struct timespec *__abs_timeout)
{
	int ret;
	
	while (EXSUCCEED!=(ret =mq_timedsend(__mqdes, ptr, len, prio, __abs_timeout)) &&
		errno==EBUSY)
	{
/*		NDRX_LOG(log_warn, "%s: got EBUSY - restarting call...", __func__); */
		usleep(SOL_RND_SLEEP);
	}
	return ret;
}

expublic  int sol_mq_timedreceive(mqd_t mqdes, char *restrict msg_ptr,
     size_t  msg_len,  unsigned  *restrict msg_prio, const struct
     timespec *restrict abs_timeout)
{
	int ret;
	
	while (EXFAIL==(ret =mq_timedsend(mqdes, msg_ptr, msg_len, msg_prio, abs_timeout)) &&
		errno==EBUSY)
	{
/*		NDRX_LOG(log_warn, "%s: got EBUSY - restarting call...", __func__);*/
		usleep(SOL_RND_SLEEP);
	}
	return ret;
}

