/* 
** Unix Abstraction Layer
**
** @file sys_unix.h
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
#ifndef SYS_UNIX_H
#define	SYS_UNIX_H

#ifdef	__cplusplus
extern "C" {
#endif
/*---------------------------Includes-----------------------------------*/
#include <config.h>
#include <stdint.h>
#include <ubf.h>
#include <atmi.h>
#include <sys_mqueue.h>
#include <ndrstandard.h>

#ifdef EX_USE_EPOLL
#include <sys/epoll.h>
#else
#include <poll.h>
#endif


/*---------------------------Externs------------------------------------*/
/*---------------------------Macros-------------------------------------*/

#ifdef EX_USE_EPOLL

#define EX_EPOLL_CTL_ADD        EPOLL_CTL_ADD
#define EX_EPOLL_CTL_DEL        EPOLL_CTL_DEL
    
    
#define EX_EPOLL_FLAGS          (EPOLLIN | EPOLLERR | EPOLLEXCLUSIVE)

#else

#define EX_EPOLL_CTL_ADD        1
#define EX_EPOLL_CTL_DEL        2
    
#define EX_EPOLL_FLAGS          POLLIN

#endif
/******************************************************************************/
/***************************** PROGNAME ***************************************/
/******************************************************************************/
/**
 * Configure the program name.
 * On same platforms/gcc versions we have __progname external
 * On other systems we will do lookup by ps function
 */    
#ifdef HAVE_PROGNAME
    
extern const char * __progname;

#define EX_PROGNAME __progname


#elif EX_OS_AIX

#define EX_PROGNAME ndrx_sys_get_proc_name_getprocs()

#else

/* the worst option: to use ps, it generates sigchild...
 * and for libraries that is not good.
 */
#define EX_PROGNAME ndrx_sys_get_proc_name_by_ps()

#endif

/******************************************************************************/
/***************************** PROCESS TESTING ********************************/
/******************************************************************************/
/**
 * The best option is linux procfs, we generally fallback to kill -0
 * but better would be to use OS options for providing pid + procname testing 
 * for existence. Because OS might reuse the PID.
 */
#ifdef EX_OS_LINUX

#define ndrx_sys_is_process_running ndrx_sys_is_process_running_procfs

#elif EX_OS_CYGWIN

/* Same as for linux */
#define ndrx_sys_is_process_running ndrx_sys_is_process_running_procfs

#else

#define ndrx_sys_is_process_running ndrx_sys_is_process_running_by_kill

#endif
 
/******************************************************************************/

/*---------------------------Enums--------------------------------------*/
/*---------------------------Typedefs-----------------------------------*/
    
/**
 * (E)poll data
 */
typedef union ndrx_epoll_data {
        void    *ptr;
        int      fd;
        uint32_t u32;
        uint64_t u64;
        mqd_t    mqd;
} ndrx_epoll_data_t;

/**
 * (E)poll event
 */
struct ndrx_epoll_event {
        uint32_t     events;    /* Epoll events */
        ndrx_epoll_data_t data;      /* User data variable */
        
        /* The structure generally is the same as for linux epoll_wait
         * This bellow is extension for non linux version.
         */
        #ifndef EX_USE_EPOLL
        int         is_mqd;      /* Set to TRUE, if call is for message q */
        #endif
};


/**
 * List of posix queues
 */
typedef struct mq_list string_list_t;
struct mq_list
{
    char *qname;
    string_list_t *next;
};

/*---------------------------Globals------------------------------------*/
/*---------------------------Statics------------------------------------*/
/*---------------------------Prototypes---------------------------------*/

/* poll ops */
extern void ndrx_epoll_sys_init(void);
extern void ndrx_epoll_sys_uninit(void);
extern char * ndrx_epoll_mode(void);
extern int ndrx_epoll_ctl(int epfd, int op, int fd, struct ndrx_epoll_event *event);
extern int ndrx_epoll_ctl_mq(int epfd, int op, mqd_t fd, struct ndrx_epoll_event *event);
extern int ndrx_epoll_create(int size);
extern int ndrx_epoll_close(int fd);
extern int ndrx_epoll_wait(int epfd, struct ndrx_epoll_event *events, int maxevents, int timeout);
extern int ndrx_epoll_errno(void);
extern char * ndrx_poll_strerror(int err);

extern void ndrx_string_list_free(string_list_t* list);

extern char *ndrx_sys_get_cur_username(void);
extern string_list_t * ndrx_sys_ps_list(char *filter1, char *filter2, char *filter3, char *filter4);
extern string_list_t* ndrx_sys_folder_list(char *path, int *return_status);

/* gen unix: */
extern char * ndrx_sys_get_proc_name_by_ps(void);
extern int ndrx_sys_is_process_running_by_kill(pid_t pid, char *proc_name);
extern int ndrx_sys_is_process_running_by_ps(pid_t pid, char *proc_name);

/* sys_linux.c: */
extern int ndrx_sys_is_process_running_procfs(pid_t pid, char *proc_name);

/* sys_aix.c: */
extern char * ndrx_sys_get_proc_name_getprocs(void);

/* provided by: sys_<platform>.c */
extern string_list_t* ndrx_sys_mqueue_list_make(char *qpath, int *return_status);

#ifdef	__cplusplus
}
#endif

#endif	/* SYS_UNIX_H */

