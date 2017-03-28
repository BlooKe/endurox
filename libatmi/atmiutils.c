/* 
** Utility functions for ATMI (generic send, etc...)
** TODO: Add timed sends.
** We might want to monitor with `stat' the disk inode of the queue
** and we could cache the opened queues, so that we do not open them
** every time.
**
** @file atmiutils.c
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
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <sys_mqueue.h>
#include <errno.h>
#include <sys/param.h>
#include <unistd.h>
#include <fcntl.h>

#include <atmi.h>
#include <ndrstandard.h>
#include <ndebug.h>
#include <atmi_int.h>
#include <ndrxdcmn.h>
#include <utlist.h>
#include <atmi_shm.h>
#include <tperror.h>

#include "gencall.h"
#include "userlog.h"
/*---------------------------Externs------------------------------------*/
/*---------------------------Macros-------------------------------------*/
#define SLEEP_ON_FULL_Q             170000   /* Sleep 150 ms every batch..... */

/* Calculate timeout time */
#define SET_TOUT_VALUE     if (use_tout)\
    {\
        struct timeval  timeval;\
        use_tout=1;\
        gettimeofday (&timeval, NULL);\
        abs_timeout.tv_sec = timeval.tv_sec+G_atmi_env.time_out;\
        abs_timeout.tv_nsec = timeval.tv_usec*1000;\
    }

/* Configure function to use TOUT */
#define SET_TOUT_CONF     if (G_atmi_env.time_out==0 || flags & TPNOTIME)\
    {\
        use_tout=0;\
    }\
    else \
    {\
        use_tout=1;\
    }
/* This prints info about q descriptor X */
#define PRINT_Q_INFO(X)             { struct mq_attr __attr;\
            memset((char *)&__attr, 0, sizeof(__attr));\
            ndrx_mq_getattr(X, &__attr);\
            NDRX_LOG(log_error, "mq_flags=%ld mq_maxmsg=%ld mq_msgsize=%ld mq_curmsgs=%ld",\
                                __attr.mq_flags, __attr.mq_maxmsg, __attr.mq_msgsize, __attr.mq_curmsgs);}
/*---------------------------Enums--------------------------------------*/
/*---------------------------Typedefs-----------------------------------*/
/*---------------------------Globals------------------------------------*/
/*---------------------------Statics------------------------------------*/
/*---------------------------Prototypes---------------------------------*/

/**
 * When tons of messages are sent to xadmin, then we might gets some sleep,
 * so that console is ready to display complete stuff..!
 * 
 * NOTE: THIS IS NOT VERY CLEARN SOLUTION! But what to do... xadmin is not very critical part anywya!
 * 
 * Or... might switch to blocked concept... with some time of 2 secs, after which
 * if expired, assume that xadmin is dead.
 * 
 * Might think in future increase msg_max size...!
 * @return 
 */
public void ndrx_mq_fix_mass_send(int *cntr)
{
    *cntr = *cntr + 1;
    /* Have some backup */
    if (*cntr >= G_atmi_env.msg_max - Q_SEND_GRACE)
    {
        NDRX_LOG(log_error, "About to sleep! %d %d", *cntr, G_atmi_env.msg_max);
        usleep(SLEEP_ON_FULL_Q);
        *cntr = 0;
    }
}

/**
 * Change attributes of the queue
 * @param q_descr
 * @return 
 */
public int ndrx_q_setblock(mqd_t q_descr, int blocked)
{
    int ret=SUCCEED;
    struct mq_attr new;
    struct mq_attr old;
    int change = FALSE;
    
    if (SUCCEED!= ndrx_mq_getattr(q_descr, &old))
    {
        NDRX_LOG(log_warn, "Failed to get attribs of Q: %d, err: %s", 
                q_descr, strerror(errno));
        ret=FAIL;
        goto out;
    }
    
    /*non blocked requested & but currently Q is blocked => change attribs*/
    if (!blocked && !(old.mq_flags & O_NONBLOCK))
    {
        memcpy(&new, &old, sizeof(new));
        NDRX_LOG(log_warn, "Switching qd %d to non-blocked", q_descr);
        new.mq_flags |= O_NONBLOCK;
        change = TRUE;
    }
    /*blocked requested & but currently Q is not blocked => change attribs*/
    else if (blocked && old.mq_flags & O_NONBLOCK)
    {
        memcpy(&new, &old, sizeof(new));
        NDRX_LOG(log_warn, "Switching qd %d to blocked", q_descr);
        new.mq_flags &= ~O_NONBLOCK; /* remove non block flag */
        change = TRUE;
    }
    
    if (change)
    {
        if (FAIL==ndrx_mq_setattr(q_descr, &new,
                            &old))
        {
            NDRX_LOG(log_error, "Failed to set attribs for qd %d: %s", 
                    q_descr, strerror(errno));
            ret=FAIL;
            goto out;
        }
    }
    
out:
    return ret;
}

