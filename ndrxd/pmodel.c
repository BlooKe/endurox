/* 
** Build process model structure
**
** @file pmodel.c
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
#include <ndrx_config.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <utlist.h>
#include <errno.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

#include <ndrstandard.h>
#include <ndebug.h>
#include <ndrxd.h>
#include <ndrxdcmn.h>
#include <userlog.h>

#include <ntimer.h>
#include <cmd_processor.h>
#include <pthread.h>
#include <nstdutil.h>
#include <bridge_int.h>
#include <atmi_shm.h>
#include <sys_unix.h>

/*---------------------------Externs------------------------------------*/
/*---------------------------Macros-------------------------------------*/
/*---------------------------Enums--------------------------------------*/
/*---------------------------Typedefs-----------------------------------*/
/*---------------------------Globals------------------------------------*/
private pthread_t M_signal_thread; /* Signalled thread */
/*---------------------------Statics------------------------------------*/
/*---------------------------Prototypes---------------------------------*/
/**
 * Report to ndrxd, that process had sigchld
 * @return
 */
public int self_notify(srv_status_t *status, int block)
{
    int ret=SUCCEED;
    size_t  send_size = sizeof(srv_status_t);

    NDRX_LOG(log_debug, "About to send: %d bytes/%d svcs",
                        send_size, status->svc_count);
    /* we want new q/open + close here,
     * so that we do not interference with our maint queue blocked/non blocked flags.
     */
    ret=cmd_generic_callfl(NDRXD_COM_PMNTIFY_RQ, NDRXD_SRC_NDRXD,
                        NDRXD_CALL_TYPE_PM_INFO,
                        (command_call_t *)status, send_size,
                        G_command_state.listenq_str,
                        (mqd_t)FAIL,
                        (mqd_t)FAIL,
                        G_command_state.listenq_str,
                        0, 
                        NULL,
                        NULL,
                        NULL,
                        NULL,
                        FALSE, TPNOBLOCK);
    
out:
    return ret;
}

/**
 * Handle the dead child detected either by sigwait or signal.
 * @param chldpid
 * @param stat_loc
 */
private void handle_child(pid_t chldpid, int stat_loc)
{
    char buf[ATMI_MSG_MAX_SIZE];
    srv_status_t *status = (srv_status_t *)buf;

    memset(buf, 0, sizeof(buf));
    
    if (chldpid>0)
    {
        NDRX_LOG(log_warn, "sigchld: PID: %d exit status: %d",
                                           chldpid, stat_loc);
        if (WIFSTOPPED(stat_loc))
        {
            NDRX_LOG(log_warn, "Process is stopped - ignore..");
            return;
        }
        status->srvinfo.pid = chldpid;

        if (WIFEXITED(stat_loc) && (0 == (stat_loc & 0xff)))
        {
            NDRX_LOG(log_error, "Process normal shutdown!");
            status->srvinfo.state = NDRXD_PM_EXIT;
        }
        else if (WIFEXITED(stat_loc) && TPEXIT_ENOENT == WEXITSTATUS(stat_loc))
        {
            NDRX_LOG(log_error, "Binary not found!");
            status->srvinfo.state = NDRXD_PM_ENOENT;
        }
        else
        {
            NDRX_LOG(log_error, "Process abnormal shutdown!");
            status->srvinfo.state = NDRXD_PM_DIED;
        }
        /* NDRX_LOG(log_warn, "Sending notification"); */
        self_notify(status, FALSE);
    }
}

/**
 * Checks for child exit.
 * We will let mainthread to do all internal struct related work!
 * @return Got child exit
 */
private void * check_child_exit(void *arg)
{
    pid_t chldpid;
    int stat_loc;
    sigset_t blockMask;
    int sig;
    
        
    sigemptyset(&blockMask);
    sigaddset(&blockMask, SIGCHLD);
    
    NDRX_LOG(log_debug, "check_child_exit - enter...");
    for (;;)
    {
	int got_something = 0;

/* seems not working on darwin ... thus just wait for pid.
 * if we do not have any childs, then sleep for 1 sec.
 */
#ifndef EX_OS_DARWIN
        NDRX_LOG(log_debug, "about to sigwait()");
        if (SUCCEED!=sigwait(&blockMask, &sig))         /* Wait for notification signal */
        {
            NDRX_LOG(log_warn, "sigwait failed:(%s)", strerror(errno));

        }        
#endif
        
        NDRX_LOG(log_debug, "about to wait()");
        while ((chldpid = wait(&stat_loc)) >= 0)
        {
            got_something++;
            handle_child(chldpid, stat_loc);
        }
#if EX_OS_DARWIN
        NDRX_LOG(6, "wait: %s", strerror(errno));
        if (!got_something)
        {
            sleep(1);
        }
#endif
    }
   
    NDRX_LOG(log_debug, "check_child_exit: %s", strerror(errno));
    return NULL;
}


