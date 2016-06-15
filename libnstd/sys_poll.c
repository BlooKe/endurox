/* 
** Poll Abstraction Layer (PAL)
** NOTE: Thread shall not modify the ndrx_epoll sets. That must be managed from
** one thread only
** NOTE: Only one POLL set actually is supported. This is due to
** Notificatio thread locking while we are not polling (to void mqds in pipe)
** which we have already processed.
** 
** @file sys_poll.c
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

/*---------------------------Includes-----------------------------------*/

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <unistd.h>
#include <stdarg.h>
#include <ctype.h>
#include <memory.h>
#include <errno.h>
#include <signal.h>
#include <limits.h>
#include <pthread.h>
#include <string.h>
#include <poll.h>
#include <fcntl.h>

#include <sys/time.h>                   /* purely for dbg_timer()       */
#include <sys/stat.h>
#include <ndrstandard.h>
#include <ndebug.h>
#include <nstdutil.h>
#include <limits.h>
#include <sys_mqueue.h>
#include <sys_unix.h>

#include <uthash.h>

/*---------------------------Externs------------------------------------*/
/*---------------------------Macros-------------------------------------*/

/* Use signalled polling */
#define EX_POLL_SIGNALLED
/*
 * Special ops for message queue...
 */
#define HASH_FIND_MQD(head,findptr,out)                                          \
    HASH_FIND(hh,head,findptr,sizeof(mqd_t),out)

#define HASH_ADD_MQD(head,ptrfield,add)                                          \
    HASH_ADD(hh,head,ptrfield,sizeof(mqd_t),add)


#define EX_POLL_SETS_MAX            1024


#define EX_EPOLL_API_ENTRY      {M_last_err = 0; M_last_err_msg[0] = EOS;}

/* The index of the "read" end of the pipe */
#define READ 0

/* The index of the "write" end of the pipe */
#define WRITE 1


#define PIPE_POLL_IDX           0   /* where to pipe sits in poll */

#define ERROR_BUFFER            1024


#define NOTIFY_SIG SIGUSR2

/*---------------------------Enums--------------------------------------*/
/*---------------------------Typedefs-----------------------------------*/

/*
 * Hash list of File descriptors we monitor..
 */
struct ndrx_epoll_fds
{
    int fd;
    UT_hash_handle hh;         /* makes this structure hashable */
};
typedef struct ndrx_epoll_fds ndrx_epoll_fds_t;

/*
 * Hash list of File descriptors we monitor..
 */
struct ndrx_epoll_mqds
{
    mqd_t mqd;
    struct sigevent sev;        /* event flags */
    UT_hash_handle hh;         /* makes this structure hashable */
};
typedef struct ndrx_epoll_mqds ndrx_epoll_mqds_t;

/*
 * Our internal 'epoll' set
 */
struct ndrx_epoll_set
{
    int fd;
    
    ndrx_epoll_fds_t *fds; /* hash of file descriptors for monitoring */
    ndrx_epoll_mqds_t *mqds; /* hash of message queues for monitoring */
    
    
    int nrfds; /* number of FDs in poll */
    struct pollfd *fdtab; /* poll() structure, pre-allocated*/
    
    int nrfmqds; /* Number of queues polled */
    
    
    int wakeup_pipe[2]; /* wakeup pipe from notification thread */
    
    UT_hash_handle hh;         /* makes this structure hashable */
};
typedef struct ndrx_epoll_set ndrx_epoll_set_t;

typedef struct ndrx_pipe_mqd_hash ndrx_pipe_mqd_hash_t;

private ndrx_epoll_set_t *M_psets = NULL; /* poll sets  */
private ndrx_pipe_mqd_hash_t *M_pipe_h = NULL; /* pipe hash */


MUTEX_LOCKDECL(M_psets_lock);
/*---------------------------Globals------------------------------------*/
/*---------------------------Statics------------------------------------*/


/* 
 * - So we need a structure for holding epoll set
 * - We need a hash list for FDs
 * - We need a hash list for mqd_t
 * - Once create is called, we prepare unnamed pipe2() for notify callback.
 */


private int __thread M_last_err = 0;
private char __thread M_last_err_msg[1024];  /* Last error message */


private pthread_t M_signal_thread; /* Signalled thread */
private int M_signal_first = TRUE; /* is first init for signal thread */

/*---------------------------Prototypes---------------------------------*/
private ndrx_epoll_mqds_t* mqd_find(ndrx_epoll_set_t *pset, mqd_t mqd);
private int signal_install_notifications_all(ndrx_epoll_set_t *s);