/**
 * Open queue with attributes
 * @param name
 * @param oflag
 * @param attr
 * @return
 */
public mqd_t ndrx_mq_open_at(const char *name, int oflag, mode_t mode, struct mq_attr *attr)
{
    struct mq_attr attr_int;
    struct mq_attr * p_at;

    if (NULL==attr)
    {
        memset(&attr_int, 0, sizeof(attr_int));
        p_at = &attr_int;
    }
    else
    {
        p_at = attr;
    }

    if (!p_at->mq_maxmsg)
        p_at->mq_maxmsg = G_atmi_env.msg_max;

    if (!p_at->mq_msgsize)
        p_at->mq_msgsize = G_atmi_env.msgsize_max;

    /*NDRX_LOG(log_debug, "mq_maxmsg: %d mq_msgsize: %d",
                                       p_at->mq_maxmsg, p_at->mq_msgsize);*/
    return ndrx_mq_open(name, oflag, mode, p_at);
}

/**
 * Generic queue open
 * @param name
 * @param oflag
 * @return
 */
public mqd_t ndrx_mq_open_at_wrp(const char *name, int oflag)
{
    
    return ndrx_mq_open_at(name, oflag, 0, NULL);
}

/**
 * Will format the queue and do the call. Also does reports back errors (with
 * details).
 * This will be blocked send.
 * 
 * TODO: Added support for TPNOBLOCK!!!
 * @param svc
 * @param data
 * @param len
 * @return
 */
public int generic_qfd_send(mqd_t q_descr, char *data, long len, long flags)
{
    int ret=SUCCEED;
    int use_tout;
    struct timespec abs_timeout;
    SET_TOUT_CONF;
    
restart:

    SET_TOUT_VALUE;
    if ((!use_tout && FAIL==ndrx_mq_send(q_descr, data, len, 0)) ||
         (use_tout && FAIL==ndrx_mq_timedsend(q_descr, data, len, 0, &abs_timeout)))
    {
        if (EINTR==errno && flags & TPSIGRSTRT)
        {
            NDRX_LOG(log_warn, "Got signal interrupt, restarting ndrx_mq_send");
            goto restart;
        }
        else if (EAGAIN==errno)
        {
            PRINT_Q_INFO(q_descr);
        }
        ret=errno;
        NDRX_LOG(log_error, "Failed to send data to fd [%d] with error: %s",
                                        q_descr, strerror(ret));
    }

out:
    return ret;
}

/**
 * Send data to Queue
 * @param queue
 * @param data
 * @param len
 * @param flags
 * @return 
 */
public int generic_q_send(char *queue, char *data, long len, long flags)
{
    return generic_q_send_2(queue, data, len, flags, FAIL);
}

/**
 * Will format the queue and do the call. Also does reports back errors (with
 * details).
 * This will be blocked send.
 * @param svc
 * @param data
 * @param len
 * @tout - time-out in seconds.
 * @return
 */