/**
 * Checks for child exit.
 * We will let mainthread to do all internal struct related work!
 * @return Got child exit
 */
public int thread_check_child_exit(void)
{
    pid_t chldpid;
    int stat_loc;
    struct rusage rusage;
    int ret=FALSE;
    char buf[ATMI_MSG_MAX_SIZE];
    srv_status_t *status = (srv_status_t *)buf;

    memset(buf, 0, sizeof(buf));
    memset(&rusage, 0, sizeof(rusage));

    while ((chldpid = wait3(&stat_loc, WNOHANG|WUNTRACED, &rusage)) > 0)
    {
        handle_child(chldpid, stat_loc);
    }
    
    return ret;
}

/**
 * Thread main entry...
 * @param arg
 * @return
 */
private void *sigthread_enter(void *arg)
{
    NDRX_LOG(log_error, "***********SIGNAL THREAD START***********");
    thread_check_child_exit();
    NDRX_LOG(log_error, "***********SIGNAL THREAD EXIT***********");
    
    return NULL;
}

/**
 * NDRXD process got sigchld (handling the signal if OS have slipped the thing) out
 * of the mask.
 * @return
 */
void sign_chld_handler(int sig)
{
    pthread_t thread;
    pthread_attr_t pthread_custom_attr;

    pthread_attr_init(&pthread_custom_attr);
    /* clean up resources after exit.. */
    pthread_attr_setdetachstate(&pthread_custom_attr, PTHREAD_CREATE_DETACHED);
    /* set some small stacks size, 1M should be fine! */
    pthread_attr_setstacksize(&pthread_custom_attr, 2048*1024);
    pthread_create(&thread, &pthread_custom_attr, sigthread_enter, NULL);

}

/**
 * Initialize polling lib
 * not thread safe.
 * @return
 */
public void ndrxd_sigchld_init(void)
{
    sigset_t blockMask;
    pthread_attr_t pthread_custom_attr;
    pthread_attr_t pthread_custom_attr_dog;
    struct sigaction sa; /* Seem on AIX signal might slip.. */
    char *fn = "ndrxd_sigchld_init";

    NDRX_LOG(log_debug, "%s - enter", fn);
    
    /* our friend AIX, might just ignore the SIG_BLOCK and raise signal
     * Thus we will handle the stuff in as it was in Enduro/X 2.5
     */
    
    sa.sa_handler = sign_chld_handler;
    sigemptyset (&sa.sa_mask);
    sa.sa_flags = SA_RESTART; /* restart system calls please... */
    sigaction (SIGCHLD, &sa, 0);
    
    /* Block the notification signal (do not need it here...) */
    
    sigemptyset(&blockMask);
    sigaddset(&blockMask, SIGCHLD);
    
    if (pthread_sigmask(SIG_BLOCK, &blockMask, NULL) == -1)
    {
        NDRX_LOG(log_always, "%s: sigprocmask failed: %s", fn, strerror(errno));
    }
    
    pthread_attr_init(&pthread_custom_attr);
    pthread_attr_init(&pthread_custom_attr_dog);
    
    /* set some small stacks size, 1M should be fine! */
    pthread_attr_setstacksize(&pthread_custom_attr, 2048*1024);
    pthread_create(&M_signal_thread, &pthread_custom_attr, 
            check_child_exit, NULL);
}

/**
 * Un-initialize sigchild monitor thread
 * @return
 */