/**
 * Return the compiled poll mode
 * @return 
 */
public char * ndrx_epoll_mode(void)
{
    static char *mode = "poll";
    
    return mode;
}

/**
 * Handle event
 * @return 
 */
private int signal_handle_event(void)
{
    int ret = SUCCEED;
    
    ndrx_epoll_set_t *s, *stmp;
    ndrx_epoll_mqds_t* m, *mtmp;
    
    /* Reregister for message notification */

    MUTEX_LOCK_V(M_psets_lock);

    HASH_ITER(hh, M_psets, s, stmp)
    {
        HASH_ITER(hh, s->mqds, m, mtmp)
        {
            struct mq_attr att;

            /* read the attributes of the Q */
            if (SUCCEED!= ndrx_mq_getattr(m->mqd, &att))
            {
                NDRX_LOG(log_warn, "Failed to get attribs of Q: %d (%s)",  
                        m->mqd, strerror(errno));
                MUTEX_UNLOCK_V(M_psets_lock);
            }

            if (att.mq_curmsgs > 0)
            {
                if (FAIL==write (s->wakeup_pipe[WRITE], (char *)&m->mqd, 
                        sizeof(m->mqd)))
                {
                    NDRX_LOG(log_error, "Error ! write fail: %s", strerror(errno));
                }

            }
        }

        /* Install all notifs */
        if (SUCCEED!=signal_install_notifications_all(s))
        {
            NDRX_LOG(log_warn, "Failed to install notifs for set: %d", s->fd);
        }
    }

    MUTEX_UNLOCK_V(M_psets_lock);
        
out:
        return ret;
}

/**
 * Process signal thread
 * TODO: We need a hash of events in PIPE! To avoid duplicate events to pipe...
 * @return 
 */
private void * signal_process(void *arg)
{
    sigset_t blockMask;
    char *fn = "signal_process";
    int ret = SUCCEED;
    int sig;
    

    NDRX_LOG(log_debug, "%s - enter", fn);
    
    /* Block the notification signal (do not need it here...) */
    
    sigemptyset(&blockMask);
    sigaddset(&blockMask, NOTIFY_SIG);
    
    for (;;)
    {
        
        NDRX_LOG(log_debug, "%s - before sigwait()", fn);
        if (SUCCEED!=sigwait(&blockMask, &sig))         /* Wait for notification signal */
        {
            NDRX_LOG(log_warn, "sigwait failed:(%s)", strerror(errno));

        }
        
        NDRX_LOG(log_debug, "%s - after sigwait()", fn);
        
        /* check all queues and pipe down the event... */
        signal_handle_event();
    }
    
out:
    return NULL;
}
    
/**
 * Install notifications for all Qs
 */
private int signal_install_notifications_all(ndrx_epoll_set_t *s)
{
    int ret = SUCCEED;
    ndrx_epoll_mqds_t* m, *mtmp;
    
    HASH_ITER(hh, s->mqds, m, mtmp)
    {
        if (FAIL==ndrx_mq_notify(m->mqd, &m->sev))
        {
            int err = errno;
	    if (EBUSY!=err)
	    {
                NDRX_LOG(log_warn, "ndrx_mq_notify failed: %d (%s) - nothing to do", 
                    m->mqd, strerror(err));
	    }
        }
    }
out:
    return ret;
}
/**
 * Initialize polling lib
 * not thread safe.
 * @return
 */
public void ndrx_epoll_sys_init(void)
{
    sigset_t blockMask;
    pthread_attr_t pthread_custom_attr;
    pthread_attr_t pthread_custom_attr_dog;
    char *fn = "ndrx_epoll_sys_init";

    NDRX_LOG(log_debug, "%s - enter", fn);
    if (!M_signal_first)
    {
	NDRX_LOG(log_warn, "Already init done for poll()");
	return;
    }
    
    /* Block the notification signal (do not need it here...) */
    
    sigemptyset(&blockMask);
    sigaddset(&blockMask, NOTIFY_SIG);
    
    if (pthread_sigmask(SIG_BLOCK, &blockMask, NULL) == -1)
    {
        NDRX_LOG(log_always, "%s: sigprocmask failed: %s", fn, strerror(errno));
    }
    
    
    pthread_attr_init(&pthread_custom_attr);
    pthread_attr_init(&pthread_custom_attr_dog);
    
    /* set some small stacks size, 1M should be fine! */
    pthread_attr_setstacksize(&pthread_custom_attr, 2048*1024);
    pthread_create(&M_signal_thread, &pthread_custom_attr, 
            signal_process, NULL);
    M_signal_first = FALSE;
    
}