public int generic_q_send_2(char *queue, char *data, long len, long flags, long tout)
{
    int ret=SUCCEED;
    mqd_t q_descr=(mqd_t)FAIL;
    int use_tout;
    struct timespec abs_timeout;
    long add_flags = 0;
    SET_TOUT_CONF;

    /* Set nonblock flag to system, if provided to EnduroX */
    if (flags & TPNOBLOCK)
    {
        NDRX_LOG(log_debug, "Enabling NONBLOCK send");
        add_flags|=O_NONBLOCK;
    }

    /* open the queue */
    /* Restart until we do not get the signal */
restart_open:
    q_descr = ndrx_mq_open_at_wrp (queue, O_WRONLY | add_flags);

    if ((mqd_t)FAIL==q_descr && EINTR==errno && flags & TPSIGRSTRT)
    {
        NDRX_LOG(log_warn, "Got signal interrupt, restarting ndrx_mq_open");
        goto restart_open;
    }
    
    if ((mqd_t)FAIL==q_descr)
    {
        NDRX_LOG(log_error, "Failed to open queue [%s] with error: %s",
                                        queue, strerror(errno));
        ret=errno;
        goto out;
    }

    /* now try to send */
restart_send:

    if (use_tout)
    {
        struct timeval  timeval;
        use_tout=1;
        gettimeofday (&timeval, NULL);
        
        /* Allow to use custom time-out handler. */
        if (tout>0)
            abs_timeout.tv_sec = timeval.tv_sec+tout;
        else
            abs_timeout.tv_sec = timeval.tv_sec+G_atmi_env.time_out;
        
        abs_timeout.tv_nsec = timeval.tv_usec*1000;
    }

    NDRX_LOG(6, "use timeout: %d config: %d", 
                use_tout, G_atmi_env.time_out);
    if ((!use_tout && FAIL==ndrx_mq_send(q_descr, data, len, 0)) ||
         (use_tout && FAIL==ndrx_mq_timedsend(q_descr, data, len, 0, &abs_timeout)))
    {
        ret=errno;
        if (EINTR==errno && flags & TPSIGRSTRT)
        {
            NDRX_LOG(log_warn, "Got signal interrupt, restarting ndrx_mq_send");
            goto restart_send;
        }
        else if (EAGAIN==errno)
        {
            PRINT_Q_INFO(q_descr);
        }
        NDRX_LOG(log_error, "Failed to send data to queue [%s] with error: %s",
                                        queue, strerror(ret));
    }

restart_close:
    /* Generally we ingore close */
    if (FAIL==ndrx_mq_close(q_descr))
    {
        if (EINTR==errno && flags & TPSIGRSTRT)
        {
            NDRX_LOG(log_warn, "Got signal interrupt, restarting ndrx_mq_close");
            goto restart_close;
        }
    }

out:
    return ret;
}

/**
 * Generic queue receiver
 * @param q_descr - queue descriptro
 * @param buf - where to put received data
 * @param buf_max - buffer size
 * @param prio - priority of message
 * @return GEN_QUEUE_ERR_NO_DATA/FAIL/data len
 */
public long generic_q_receive(mqd_t q_descr, char *buf, long buf_max, unsigned *prio, long flags)
{
    long ret=SUCCEED;
    int use_tout;
    struct timespec abs_timeout;
    SET_TOUT_CONF;
    
restart:
    SET_TOUT_VALUE;
    NDRX_LOG(6, "use timeout: %d config: %d", use_tout, G_atmi_env.time_out);
    if ((!use_tout && FAIL==(ret=ndrx_mq_receive (q_descr, (char *)buf, buf_max, prio))) ||
         (use_tout && FAIL==(ret=ndrx_mq_timedreceive (q_descr, (char *)buf, buf_max, prio, &abs_timeout))))
    {
        if (EINTR==errno && flags & TPSIGRSTRT)
        {
            NDRX_LOG(log_warn, "Got signal interrupt, restarting ndrx_mq_receive");
            goto restart;
        }
        /* save the errno, so that no one changes */
        ret=errno;
        if (EAGAIN==ret)
        {
            NDRX_LOG(log_debug, "No messages in queue");
            ret= GEN_QUEUE_ERR_NO_DATA;
        }
        else
        {
            int err;

            CONV_ERROR_CODE(ret, err);

            ret=FAIL;
            _TPset_error_fmt(err, "ndrx_mq_receive failed: %s", strerror(errno));
        }
    }
    return ret;
}

/**
 * Initialize generic structure (used for network send.)
 * @param call
 */
public void cmd_generic_init(int ndrxd_cmd, int msg_src, int msg_type,
                            command_call_t *call, char *reply_q)
{
        call->command = ndrxd_cmd;
        call->magic = NDRX_MAGIC;
        call->msg_src = msg_src;
        call->msg_type = msg_type;
        strcpy(call->reply_queue, reply_q);
        call->caller_nodeid = G_atmi_env.our_nodeid;
}

/**
 * Generic server commandj call & response processing.
 * @param p_cmd_map
 * @param argc
 * @param argv
 * @param p_have_next
 * @return SUCCEED/FAIL
 */