public void ndrxd_sigchld_uninit(void)
{
    char *fn = "ndrxd_sigchld_uninit";

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
 * Add PID to PID hash
 * @param pid_hash
 * @return SUCCEED/FAIL
 */
public int add_to_pid_hash(pm_pidhash_t **pid_hash, pm_node_t *p_pm)
{
    int hash_key = p_pm->pid % ndrx_get_G_atmi_env()->max_servers;
    int ret=SUCCEED;
    pm_pidhash_t *pm_pid  = malloc(sizeof(pm_pidhash_t));
    memset(pm_pid, 0, sizeof(pm_pidhash_t));

    NDRX_LOG(log_debug, "About to add pid %d", p_pm->pid);

    /* check error */
    if (NULL==pm_pid)
    {
        ret=FAIL;
        NDRXD_set_error_fmt(NDRXD_EOS, "failed to allocate pm_pidhash_t (%d bytes): %s",
                            sizeof(pm_pidhash_t), strerror(errno));
        goto out;
    }

    pm_pid->p_pm = p_pm;
    pm_pid->pid = p_pm->pid;
    NDRX_LOG(log_debug, "Added pid %d to hash with key %d", pm_pid->pid, hash_key);
    DL_APPEND(pid_hash[hash_key], pm_pid);

out:
    return ret;
}

/**
 * Delete from PIDhash
 * @return SUCCEED/FAIL
 */
public int delete_from_pid_hash(pm_pidhash_t **pid_hash, pm_pidhash_t *pm_pid)
{
    int ret=SUCCEED;

    if (NULL!=pm_pid)
    {
        int hash_key = pm_pid->pid % ndrx_get_G_atmi_env()->max_servers;

        if (NULL!=pm_pid)
        {
            /* delete it */
            NDRX_LOG(log_error, "Removing pid %d from pidhash", pm_pid->pid);
            DL_DELETE(pid_hash[hash_key], pm_pid);
            /* here was memory leak!! */
            free(pm_pid);
        }
    }
out:
    return ret;
}

/**
 * Compares two field defs and returns equality result
 * @param a
 * @param b
 * @return 0 - equal/ -1 - not equal
 */
private int pid_hash_cmp(pm_pidhash_t *a, pm_pidhash_t *b)
{
    return (a->pid==b->pid?SUCCEED:FAIL);
}

/**
 * Get field entry from int hash
 */
public pm_pidhash_t *pid_hash_get(pm_pidhash_t **pid_hash, pid_t pid)
{
    /* Get the linear array key */
    int hash_key = pid % ndrx_get_G_atmi_env()->max_servers; /* Simple mod based hash */
    pm_pidhash_t *ret=NULL;
    pm_pidhash_t tmp;
    
    tmp.pid=pid;
    
    if (NULL!=pid_hash && NULL!=pid_hash[hash_key])
    {
        DL_SEARCH(pid_hash[hash_key], ret, &tmp, pid_hash_cmp);
        NDRX_LOG(log_debug, "Search for pid %d to hash with key %d, result: 0x%lx",
                            tmp.pid, hash_key, ret);
    }
    else
    {
       NDRX_LOG(log_warn, "Empty PID hashes. Cannot process pid %d", pid);
    }

    return ret;
}

/**
 * This will build initial process model
 * So this is linked list with all processes.
 *
 * Shall this support re-load?
 * In that case we should check that servers with existing ID's are not running,
 * but configuration is different?
 * - That probably could be done in another stange, there we should check existing
 * ones, if some process names are different, then reject!?!?
 */
public int build_process_model(conf_server_node_t *p_server_conf,
                                pm_node_t **p_pm_model, /* proces model linked list */
                                pm_node_t **p_pm_hash/* Hash table models */)
{
    int ret=SUCCEED;
    conf_server_node_t *p_conf;
    pm_node_t   *p_pm;

    int cnt;
    NDRX_LOG(log_debug, "build_process_model enter");

    DL_FOREACH(p_server_conf, p_conf)
    {
        for (cnt=0; cnt<p_conf->max; cnt++)
        {
            /* Now prepare add node to list + hash table */
            p_pm = calloc(1, sizeof(pm_node_t));
            if (NULL==p_pm)
            {
                /* Set return error code? */
                NDRXD_set_error_fmt(NDRXD_EOS, "Failed to allocate pm_node_t: `%s'",
                                                    strerror(errno));
                ret=FAIL;
                goto out;
            }
            
            /* format the process model entry */
            strcpy(p_pm->binary_name, p_conf->binary_name);
            p_pm->conf = p_conf; /* keep the reference to config entry */
            p_pm->srvid = p_conf->srvid+cnt;
            /* Request state is stopped */
            p_pm->reqstate = NDRXD_PM_NOT_STARTED;
            
            /* This must autostart! */
            if (p_conf->min > cnt)
            {
                p_pm->autostart=TRUE;
            }
            p_pm->autokill = p_conf->autokill;
            
            sprintf(p_pm->clopt, "-k %s -i %d %s", ndrx_get_G_atmi_env()->rnd_key, p_pm->srvid, p_conf->clopt);

            /* now check the hash table for server instance entry */
            if (p_pm->srvid < 1 || p_pm->srvid>ndrx_get_G_atmi_env()->max_servers)
            {
                /* Invalid srvid  */
                NDRXD_set_error_fmt(NDRXD_ESRVCIDINV, "Invalid server id `%d'", p_pm->srvid);
                ret = FAIL;
                goto out;
            }
            else if (NULL!=p_pm_hash[p_pm->srvid])
            {
                /* Duplicate srvid */
                NDRXD_set_error_fmt(NDRXD_ESRVCIDDUP, "Duplicate server id `%d'", p_pm->srvid);
                ret = FAIL;
                goto out;
            }
            else
            {
                NDRX_LOG(log_debug, "adding %s:%d", p_pm->binary_name, p_pm->srvid);
                /* Add it to has & model? */
                DL_APPEND(*p_pm_model, p_pm);
                /* Add it to the hash */
                p_pm_hash[p_pm->srvid] = p_pm;
            }
        }/* for */
    } /* DL_FOREACH */

out:
    NDRX_LOG(log_debug, "build_process_model return %d", ret);
    return ret;
}

/**
 *
 * @param srvid
 * @return
 */
public pm_node_t * get_pm_from_srvid(int srvid)
{
    if (srvid>=0 && srvid<ndrx_get_G_atmi_env()->max_servers)
    {
        return G_process_model_hash[srvid];
    }
    else
    {
        NDRX_LOG(log_error, "Got invalid srvid %d", srvid);
        return NULL;
    }
}

/**
 * Returns server admin q formatted.
 * @param p_pm
 * @return 
 */
public char * get_srv_admin_q(pm_node_t * p_pm)
{
    static char ret[NDRX_MAX_Q_SIZE+1];
    
    sprintf(ret, NDRX_ADMIN_FMT, G_sys_config.qprefix, p_pm->binary_name, 
            p_pm->srvid, p_pm->pid);
    
    return ret;
}

/**
 * Remove resources used by process which failed to start.
 * I.e. this should remove process from PIDhash & and any services advertised.
 * @param p_pm
 * @param svcnm - single service to un-register, optional
 * @param pm_pid - Optional, if provided then PIDs of p_pm & pm_pid ir tested.
 *                 If they are different then only record from pidhash is removed
 *                 with PID of pm_pid.
 * @return
 */
public int remove_startfail_process(pm_node_t *p_pm, char *svcnm, pm_pidhash_t *pm_pid)
{
    int ret=SUCCEED;
    pm_node_svc_t *elt, *tmp;
    
    if (NULL==p_pm)
        goto out;

    
    if (NULL!=pm_pid && p_pm->pid!=pm_pid->pid)
    {
        NDRX_LOG(log_warn, "Proces Model SRV/PID=%d/%d but given "
                "PID Hash SRV/PID=%d/%d - thus remove later from pidhash only!",
                p_pm->srvid, p_pm->pid, pm_pid->p_pm->srvid, pm_pid->pid);
        
        /* Remove only pidhash entry.
         * This might happen in cases if binary was externally started or some 
         * kind of delays in startup caused two instances to be started?
         */
        if (FAIL!=pm_pid->pid && 0!=pm_pid->pid)
        {
            delete_from_pid_hash(G_process_model_pid_hash,
                                pid_hash_get(G_process_model_pid_hash, pm_pid->pid));
        }
        
        goto out;
        
    }
    
    if (NULL==svcnm)
    {
        /* Remote from pidhash */
        if (FAIL!=p_pm->pid && 0!=p_pm->pid)
        {
            delete_from_pid_hash(G_process_model_pid_hash,
                                pid_hash_get(G_process_model_pid_hash, p_pm->pid));
        }
        /* Reset signal counter */
        p_pm->num_term_sigs=0;
     
        /* Reset kill request */
        p_pm->killreq = FALSE;
        
        /* Remove any queues used... */
        remove_server_queues(p_pm->binary_name, p_pm->pid, p_pm->srvid, NULL);
        
    }
    
    /* delete any services allocated */
    if (p_pm->flags&SRV_KEY_FLAGS_BRIDGE)
    {
        brd_del_bridge(p_pm->nodeid);
        /* Remove bridge related flags. */
        p_pm->flags&=~SRV_KEY_FLAGS_BRIDGE;
        p_pm->flags&=~SRV_KEY_FLAGS_SENDREFERSH;
        p_pm->nodeid = 0;
    }
    
    brd_begin_diff();
    /* Delete the stuff out */
    DL_FOREACH_SAFE(p_pm->svcs,elt,tmp)
    {
        int last;
        if (NULL==svcnm || 0==strcmp(elt->svc.svc_nm, svcnm))
        {
            NDRX_LOG(log_warn, "Removing pid's %d service [%s]", p_pm->pid, elt->svc.svc_nm);
            /* Remove from shm */
            
            /* ###################### CRITICAL SECTION ###################### */
            /* So we make this part critical... */
            if (SUCCEED!=ndrx_lock_svc_op())
            {
                ret=FAIL;
                goto out;
            }
            
            ndrxd_shm_uninstall_svc(elt->svc.svc_nm, &last, p_pm->srvid);
#ifndef EX_USE_EPOLL
            /* for poll() queues must be always removed. */
            remove_service_q(elt->svc.svc_nm, p_pm->srvid);
#else
            if (last)
            {
                remove_service_q(elt->svc.svc_nm, p_pm->srvid);
            }
#endif
            
            /* Remove the lock! */
            ndrx_unlock_svc_op();
            /* ###################### CRITICAL SECTION, END ################# */
            
            /*  Delete it from our bridge view */
            brd_del_svc_from_hash(elt->svc.svc_nm);
            
            /* Delete out it from list */
            DL_DELETE(p_pm->svcs,elt);
            /* Fee up resources */
            free(elt);
            
            if (NULL!=svcnm)
                break; /* This was one we wanted to remove! */
        }
    }
    brd_end_diff();
    
out:
    return ret;
}

/**
 * Start single process...
 * TODO: Add SIGCHLD handler here!
 * @param pm
 * @return SUCCEED/FAIL
 */
public int start_process(command_startstop_t *cmd_call, pm_node_t *p_pm,
            void (*p_startup_progress)(command_startstop_t *call, pm_node_t *p_pm, int calltype),
            long *p_processes_started,
            int no_wait,
            int *abort)
{
    int ret=SUCCEED;
    pid_t pid;

    /* prepare args for execution... */
    char cmd_str[PATH_MAX];
    char *cmd[PATH_MAX]; /* splitted pointers.. */
    char separators[]   = " ,\t\n";
    char *token;
    int numargs;


    NDRX_LOG(log_warn, "*********processing for startup %s/%d*********",
                                    p_pm->binary_name, p_pm->srvid);
    
    if (NDRXD_PM_RUNNING_OK==p_pm->state)
    {

        NDRX_LOG(log_warn, "Not starting %s/%d, laready in "
                                      "running state!",
                                      p_pm->binary_name, p_pm->srvid);
        goto out;
    }

   /* Send notification, that we are about to start? */
    p_pm->state = NDRXD_PM_STARTING;
    p_pm->state_changed = SANITY_CNT_START;

    if (NULL!=p_startup_progress)
        p_startup_progress(cmd_call, p_pm, NDRXD_CALL_TYPE_PM_STARTING);
    
    /* clone our self */
    pid = fork();

    if( pid == 0)
    {
        /* some small delay so that parent gets time for PIDhash setup! */
        usleep(9000);
        /* this is child - start EnduroX back-end*/
        /*fprintf(stderr, "starting with: [%s]", p_pm->clopt);*/
        strcpy(cmd_str, p_pm->clopt);

        cmd[0] = p_pm->binary_name;
        numargs=1;

        token = strtok(cmd_str, separators);
        while( token != NULL )
        {
            cmd[numargs] = token;
            token = strtok( NULL, separators );
            numargs++;
        }
        cmd[numargs] = NULL;
        
        /*  Override environment, if there is such thing */
        if (EOS!=p_pm->conf->env[0])
        {
            if (SUCCEED!=ndrx_load_new_env(p_pm->conf->env))
            {
                fprintf(stderr, "Failed to load custom env from: %s!\n", 
                        p_pm->conf->env);
                exit(1);
            }
        }
        
        if (EOS!=p_pm->conf->cctag[0])
        {
            if (SUCCEED!=setenv(NDRX_CCTAG, p_pm->conf->cctag, TRUE))
            {
                fprintf(stderr, "Cannot set [%s] to [%s]: %s\n", 
                        NDRX_CCTAG, p_pm->conf->cctag, strerror(errno));
                exit(1);
            }
        }
        
        if (SUCCEED != execvp (p_pm->binary_name, cmd))
        {
            int err = errno;

            fprintf(stderr, "Failed to start server, error: %d, %s\n",
                                err, strerror(err));
            if (ENOENT==err)
                exit(TPEXIT_ENOENT);
            else
                exit(1);
        }
    }
    else if (FAIL!=pid)
    {
        ndrx_timer_t timer;
        int finished = FALSE;
        /* Add stuff to PIDhash */
        p_pm->pid = pid;
        add_to_pid_hash(G_process_model_pid_hash, p_pm);
        
        /* Reset PM timer */
        p_pm->rsptimer = SANITY_CNT_START;
        
        /* Set requested state to started */
        p_pm->reqstate=NDRXD_PM_RUNNING_OK;
        
        /* Each time we try to start it we increment the try counter: */
        if (p_pm->exec_seq_try+1 < RESPAWN_CNTR_MAX)
        {
            p_pm->exec_seq_try++;
        }
        
        /* Do bellow stuff only if we can wait! */
        if (!no_wait)
        {
            /* this is parent for child - sleep some seconds, then check for PID... */
            /* TODO: Replace sleep with wait call from service - wait for message? */
            /*usleep(250000);  250 milli seconds */
            ndrx_timer_reset(&timer);

            do
            {
                NDRX_LOG(log_debug, "Waiting for response from srv...");
                /* do command processing for now */
                command_wait_and_run(&finished, abort);
                /* check the status? */
            } while (ndrx_timer_get_delta(&timer) < p_pm->conf->srvstartwait && 
                            NDRXD_PM_STARTING==p_pm->state && !(*abort));
            
            if (NDRXD_PM_RUNNING_OK==p_pm->state && p_pm->conf->sleep_after)
            {
                ndrx_timer_t sleep_timer;
                ndrx_timer_reset(&sleep_timer);
                
                do
                {
                    NDRX_LOG(log_debug, "In process after start sleep...");
                    command_wait_and_run(&finished, abort);
                } while (ndrx_timer_get_delta_sec(&sleep_timer) < p_pm->conf->sleep_after);
                
            }
            
            /* Check for process name & pid */
            if (ndrx_sys_is_process_running(pid, p_pm->binary_name))
            {
                /*Should be set at info: p_pm->state = NDRXD_PM_RUNNING;*/
                NDRX_LOG(log_debug, "binary %s, srvid %d started with pid %d",
                            p_pm->binary_name, p_pm->srvid, pid);
                (*p_processes_started)++;
            }
            else if (NDRXD_PM_NOT_STARTED==p_pm->state)
            {
                /* Assume died? */
                p_pm->state = NDRXD_PM_DIED;
                p_pm->state_changed = SANITY_CNT_START;
                NDRX_LOG(log_debug, "binary %s, srvid %d failed to start",
                            p_pm->binary_name, p_pm->srvid, pid);
            }
        }
    }
    else
    {
        NDRXD_set_error_fmt(NDRXD_EOS, "Fork failed: %s", strerror(errno));
        p_pm->state = NDRXD_PM_DIED;
        p_pm->state_changed = SANITY_CNT_START;
        /*
        ret=FAIL;
         */
    }

    NDRX_LOG(log_debug, "PID of started process is %d", p_pm->pid);
            if (NULL!=p_startup_progress)
                p_startup_progress(cmd_call, p_pm, NDRXD_CALL_TYPE_PM_STARTED);

out:
    return ret;
}

/**
 * Stop single process
 * @param p_pm
 * @return SUCCEED/FAIL
 */
public int stop_process(command_startstop_t *cmd_call, pm_node_t *p_pm,
            void (*p_shutdown_progress)(command_call_t *call, pm_node_t *pm, int calltype),
            long *p_processes_shutdown,
            int *abort)
{
    int ret=SUCCEED;
    command_call_t call;
    ndrx_timer_t timer;
    int finished = FALSE;
    char srv_queue[NDRX_MAX_Q_SIZE+1];
    char fn[] = "stop_process";

    memset(&call, 0, sizeof(call));
    NDRX_LOG(log_debug, "%s: Enter", fn);


    /* Check process initially, do it really requires shutdown? */
    NDRX_LOG(log_warn, "processing shutdown %s/%d",
                                p_pm->binary_name, p_pm->srvid);

    /* Request that we stay stopped! */
    p_pm->reqstate = NDRXD_PM_NOT_STARTED;
    
    if (NDRXD_PM_RUNNING_OK!=p_pm->state &&
            NDRXD_PM_STOPPING!=p_pm->state &&
            NDRXD_PM_STARTING!=p_pm->state)
    {
        NDRX_LOG(log_debug, "Proces already in non-runnabled "
                                    "state: %d", p_pm->state);
        /* set the state to shutdown! */
        p_pm->state = NDRXD_PM_EXIT;
        p_pm->state_changed = SANITY_CNT_START;
        goto out;
    }

    /* Send notification, that we are about to stop? */
    p_pm->state = NDRXD_PM_STOPPING;
    p_pm->state_changed = SANITY_CNT_START;
    /* Reset response timer */
    p_pm->rsptimer = SANITY_CNT_START;
    

    if (NULL!=cmd_call && NULL!=p_shutdown_progress)
        p_shutdown_progress((command_call_t*)cmd_call, p_pm, NDRXD_CALL_TYPE_PM_STOPPING);

    /* Form a call queue, probably we need move all stuff to atmienv!  */
    sprintf(srv_queue, NDRX_ADMIN_FMT, G_sys_config.qprefix, 
            p_pm->binary_name, p_pm->srvid, p_pm->pid);
    NDRX_LOG(log_debug, "%s: calling up: [%s]", fn, srv_queue);
    
    /* Then get listing... */
    if (SUCCEED!=(ret=cmd_generic_call_2(NDRXD_COM_SRVSTOP_RQ, NDRXD_SRC_ADMIN,
                    NDRXD_CALL_TYPE_GENERIC,
                    &call, sizeof(call),
                    G_command_state.listenq_str,
                    G_command_state.listenq,
                    (mqd_t)FAIL,
                    srv_queue,
                    0, NULL,
                    NULL,
                    NULL,
                    NULL,
                    FALSE,
                    FALSE,
                    NULL, NULL, TPNOTIME, NULL)))
    {
        /*goto out; Ignore this condition, just get the status of binary... */
    }

    ndrx_timer_reset(&timer);
    do
    {
        NDRX_LOG(log_debug, "Waiting for response from srv... state: %d",
                                        p_pm->state);   
        /* do command processing for now */
        command_wait_and_run(&finished, abort);
        /* check the status? */
    } while (ndrx_timer_get_delta(&timer) < p_pm->conf->srvstopwait &&
                    !PM_NOT_RUNNING(p_pm->state) &&
                    !(*abort));

    /* grab some stats... */
    if (PM_NOT_RUNNING(p_pm->state))
    {
        if (NULL!=p_processes_shutdown)
            (*p_processes_shutdown)++;
    }
    
    /* Send the progress of the shtudown (detail) */
    if (NULL!=cmd_call && NULL!=p_shutdown_progress)
        p_shutdown_progress((command_call_t*)cmd_call, p_pm, NDRXD_CALL_TYPE_PM_STOPPED);

out:
    NDRX_LOG(log_debug, "%s: Exit (%d)", fn, ret);
    return ret;
}

/**
 * Start whole application. If configuration is not loaded, then this will
 * initiate configuration load.
 * @return SUCCEED/FAIL
 */
public int app_startup(command_startstop_t *call,
        void (*p_startup_progress)(command_startstop_t *call, pm_node_t *pm, int calltype),
        long *p_processes_started) /* have some progress feedback */
{
    int ret=SUCCEED;
    pm_node_t *p_pm;
    int abort = FALSE;
    NDRX_LOG(log_warn, "Starting application domain");

    if (NULL==G_app_config && SUCCEED!=load_active_config(&G_app_config,
                &G_process_model, &G_process_model_hash, &G_process_model_pid_hash))
    {
        ret=FAIL;
        goto out;
    }

    /* OK, now loop throught the stuff */
    G_sys_config.stat_flags |= NDRXD_STATE_DOMSTART;

    if (FAIL!=call->srvid)
    {
        /* Check the servid... */
        if (call->srvid>=0 && call->srvid<ndrx_get_G_atmi_env()->max_servers)
        {
            pm_node_t *p_pm_srvid = G_process_model_hash[call->srvid];

            if (NULL!=p_pm_srvid)
            {
                start_process(call, p_pm_srvid, p_startup_progress,
                                    p_processes_started, FALSE, &abort);
            }
            else
            {
                NDRX_LOG(log_error, "Srvid: %d not initialized", call->srvid);
            }
        }
        else
        {
            NDRX_LOG(log_error, "Invalid srvid: %d", call->srvid);
        }
    }
    else /* process this if request for srvnm or full startup... */
    {
        DL_FOREACH(G_process_model, p_pm)
        {
            /* if particular binary shutdown requested (probably we could add some index!?) */
            if ((EOS!=call->binary_name[0] && 0==strcmp(call->binary_name, p_pm->binary_name)) ||
                    /* Do full startup if requested autostart! */
                    (EOS==call->binary_name[0] && p_pm->autostart)) /* or If full shutdown requested */
            {
                start_process(call, p_pm, p_startup_progress, 
                        p_processes_started, FALSE, &abort);
                
                if (abort)
                {
                    NDRX_LOG(log_warn, "Aborting app domain startup!");
                    NDRXD_set_error_fmt(NDRXD_EABORT, "App domain startup aborted!");
                    ret=FAIL;
                    goto out;
                }
            }
        } /* DL_FORACH pm. */
    }
    
out:
    return ret;
}

/**
 * Stop whole application.
 * App domain must be started in order to do shutdown.
 * @return SUCCEED/FAIL
 */
public int app_shutdown(command_startstop_t *call,
        /* have some progress feedback */
        void (*p_shutdown_progress)(command_call_t *call, pm_node_t *pm, int calltype),
        long *p_processes_shutdown)
{
    int ret=SUCCEED;
    int abort=FALSE;
    pm_node_t *p_pm;
    
    NDRX_LOG(log_warn, "Stopping application domain");


    /* OK, now loop throught the stuff 
    G_sys_config.stat_flags |= NDRXD_STATE_SHUTDOWN;
*/
    if (FAIL!=call->srvid)
    {
        /* Check the servid... */
        if (call->srvid>=0 && call->srvid<ndrx_get_G_atmi_env()->max_servers)
        {
            pm_node_t *p_pm_srvid = G_process_model_hash[call->srvid];
            
            if (NULL!=p_pm_srvid)
            {
                stop_process(call, p_pm_srvid, p_shutdown_progress,
                                    p_processes_shutdown, &abort);
            }
            else
            {
                NDRX_LOG(log_error, "Srvid: %d not initialized", call->srvid);
            }
        }
        else
        {
            NDRX_LOG(log_error, "Invalid srvid: %d", call->srvid);
        }
    }
    else if (G_process_model) /* process this if request for srvnm or full shutdown... */
    {
        int i;
        DL_REVFOREACH(G_process_model, p_pm, i)
        {
                /* if particular binary shutdown requested (probably we could add some index!?) */
            if ((EOS!=call->binary_name[0] && 0==strcmp(call->binary_name, p_pm->binary_name)) ||
                    EOS==call->binary_name[0]) /* or If full shutdown requested */
            {
                stop_process(call, p_pm, p_shutdown_progress, 
                        p_processes_shutdown, &abort);
                
                if (abort)
                {
                    NDRX_LOG(log_warn, "Aborting app domain shutdown!");
                    NDRXD_set_error_fmt(NDRXD_EABORT, "App domain shutdown aborted!");
                    ret=FAIL;
                    goto out;
                }
            }
        } /* For each pm. */
    }
    
    /* set state that we are shutdowned...
     * Hmm we should reply back and at next loop go down?
     * Or stay in idle?
     Not now, somehow needs to sync...
    G_sys_config.stat_flags &= ~NDRXD_STATE_SHUTDOWN;
    G_sys_config.stat_flags |= NDRXD_STATE_SHUTDOWNED;
    */
out:
    return ret;
}

/**
 * Test process mode, it should be shutdown...!
 * @return 
 */
public int is_srvs_down(void)
{
    pm_node_t *p_pm;
    int is_down = TRUE;
    
    DL_FOREACH(G_process_model, p_pm)
    {
        if (PM_RUNNING(p_pm->state))
        {
            NDRX_LOG(6, "All servers not down...");
            is_down=FALSE;
        }
    } /* DL_FORACH pm. */
    
    return is_down;
}

/**
 * Reload services...
 * @return SUCCEED/FAIL
 */
public int app_sreload(command_startstop_t *call,
        void (*p_startup_progress)(command_startstop_t *call, pm_node_t *pm, int calltype),
        void (*p_shutdown_progress)(command_call_t *call, pm_node_t *pm, int calltype),
        long *p_processes_started) /* have some progress feedback */
{
    int ret=SUCCEED;
    pm_node_t *p_pm;
    int abort = FALSE;
    NDRX_LOG(log_warn, "Starting application domain");

    /*
    if (NULL==G_app_config && SUCCEED!=load_active_config(&G_app_config,
                &G_process_model, &G_process_model_hash, &G_process_model_pid_hash))
    {
        ret=FAIL;
        goto out;
    }
     */
    
    if (NULL==G_app_config)
    {
        NDRX_LOG(log_error, "No configuration loaded!");
        ret=FAIL;
        goto out;
    }

    /* OK, now loop throught the stuff 
    G_sys_config.stat_flags |= NDRXD_STATE_DOMSTART;
     * */

    if (FAIL!=call->srvid)
    {
        /* Check the servid... */
        if (call->srvid>=0 && call->srvid<ndrx_get_G_atmi_env()->max_servers)
        {
            pm_node_t *p_pm_srvid = G_process_model_hash[call->srvid];

            if (NULL!=p_pm_srvid)
            {
                /* Firstly stop it */
                stop_process(call, p_pm_srvid, p_shutdown_progress, 
                        NULL, &abort);

                /* Then start it */
                if (!abort)
                {
                    start_process(call, p_pm_srvid, p_startup_progress,
                                        p_processes_started, FALSE, &abort);
                }
                
                if (abort)
                {
                    NDRX_LOG(log_warn, "Aborting app domain startup!");
                    NDRXD_set_error_fmt(NDRXD_EABORT, "App domain startup aborted!");
                    ret=FAIL;
                    goto out;
                }
            }
            else
            {
                NDRX_LOG(log_error, "Srvid: %d not initialized", call->srvid);
            }
        }
        else
        {
            NDRX_LOG(log_error, "Invalid srvid: %d", call->srvid);
        }
    }
    else /* process this if request for srvnm or full startup... */
    {
        DL_FOREACH(G_process_model, p_pm)
        {
            /* if particular binary shutdown requested (probably we could add some index!?) */
            if ((EOS!=call->binary_name[0] && 0==strcmp(call->binary_name, p_pm->binary_name)) ||
                    /* Do full startup if requested autostart! */
                    (EOS==call->binary_name[0] && p_pm->autostart)) /* or If full shutdown requested */
            {
                
                stop_process(call, p_pm, p_shutdown_progress, 
                        NULL, &abort);
                
                if (!abort)
                {
                    start_process(call, p_pm, p_startup_progress, 
                            p_processes_started, FALSE, &abort);
                }
                
                if (abort)
                {
                    NDRX_LOG(log_warn, "Aborting app domain startup!");
                    NDRXD_set_error_fmt(NDRXD_EABORT, "App domain startup aborted!");
                    ret=FAIL;
                    goto out;
                }
            }
        } /* DL_FORACH pm. */
    }
    
out:
    return ret;
}