/**
 * Un-initialize polling lib
 * @return
 */
public void ndrx_epoll_sys_uninit(void)
{
    char *fn = "ndrx_epoll_sys_uninit";

    NDRX_LOG(log_debug, "%s - enter", fn);
    

    NDRX_LOG(log_debug, "About to cancel signal thread");
    
    /* TODO: have a counter for number of sets, so that we can do 
     * un-init...
     */
    if (SUCCEED!=pthread_cancel(M_signal_thread))
    {
        NDRX_LOG(log_error, "Failed to kill poll signal thread: %s", strerror(errno));
    }
    else
    {
        void * res = SUCCEED;
        if (SUCCEED!=pthread_join(M_signal_thread, &res))
        {
            NDRX_LOG(log_error, "Failed to join pthread_join() signal thread: %s", 
                    strerror(errno));
        }

        if (res == PTHREAD_CANCELED)
        {
            NDRX_LOG(log_info, "Signal thread canceled ok!")
        }
        else
        {
            NDRX_LOG(log_info, "Signal thread failed to cancel "
                    "(should not happen!!)");
        }
    }
    
    NDRX_LOG(log_debug, "finished ok");
}

/**
 * Message queue notification function
 * We got to iterate overall sets to find right one for the queue...
 * TODO: we need to lock the ndrx_epoll sets as we do the lookup in thread.
 * @param sv
 */
private void ndrx_ndrx_mq_notify_func(union sigval sv)
{
    mqd_t mqdes = *((mqd_t *) sv.sival_ptr);

    ndrx_epoll_set_t *s, *stmp;
    ndrx_epoll_mqds_t* mqd_h;

    NDRX_LOG(log_debug, "ndrx_ndrx_mq_notify_func() called mqd %d\n", mqdes);

    MUTEX_LOCK_V(M_psets_lock);

    HASH_ITER(hh, M_psets, s, stmp)
    {
        if (NULL!=(mqd_h =  mqd_find(s, mqdes)))
        {
            break;
        }
    }

    if (NULL==mqd_h)
    {
        NDRX_LOG(log_error, "ndrx_epoll set not found for mqd %d", mqdes);
        goto out;
    }

    NDRX_LOG(log_debug, "ndrx_epoll set found piping event...");

    if (-1==write (s->wakeup_pipe[WRITE], (char *)&mqdes, sizeof(mqdes)))
    {
        NDRX_LOG(log_error, "Error ! write fail: %s", strerror(errno));
    }


    /* Install handler back */

    if (FAIL==ndrx_mq_notify(mqdes, &mqd_h->sev))
    {
        NDRX_LOG(log_error, "Failed to register notification for mqd %d (%s)!!!", 
                mqdes, strerror(errno));
    }

out:
    MUTEX_UNLOCK_V(M_psets_lock);
}


/**
 * We basically will use unix error codes
 */
private void ndrx_epoll_set_err(int error_code, const char *fmt, ...)
{
    char msg[ERROR_BUFFER+1] = {EOS};
    va_list ap;

    va_start(ap, fmt);
    (void) vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    strcpy(M_last_err_msg, msg);
    M_last_err = error_code;

    NDRX_LOG(log_warn, "ndrx_epoll_set_err: %d (%s) (%s)",
                    error_code, strerror(M_last_err), M_last_err_msg);
    
}

/**
 * Find FD in hash
 */
private ndrx_epoll_fds_t* fd_find(ndrx_epoll_set_t *pset, int fd)
{
    ndrx_epoll_fds_t*ret = NULL;
    
    HASH_FIND_INT( pset->fds, &fd, ret);
    
    return ret;
}

/**
 * FInd q descriptor in hash
 */
private ndrx_epoll_mqds_t* mqd_find(ndrx_epoll_set_t *pset, mqd_t mqd)
{
    ndrx_epoll_mqds_t*ret = NULL;
    
    HASH_FIND_MQD( pset->mqds, &mqd, ret);
    
    return ret;
}

/**
 * Find polling set
 */
private ndrx_epoll_set_t* pset_find(int epfd)
{
    ndrx_epoll_set_t *ret = NULL;
    
    HASH_FIND_INT( M_psets, &epfd, ret);
    
    return ret;
}

/**
 * Wrapper for epoll_ctl, for standard file descriptors
 * @param epfd
 * @param op
 * @param fd
 * @param event
 * @return 
 */