public int cmd_generic_call_2(int ndrxd_cmd, int msg_src, int msg_type,
                            command_call_t *call, size_t call_size,
                            char *reply_q,
                            mqd_t reply_queue, /* listen for answer on this! */
                            mqd_t admin_queue, /* this might be FAIL! */
                            char *admin_q_str, /* should be set! */
                            int argc, char **argv,
                            int *p_have_next,
                            int (*p_rsp_process)(command_reply_t *reply, size_t reply_len),
                            void (*p_put_output)(char *text),
                            int need_reply,
                            int reply_only,
                            char *rply_buf_out,             /* might be a null  */
                            int *rply_buf_out_len,          /* if above is set, then must not be null */
                            int flags,
        /* TODO Might want to add checks before calling rply_request func...
         * for magic and command code. */
                            int (*p_rply_request)(char *buf, long len)
        )
{
    int ret=SUCCEED;
    command_reply_t *reply;
    unsigned prio = 0;
    char    msg_buffer_max[ATMI_MSG_MAX_SIZE];
    
    long  reply_len;

    NDRX_LOG(log_debug, "gencall command: %d, reply_only=%d, need_reply=%d "
            "call flags=0x%x, getcall flags=%d",
            ndrxd_cmd, reply_only, need_reply, call->flags, flags);
    
    if (NULL!=rply_buf_out && NULL==rply_buf_out_len)
    {
        NDRX_LOG(log_error, "User buffer address specified in params, "
                "but rply_buf_out_len is NULL!");
        FAIL_OUT(ret);
    }
    
    if (!reply_only)
    {
        call->command = ndrxd_cmd;
        call->magic = NDRX_MAGIC;
        call->msg_src = msg_src;
        call->msg_type = msg_type;
        strcpy(call->reply_queue, reply_q);
        call->caller_nodeid = G_atmi_env.our_nodeid;

        if ((mqd_t)FAIL!=admin_queue)
        {
            NDRX_LOG(log_error, "Sending data to [%s], fd=%d, call flags=0x%x", 
                                admin_q_str, admin_queue, call->flags);
            if (SUCCEED!=generic_qfd_send(admin_queue, (char *)call, call_size, flags))
            {
                NDRX_LOG(log_error, "Failed to send msg to ndrxd!");

                if (NULL!=p_put_output)
                    p_put_output("Failed to send msg to ndrxd!");

                ret=FAIL;
                goto out;
            }
        }
        else
        {
            NDRX_LOG(log_info, "Sending data to [%s] call flags=0x%x", 
                                    admin_q_str, call->flags);
            if (SUCCEED!=generic_q_send(admin_q_str, (char *)call, call_size, flags))
            {
                if (NULL!=p_put_output)
                    p_put_output("Failed to send msg to ndrxd!");

                ret=FAIL;
                goto out;
            }
        }
    }
    else
    {
        NDRX_LOG(log_debug, "Reply mode only");
    }

    if (need_reply)
    {
        NDRX_LOG(log_debug, "Waiting for response from ndrxd on [%s]", reply_q);
    }
    else
    {
        NDRX_LOG(log_debug, "Reply not needed!");
        goto out;
    }

    do
    {
        command_call_t *test_call = (command_call_t *)msg_buffer_max;
        /* Error could be also -2..! */
        if (0>(reply_len=generic_q_receive(reply_queue,
                msg_buffer_max, sizeof(msg_buffer_max), &prio, flags)))
        {
            NDRX_LOG(log_error, "Failed to receive reply from ndrxd!");

            if (NULL!=p_put_output)
                p_put_output("Failed to receive reply from ndrxd!");

            ret=FAIL;
            goto out;
        }
        else if (test_call->command % 2 == 0 && NULL!=p_rply_request)
        {
            if (SUCCEED!=p_rply_request(msg_buffer_max, reply_len))
            {
                NDRX_LOG(log_error, "Failed to process request!");
                ret=FAIL;
                goto out;
            }
            else
            {
                NDRX_LOG(log_warn, "Waiting for next rply msg...");
                continue;
            }
        }
        else if (reply_len < sizeof(command_reply_t))
        {
            NDRX_LOG(log_error, "Reply size %ld, expected atleast %ld!",
                                reply_len, sizeof(command_reply_t));

            if (NULL!=p_put_output)
                p_put_output("Invalid reply size from ndrxd!");

            ret=FAIL;
            goto out;
        }
        /* Check the reply msg */
        reply = (command_reply_t *)msg_buffer_max;

        if (NDRX_MAGIC!=reply->magic)
        {
            NDRX_LOG(log_error, "Got invalid response from ndrxd: invalid magic (expected: %ld, got: %ld)!",
                    NDRX_MAGIC, reply->magic);

            if (NULL!=p_put_output)
                p_put_output("Invalid response - invalid header!");

            ret=FAIL;
            goto out;
        }

        /* Check command code */
        if (ndrxd_cmd+1 != reply->command)
        {
            NDRX_LOG(log_error, "Got invalid response from ndrxd: expected: %d, got %d",
                                    call->command+1, reply->command);

            if (NULL!=p_put_output)
                p_put_output("Invalid response - invalid response command code!");

            ret=FAIL;
            goto out;
        }

        /* check reply status: */
        if (SUCCEED==reply->status)
        {
            if (NULL!=p_rsp_process)
            {
                ret=p_rsp_process(reply, reply_len);
            }
            else
            {   /* just print OK */
                if (NULL!=p_put_output)
                    p_put_output("OK");
            }
            
            if (NULL!=rply_buf_out && NULL!=rply_buf_out_len)
            {
                if (reply_len>*rply_buf_out_len)
                {
                    NDRX_LOG(log_warn, "Received packet size %d longer "
                            "than user buffer in rply_buf_len %d", 
                            reply_len, *rply_buf_out_len);
                    FAIL_OUT(ret);
                }
                else
                {
                    memcpy(rply_buf_out, reply, reply_len);
                    *rply_buf_out_len = reply_len;
                }
            }
        }
        else
        {
            char buf[2048];
            sprintf(buf, "fail, code: %d: %s", reply->error_code, reply->error_msg);
            NDRX_LOG(log_warn, "%s", buf);

            if (NULL!=p_put_output)
                    p_put_output(buf);

            ret = reply->status;
            goto out;
        }
        /* do above while we are waiting for stuff back... */
    } while((reply->flags & NDRXD_REPLY_HAVE_MORE));

out:
    return ret;
}

/**
 * This is wrapper for cmd_generic_call_2,
 * Full request.
 */
public int cmd_generic_call(int ndrxd_cmd, int msg_src, int msg_type,
                            command_call_t *call, size_t call_size,
                            char *reply_q,
                            mqd_t reply_queue, /* listen for answer on this! */
                            mqd_t admin_queue, /* this might be FAIL! */
                            char *admin_q_str, /* should be set! */
                            int argc, char **argv,
                            int *p_have_next,
                            int (*p_rsp_process)(command_reply_t *reply, size_t reply_len),
                            void (*p_put_output)(char *text),
                            int need_reply)
{
    return cmd_generic_call_2(ndrxd_cmd, msg_src, msg_type,
                            call, call_size,
                            reply_q,
                            reply_queue,
                            admin_queue,
                            admin_q_str,
                            argc, argv,
                            p_have_next,
                            p_rsp_process,
                            p_put_output,
                            need_reply,
                            FALSE,
                            NULL, NULL, TPNOTIME, NULL);
}

/**
 * This is wrapper for cmd_generic_call_2,
 * This exposes flags to user.
 */
public int cmd_generic_callfl(int ndrxd_cmd, int msg_src, int msg_type,
                            command_call_t *call, size_t call_size,
                            char *reply_q,
                            mqd_t reply_queue, /* listen for answer on this! */
                            mqd_t admin_queue, /* this might be FAIL! */
                            char *admin_q_str, /* should be set! */
                            int argc, char **argv,
                            int *p_have_next,
                            int (*p_rsp_process)(command_reply_t *reply, size_t reply_len),
                            void (*p_put_output)(char *text),
                            int need_reply,
                            int flags)
{
    return cmd_generic_call_2(ndrxd_cmd, msg_src, msg_type,
                            call, call_size,
                            reply_q,
                            reply_queue,
                            admin_queue,
                            admin_q_str,
                            argc, argv,
                            p_have_next,
                            p_rsp_process,
                            p_put_output,
                            need_reply,
                            FALSE,
                            NULL, NULL, flags, NULL);
}


/**
 * Full request.
 * Including output buffers.
 */