public int ndrx_epoll_ctl(int epfd, int op, int fd, struct ndrx_epoll_event *event)
{
    int ret = SUCCEED;
    ndrx_epoll_set_t* set = NULL;
    ndrx_epoll_fds_t * tmp = NULL;
    char *fn = "ndrx_epoll_ctl";
    
    
    EX_EPOLL_API_ENTRY;
    
    MUTEX_LOCK_V(M_psets_lock);
    
    if (NULL==(set = pset_find(epfd)))
    {
        NDRX_LOG(log_error, "ndrx_epoll set %d not found", epfd);
        ndrx_epoll_set_err(ENOSYS, "ndrx_epoll set %d not found", epfd);
        FAIL_OUT(ret);
    }
    
    if (EX_EPOLL_CTL_ADD == op)
    {
        NDRX_LOG(log_info, "%s: Add operation on ndrx_epoll set %d, fd %d", fn, epfd, fd);
        
        /* test & add to FD hash */
        if (NULL!=fd_find(set, fd))
        {

            ndrx_epoll_set_err(EINVAL, "fd %d already exists in ndrx_epoll set (epfd %d)", 
                    fd, set->fd);
            NDRX_LOG(log_error, "fd %d already exists in ndrx_epoll set (epfd %d)", 
                 fd, set->fd);
            FAIL_OUT(ret);
        }
        
        if (NULL==(tmp = calloc(1, sizeof(*tmp))))
        {
            ndrx_epoll_set_err(errno, "Failed to alloc FD hash entry");
            NDRX_LOG(log_error, "Failed to alloc FD hash entry");
            FAIL_OUT(ret);
        }
        
        tmp->fd = fd;
        HASH_ADD_INT(set->fds, fd, tmp);
        
        /* resize/realloc events list, add fd */
        set->nrfds++;
        
        NDRX_LOG(log_info, "set nrfds incremented to %d", set->nrfds);
        
        if (NULL==(set->fdtab=realloc(set->fdtab, sizeof(struct pollfd)*set->nrfds)))
        {
            ndrx_epoll_set_err(errno, "Failed to realloc %d/%d", 
                    set->nrfds, sizeof(struct pollfd)*set->nrfds);
            NDRX_LOG(log_error, "Failed to realloc %d/%d", 
                    set->nrfds, sizeof(struct pollfd)*set->nrfds);
            FAIL_OUT(ret);
        }
        
        set->fdtab[set->nrfds-1].fd = fd;
        set->fdtab[set->nrfds-1].events = event->events;
    }
    else if (EX_EPOLL_CTL_DEL == op)
    {
        int i;
        NDRX_LOG(log_info, "%s: Delete operation on ndrx_epoll set %d, fd %d", fn, epfd, fd);
        
        /* test & add to FD hash */
        if (NULL==(tmp=fd_find(set, fd)))
        {
            ndrx_epoll_set_err(EINVAL, "fd %d not found in ndrx_epoll set (epfd %d)", 
                    fd, set->fd);
            NDRX_LOG(log_error, "fd %d not found in ndrx_epoll set (epfd %d)", 
                    fd, set->fd);
            FAIL_OUT(ret);
        }
        
        /* Remove fd from set->fdtab & from hash */
        
        HASH_DEL(set->fds, tmp);
        free((char *)tmp);
        
        for (i = 0; i < set->nrfds; i++)
        {
            if (set->fdtab[i].fd == fd)
            {
                /* kill the element */
                if (i!=set->nrfds-1 && set->nrfds>1)
                {
                    /* It is not last element, thus we can move something... */
                    memmove(set->fdtab+i, set->fdtab+i+1, set->nrfds-i-1);
                }
                
                set->nrfds--;
                
                NDRX_LOG(log_info, "set nrfds decremented to %d", set->nrfds);
                
                if (0==set->nrfds)
                {
                    NDRX_LOG(log_warn, "set->nrfds == 0, => free");
                    free((char *)set->fdtab);
                }
                else if (NULL==(set->fdtab=realloc(set->fdtab, sizeof(struct pollfd)*set->nrfds)))
                {
                    ndrx_epoll_set_err(errno, "Failed to realloc %d/%d", 
                            set->nrfds, sizeof(struct pollfd)*set->nrfds);
                    
                    NDRX_LOG(log_error, "Failed to realloc %d/%d", 
                            set->nrfds, sizeof(struct pollfd)*set->nrfds);
                    
                    FAIL_OUT(ret);
                }
            }
        }
    }
    else
    {
        ndrx_epoll_set_err(EINVAL, "Invalid operation %d", op);
        NDRX_LOG(log_error, "Invalid operation %d", op);
        
        FAIL_OUT(ret);
    }

out:

    MUTEX_UNLOCK_V(M_psets_lock);

    NDRX_LOG(log_info, "%s return %d", fn, ret);

    return ret;
}