public int cmd_generic_bufcall(int ndrxd_cmd, int msg_src, int msg_type,
                            command_call_t *call, size_t call_size,
                            char *reply_q,
                            mqd_t reply_queue, /* listen for answer on this! */
                            mqd_t admin_queue, /* this might be FAIL! */
                            char *admin_q_str, /* should be set! */
                            int argc, char **argv,
                            int *p_have_next,
                            int (*p_rsp_process)(command_reply_t *reply, size_t reply_len),
                            void (*p_put_output)(char *text),
                            int need_reply,
                            int reply_only,
                            char *rply_buf_out,             /* might be a null  */
                            int *rply_buf_out_len,          /* if above is set, then must not be null */
                            int flags,
                            int (*p_rply_request)(char *buf, long len)
        )
{
    return cmd_generic_call_2(ndrxd_cmd, msg_src, msg_type,
                            call, call_size,
                            reply_q,
                            reply_queue,
                            admin_queue,
                            admin_q_str,
                            argc, argv,
                            p_have_next,
                            p_rsp_process,
                            p_put_output,
                            need_reply,
                            reply_only,
                            rply_buf_out, 
                            rply_buf_out_len, 
                            flags,
                            p_rply_request);
}


/**
 * Call from list.
 * @param ndrxd_cmd
 * @param msg_src
 * @param msg_type
 * @param call
 * @param call_size
 * @param reply_q
 * @param reply_queue
 * @param admin_queue
 * @param admin_q_str
 * @param argc
 * @param argv
 * @param p_have_next
 * @param arglist
 * @param reply_only
 * @return
 */
extern int cmd_generic_listcall(int ndrxd_cmd, int msg_src, int msg_type,
                            command_call_t *call, size_t call_size,
                            char *reply_q,
                            mqd_t reply_queue, /* listen for answer on this! */
                            mqd_t admin_queue, /* this might be FAIL! */
                            char *admin_q_str, /* should be set! */
                            int argc, char **argv,
                            int *p_have_next,
                            gencall_args_t *arglist/* this is list of process */,
                            int reply_only)
{
        return cmd_generic_call_2(ndrxd_cmd, msg_src, msg_type,
                            call, call_size,
                            reply_q,
                            reply_queue,
                            admin_queue,
                            admin_q_str,
                            argc, argv,
                            p_have_next,
                            arglist[ndrxd_cmd].p_rsp_process,
                            arglist[ndrxd_cmd].p_put_output,
                            arglist[ndrxd_cmd].need_reply,
                            reply_only,
			    /* xadmin on solaris fails to recieve answers when ndrxd exits.. */
                            NULL, NULL, TPNOTIME | TPSIGRSTRT, NULL);
}

/**
 * Send error reply back to originator.
 * Fix this function to send error message over the bridge.
 */
public int reply_with_failure(long flags, tp_command_call_t *last_call,
            char *buf, int *len, long rcode)
{
    int ret=SUCCEED;
    char fn[] = "reply_with_failure";
    tp_command_call_t call_b;
    tp_command_call_t *call;
    char reply_to[NDRX_MAX_Q_SIZE+1] = {EOS};

    if (NULL==buf)
    {
        call = &call_b;
    }
    else
    {
        call = (tp_command_call_t *)buf;
    }
    
    memset(call, 0, sizeof(*call));
    call->command_id = ATMI_COMMAND_TPREPLY;
    call->cd = last_call->cd;
    call->cd = last_call->cd;
    call->timestamp = last_call->timestamp;
    call->callseq = last_call->callseq;
    /* Give some info which server replied - for bridge we need target put here! */
    strcpy(call->reply_to, last_call->reply_to);
    call->sysflags |=SYS_FLAG_REPLY_ERROR;
    call->rcode = rcode;
    strcpy(call->callstack, last_call->callstack);
    
    if (SUCCEED!=fill_reply_queue(call->callstack, last_call->reply_to, reply_to))
    {
        NDRX_LOG(log_error, "ATTENTION!! Failed to get reply queue");
        goto out;
    }

    if (NULL==buf)
    {
        if (SUCCEED!=(ret=generic_q_send(reply_to, (char *)call, sizeof(*call), flags)))
        {
            NDRX_LOG(log_error, "%s: Failed to send error reply back, os err: %s", fn, strerror(ret));
            goto out;
        }
    }
    else
    {
        NDRX_LOG(log_debug, "Buffer specified not sending anywhere");
    }
    

out:
    return ret;
}

/**
 * Get q attributes
 * @param q - queue name
 * @param p_att - attributes 
 * @return SUCCEED/FAIL
 */
public int ndrx_get_q_attr(char *q, struct mq_attr *p_att)
{
    int ret=SUCCEED;
    mqd_t q_descr=(mqd_t)FAIL;
    
    /* read the stats of the queue */
    if ((mqd_t)FAIL==(q_descr = ndrx_mq_open_at_wrp(q, 0)))
    {
        NDRX_LOG(log_warn, "Failed to get attribs of Q: [%s], err: %s", 
                q, strerror(errno));
        FAIL_OUT(ret);
    }

    /* read the attributes of the Q */
    if (SUCCEED!= ndrx_mq_getattr(q_descr, p_att))
    {
        NDRX_LOG(log_warn, "Failed to get attribs of Q: %d, err: %s", 
                q_descr, strerror(errno));
        FAIL_OUT(ret);
    }
    
out:
    
    if ((mqd_t)FAIL!=q_descr)
    {
        ndrx_mq_close(q_descr);
    }

    return ret;
}

/**
 * Return list of services, starting with name
 * @param start_with - service name start.
 * @return 
 */
public atmi_svc_list_t* ndrx_get_svc_list(int (*p_filter)(char *svcnm))
{
    atmi_svc_list_t *ret = NULL;
    atmi_svc_list_t *tmp;
    shm_svcinfo_t *svcinfo = (shm_svcinfo_t *) G_svcinfo.mem;
    int i;
    
    if (NULL==svcinfo)
    {
        NDRX_LOG(log_error, "shm_svcinfo memory is NULL!");
        return NULL; /* Nothing todo. */
    }
    
    /* We assume shm is OK! */
    for (i=0; i<G_max_svcs; i++)
    {
        /* we might have in old service in SHM (with no actual servers) thus 
         * skip such.
         */
        if (EOS!=SHM_SVCINFO_INDEX(svcinfo, i)->service[0] && 
                (SHM_SVCINFO_INDEX(svcinfo, i)->srvs || SHM_SVCINFO_INDEX(svcinfo, i)->csrvs) )
        {
            /* Check filter, if ok - add to list! */
            if (p_filter(SHM_SVCINFO_INDEX(svcinfo, i)->service))
            {
                if (NULL==(tmp = NDRX_CALLOC(1, sizeof(atmi_svc_list_t))))
                {
                    NDRX_LOG(log_error, "Failed to malloc %d: %s", 
                        sizeof(atmi_svc_list_t), strerror(errno));
                    
                    userlog("Failed to malloc %d: %s", 
                        sizeof(atmi_svc_list_t), strerror(errno));
                    
                    goto out;
                }
                strcpy(tmp->svcnm, SHM_SVCINFO_INDEX(svcinfo, i)->service);
                LL_APPEND(ret, tmp);
            }   
        }
    } /* for */
    
out:
    return ret;
}

/**
 * Reply back to caller
 * @param tp_call
 * @param flags
 * @param rcode Error code
 * @param reply_to_q Sender id
 */
public void ndrx_reply_with_failure(tp_command_call_t *tp_call, long flags, 
        long rcode, char *reply_to_q)
{
    int ret=SUCCEED;
    char fn[] = "ndrx_reply_with_failure";
    tp_command_call_t call;

    NDRX_LOG(log_warn, "Replying  back to [%s] with TPESVCERR", 
            tp_call->reply_to, reply_to_q);
    
    NDRX_LOG(log_error, "Dumping original call in queue:");
    ndrx_dump_call_struct(log_error, tp_call);
    
    memset(&call, 0, sizeof(call));
    call.command_id = ATMI_COMMAND_TPREPLY;
    call.cd = tp_call->cd;
    call.timestamp = tp_call->timestamp;
    call.callseq = tp_call->callseq;
    /* Give some info which server replied */
    strcpy(call.reply_to, reply_to_q);
    call.sysflags |=SYS_FLAG_REPLY_ERROR;
    /* Generate no entry, because we removed the queue
     * yeah, it might be too late for TPNOENT, but this is real error */
    call.rcode = rcode;
    
    NDRX_LOG(log_error, "Dumping error reply about to send:");
    ndrx_dump_call_struct(log_error, &call);

    if (SUCCEED!=(ret=generic_q_send(tp_call->reply_to, (char *)&call, sizeof(call), flags)))
    {
        NDRX_LOG(log_error, "%s: Failed to send error reply back, os err: %s", fn, strerror(ret));
    }
}