/**
 * epoll_ctl for Posix queue descriptors
 * @param epfd
 * @param op
 * @param fd
 * @param event
 * @return 
 */
public int ndrx_epoll_ctl_mq(int epfd, int op, mqd_t mqd, struct ndrx_epoll_event *event)
{
    int ret = SUCCEED;
    ndrx_epoll_set_t* set = NULL;
    ndrx_epoll_mqds_t * tmp = NULL;
    char *fn = "ndrx_epoll_ctl_mq";
    
    EX_EPOLL_API_ENTRY;
    
    MUTEX_LOCK_V(M_psets_lock);
    
    if (NULL==(set = pset_find(epfd)))
    {
        ndrx_epoll_set_err(ENOSYS, "ndrx_epoll set %d not found", epfd);
        NDRX_LOG(log_error, "ndrx_epoll set %d not found", epfd);
        
        FAIL_OUT(ret);
    }
    
    if (EX_EPOLL_CTL_ADD == op)
    {
        NDRX_LOG(log_info, "%s: Add operation on ndrx_epoll set %d, fd %d", fn, epfd, mqd);
        
        /* test & add to FD hash */
        if (NULL!=mqd_find(set, mqd))
        {
            ndrx_epoll_set_err(EINVAL, "fd %d already exists in ndrx_epoll set (epfd %d)", 
                    mqd, set->fd);
            NDRX_LOG(log_error, "fd %d already exists in ndrx_epoll set (epfd %d)", 
                    mqd, set->fd);
            FAIL_OUT(ret);
        }
        
        if (NULL==(tmp = calloc(1, sizeof(*tmp))))
        {
            ndrx_epoll_set_err(errno, "Failed to alloc FD hash entry");
            NDRX_LOG(log_error, "Failed to alloc FD hash entry");
            
            FAIL_OUT(ret);
        }
        
        tmp->mqd = mqd;
        HASH_ADD_MQD(set->mqds, mqd, tmp);
        
        /* resize/realloc events list, add fd */
        set->nrfmqds++;
        
        NDRX_LOG(log_info, "set nrfmqds incremented to %d", set->nrfmqds);
        
        /* Register notification for mqd */
        
        /* notification block */
#ifdef EX_POLL_SIGNALLED
        tmp->sev.sigev_notify = SIGEV_SIGNAL;
        tmp->sev.sigev_signo = NOTIFY_SIG;
#else
        
        tmp->sev.sigev_notify = SIGEV_THREAD;
        tmp->sev.sigev_notify_function = ndrx_ndrx_mq_notify_func;
        tmp->sev.sigev_notify_attributes = NULL;
        tmp->sev.sigev_value.sival_ptr = &tmp->mqd;
        
        if (FAIL==ndrx_mq_notify(mqd, &tmp->sev))
        {
            ndrx_epoll_set_err(errno, "Failed to register notification for mqd %d", mqd);
            FAIL_OUT(ret);
        }
        
#endif
        
        
        
    }
    else if (EX_EPOLL_CTL_DEL == op)
    {
        NDRX_LOG(log_info, "%s: Delete operation on ndrx_epoll set %d, fd %d", fn, epfd, mqd);
        
        /* test & add to FD hash */
        if (NULL==(tmp=mqd_find(set, mqd)))
        {
            ndrx_epoll_set_err(EINVAL, "fd %d not found in ndrx_epoll set (epfd %d)", 
                    mqd, set->fd);
            
            NDRX_LOG(log_error, "fd %d not found in ndrx_epoll set (epfd %d)", 
                    mqd, set->fd);
            
            FAIL_OUT(ret);
        }
        
        /* Remove fd from set->fdtab & from hash */
        HASH_DEL(set->mqds, tmp);
        free((char *)tmp);
    }
    else
    {
        ndrx_epoll_set_err(EINVAL, "Invalid operation %d", op);
        
        NDRX_LOG(log_error, "Invalid operation %d", op);
        
        FAIL_OUT(ret);
    }

out:

    MUTEX_UNLOCK_V(M_psets_lock);

    NDRX_LOG(log_info, "%s return %d", fn, ret);

    return ret;
}

/**
 * Wrapper for epoll_create
 * @param size
 * @return 
 */
public int ndrx_epoll_create(int size)
{
    int ret = SUCCEED;
    int i = 1;
    ndrx_epoll_set_t *set;
    
    EX_EPOLL_API_ENTRY;
    
    while (NULL!=(set=pset_find(i)) && i < EX_POLL_SETS_MAX)
    {
        i++;
    }
    
    /* we must have free set */
    if (NULL!=set)
    {
        ndrx_epoll_set_err(EMFILE, "Max ndrx_epoll_sets_reached");
        NDRX_LOG(log_error, "Max ndrx_epoll_sets_reached");
                
        
        set = NULL;
        FAIL_OUT(ret);
    }
    
    NDRX_LOG(log_info, "Creating ndrx_epoll set: %d", i);
    
    if (NULL==(set = (ndrx_epoll_set_t *)calloc(1, sizeof(*set))))
    {
        ndrx_epoll_set_err(errno, "Failed to alloc: %d bytes", sizeof(*set));
        
        NDRX_LOG(log_error, "Failed to alloc: %d bytes", sizeof(*set));

        FAIL_OUT(ret);
    }
    
	/* O_NONBLOCK */
    if (pipe(set->wakeup_pipe) == -1)
    {
        ndrx_epoll_set_err(errno, "pipe failed");
        NDRX_LOG(log_error, "pipe failed");
        FAIL_OUT(ret);
    }

    if (FAIL==fcntl(set->wakeup_pipe[READ], F_SETFL, 
		fcntl(set->wakeup_pipe[READ], F_GETFL) | O_NONBLOCK))
    {
        ndrx_epoll_set_err(errno, "fcntl READ pipe set O_NONBLOCK failed");
        NDRX_LOG(log_error, "fcntl READ pipe set O_NONBLOCK failed");
        FAIL_OUT(ret);
    }

    if (FAIL==fcntl(set->wakeup_pipe[WRITE], F_SETFL, 
		fcntl(set->wakeup_pipe[WRITE], F_GETFL) | O_NONBLOCK))
    {
        ndrx_epoll_set_err(errno, "fcntl WRITE pipe set O_NONBLOCK failed");
        NDRX_LOG(log_error, "fcntl WRITE pipe set O_NONBLOCK failed");
        FAIL_OUT(ret);
    }
    
    set->nrfds = 1; /* initially only pipe wait */
    set->fd = i; /* assign the FD num */
    if (NULL==(set->fdtab = calloc(set->nrfds, sizeof(struct pollfd))))
    {
        ndrx_epoll_set_err(errno, "calloc for pollfd failed");
        NDRX_LOG(log_error, "calloc for pollfd failed");
        FAIL_OUT(ret);
    }

    /* So wait for events here in the pip form Q thread */
    set->fdtab[PIPE_POLL_IDX].fd = set->wakeup_pipe[READ];
    set->fdtab[PIPE_POLL_IDX].events = POLLIN;
    
    /* add finally to hash */
    HASH_ADD_INT(M_psets, fd, set);
    
    NDRX_LOG(log_info, "ndrx_epoll_create succeed, fd=%d", i);
    
out:

    if (SUCCEED!=ret)
    {
        if (NULL!=set && set->wakeup_pipe[READ])
        {
            close(set->wakeup_pipe[READ]);
        }
        
        if (NULL!=set && set->wakeup_pipe[WRITE])
        {
            close(set->wakeup_pipe[WRITE]);
        }
            
        if (NULL!=set)
        {
            free((char *)set);
        }
        
        return FAIL;
        
    }

    return i;
}

/**
 * Close the epoll set
 */
public int ndrx_epoll_close(int epfd)
{
    int ret = SUCCEED;
    ndrx_epoll_set_t* set = NULL;
    
    ndrx_epoll_fds_t* f, *ftmp;
    ndrx_epoll_mqds_t* m, *mtmp;
    
    NDRX_LOG(log_debug, "ndrx_epoll_close(%d) enter", epfd);
            
    MUTEX_LOCK_V(M_psets_lock);
    
    NDRX_LOG(log_debug, "ndrx_epoll_close(%d) enter (after lock", epfd);
    
    if (NULL==(set = pset_find(epfd)))
    {
        MUTEX_UNLOCK_V(M_psets_lock); /*  <<< release the lock */
        
        ndrx_epoll_set_err(EINVAL, "ndrx_epoll set %d not found", epfd);
        NDRX_LOG(log_error, "ndrx_epoll set %d not found", epfd);
        
        
        FAIL_OUT(ret);
    }
    MUTEX_UNLOCK_V(M_psets_lock);
    
    if (set->wakeup_pipe[READ])
    {
        close(set->wakeup_pipe[READ]);
    }
    
    if (set->wakeup_pipe[WRITE])
    {
        close(set->wakeup_pipe[WRITE]);
    }
    
    /* Kill file descriptor hash */
    HASH_ITER(hh, set->fds, f, ftmp)
    {
        
        ndrx_epoll_ctl(set->fd, EX_EPOLL_CTL_DEL, f->fd, NULL);
    }
    
    /* Kill message queue descriptor hash */
    HASH_ITER(hh, set->mqds, m, mtmp)
    {
        ndrx_epoll_ctl_mq(set->fd, EX_EPOLL_CTL_DEL, m->mqd, NULL);
    }
    
    MUTEX_LOCK_V(M_psets_lock);
    free(set);
    HASH_DEL(M_psets, set);
    MUTEX_UNLOCK_V(M_psets_lock);
    
    
out:
    return FAIL;
}

/**
 * Wrapper for epoll_wait
 * We do a classical poll here, but before poll, we scan all the queues,
 * so that they are empty before poll() call.
 * if any queue we have something, we return it in evens. Currenly one event per call.
 * 
 * TODO: clear revents before poll..
 * 
 * @param epfd
 * @param events
 * @param maxevents
 * @param timeout
 * @return 
 */
public int ndrx_epoll_wait(int epfd, struct ndrx_epoll_event *events, int maxevents, int timeout)
{
    int ret = SUCCEED;
    int numevents = 0;
    ndrx_epoll_set_t* set = NULL;
    char *fn = "ndrx_epoll_wait";
    int i;
    int retpoll;
    int try;
    ndrx_epoll_mqds_t* m, *mtmp;
    
    EX_EPOLL_API_ENTRY;
    
    /*  !!!! LOCKED AREA !!!! */
    MUTEX_LOCK_V(M_psets_lock);
    
    if (NULL==(set = pset_find(epfd)))
    {
        MUTEX_UNLOCK_V(M_psets_lock); /*  <<< release the lock */
        
        ndrx_epoll_set_err(EINVAL, "ndrx_epoll set %d not found", epfd);
        
        NDRX_LOG(log_error, "ndrx_epoll set %d not found", epfd);
        
        FAIL_OUT(ret);
    }
    
    MUTEX_UNLOCK_V(M_psets_lock); /*  <<< release the lock */
    
    /*  !!!! LOCKED AREA, end !!!! */
    
    /* Reset received events... */
    
    /* if there is data in Q, we will never get a notification!!! 
     * Thus we need somehow ensure that queues are empty before doing poll...
     * If q if flushed, then we will get piped message.
     * Even if we catch message here, bellow will return something, 
     * but svqdispatch will get eagain.. no problem
     */
    NDRX_LOG(log_debug, "Checking early messages...");
    
    for (try = 0; try<2; try++)
    {
        MUTEX_LOCK_V(M_psets_lock);
        HASH_ITER(hh, set->mqds, m, mtmp)
        {
            struct mq_attr att;

            /* read the attributes of the Q */
            if (SUCCEED!= ndrx_mq_getattr(m->mqd, &att))
            {
                ndrx_epoll_set_err(errno, "Failed to get attribs of Q: %d",  m->mqd);
                NDRX_LOG(log_warn, "Failed to get attribs of Q: %d",  m->mqd);
                FAIL_OUT(ret);
            }

            if (att.mq_curmsgs > 0)
            {
                if (numevents < maxevents )
                {
                    numevents++;

                    NDRX_LOG(log_info, "Got mqdes %d for pipe", m->mqd);

                    events[numevents-1].data.mqd = m->mqd;
                    /* take from pipe */
                    events[numevents-1].events = set->fdtab[PIPE_POLL_IDX].revents;
                    events[numevents-1].is_mqd = TRUE;
                }
                else
                {
                    break;
                }
            }
        }
        MUTEX_UNLOCK_V(M_psets_lock);

        if (numevents)
        {
            NDRX_LOG(log_info, "Found %d events before polling "
                    "(messages in Q) returning...", numevents);
            goto out;
        }
        
#ifdef EX_POLL_SIGNALLED

        if (0==try)
        {
            /* Only at try 0, we ensure that all notifs are set.
             * This is due to fact that we might face following scenario:
             * 1) we download all messages
             * 1.1) ERROR: some message comes in and it is not notified...
             * 2) We set notif handler
             * Shut to avoid this:
             * 1) download all messages
             * 2) set notif handler
             * 3) FIX: if any message comes in it will be downloaded by next
             * 4) again download all messages
             * 
             * If any new message comes in it will be handled by signal thread
             * + thread will ensure that notifications are set.
             */
            MUTEX_LOCK_V(M_psets_lock);

            /* put the handlers back for all queues..., for signalled tech 
             * Put them anyway.. (if we slipped something...)
             */

            if (SUCCEED!=signal_install_notifications_all(set))
            {
                ndrx_epoll_set_err(EOS, "Failed to install notifs!");
                NDRX_LOG(log_error, "Failed to install notifs!");
                MUTEX_UNLOCK_V(M_psets_lock);
                FAIL_OUT(ret);
            }

            MUTEX_UNLOCK_V(M_psets_lock);
        }
#endif
    }
    
    
    
    for (i=0; i<set->nrfds; i++)
    {
        set->fdtab[i].revents = 0;
        NDRX_LOG(log_debug, "poll i=%d, fd=%d, events=%d", 
                i, set->fdtab[i].fd, set->fdtab[i].events);
    }

    NDRX_LOG(log_debug, "%s: epfd=%d, events=%p, maxevents=%d, timeout=%d - about to poll(nrfds=%d)",
                        fn, epfd, events, maxevents, timeout, set->nrfds);
    
    
    retpoll = poll( set->fdtab, set->nrfds, timeout);
    
    for (i=0; i<set->nrfds; i++)
    {
        if (set->fdtab[i].revents)
        {
            NDRX_LOG(log_debug, "%d revents=%d", i, set->fdtab[i].revents);
            if (PIPE_POLL_IDX==i)
            {
                /* Receive the p */
                int err;
                mqd_t mqdes = 0;
                while (numevents < maxevents && 
                        FAIL!=(ret=read(set->wakeup_pipe[READ], (char *)&mqdes, sizeof(mqdes))))
                {
		    struct mq_attr att;
	   /* read the attributes of the Q */
	  /* we get some strange lock-ups on solaris, thus ignore empty q wakeups... */
            if (SUCCEED!= ndrx_mq_getattr(mqdes, &att))
            {
                /*ndrx_epoll_set_err(errno, "Failed to get attribs of Q: %d",  m->mqd);*/
                NDRX_LOG(log_warn, "Failed to get attribs of Q: %d",  m->mqd);
          /*      FAIL_OUT(ret);*/
            }
		else if (att.mq_curmsgs > 0)
		{


                    numevents++;
                    
                    NDRX_LOG(log_info, "Got mqdes %d for pipe", mqdes);
                    
                    events[numevents-1].data.mqd = mqdes;
                    events[numevents-1].events = set->fdtab[i].revents;
                    events[numevents-1].is_mqd = TRUE;
		}
                    
                }
                
                err = errno;
                
                if (FAIL==ret)
                {
                    if (EAGAIN == err || EWOULDBLOCK == err)
                    {
                        ret = SUCCEED;
                    }
                    else
                    {
                        ndrx_epoll_set_err(err, "Failed to read notify unnamed pipe!");
                        NDRX_LOG(log_error, "Failed to read notify unnamed pipe!");
                        FAIL_OUT(ret);
                    }
                }
                else if (ret!=sizeof(mqdes))
                {
                    ndrx_epoll_set_err(EOS, "Error ! Expected piped read size %d but got %d!", 
                            ret, sizeof(mqdes));
                    NDRX_LOG(log_error, "Error ! Expected piped read size %d but got %d!", 
                            ret, sizeof(mqdes));
                    FAIL_OUT(ret);
                }
                else
                {
                    ret = SUCCEED; /* ok */
                }
                
            }
            else if (numevents < maxevents)
            {
                numevents++;
                
                NDRX_LOG(log_debug, "return normal fd");
                
                events[numevents-1].data.fd =set->fdtab[i].fd;
                events[numevents-1].events = set->fdtab[i].revents;
                events[numevents-1].is_mqd = FALSE;
            }
            else
            {
                NDRX_LOG(log_warn, "Return struct full (reached %d)", maxevents);
            }
        }
    }
    
out:

    NDRX_LOG(log_info, "%s ret=%d numevents=%d", fn, ret, numevents);

    if (SUCCEED==ret)
    {
        return numevents;
    }
    else
    {
        return FAIL;
    }
}

/**
 * Return errno for ndrx_poll() operation
 * @return 
 */
public int ndrx_epoll_errno(void)
{
    return M_last_err;
}

/**
 * Wrapper for strerror
 * @param err
 * @return 
 */
public char * ndrx_poll_strerror(int err)
{
    static __thread char ret[ERROR_BUFFER];
    
    sprintf(ret, "%s (last error: %s)", strerror(err), M_last_err_msg);
    
    return ret;
}

