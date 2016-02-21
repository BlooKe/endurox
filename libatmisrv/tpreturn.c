/* 
** tpreturn function implementation.
**
** @file tpreturn.c
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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <setjmp.h>
#include <errno.h>

#include <atmi.h>
#include <ndebug.h>
#include <tperror.h>
#include <typed_buf.h>
#include <atmi_int.h>
#include <srv_int.h>
#include <gencall.h>

#include "xa_cmn.h"
#include "userlog.h"
/*---------------------------Externs------------------------------------*/
/*---------------------------Macros-------------------------------------*/
/*---------------------------Enums--------------------------------------*/
/*---------------------------Typedefs-----------------------------------*/
/*---------------------------Globals------------------------------------*/
/*---------------------------Statics------------------------------------*/
/*---------------------------Prototypes---------------------------------*/
/**
 * Internal version to tpreturn.
 * This is
 * @param rval
 * @param rcode
 * @param data
 * @param len
 * @param flags
 */
public void	_tpreturn (int rval, long rcode, char *data, long len, long flags)
{
    int ret=SUCCEED;
    char buf[ATMI_MSG_MAX_SIZE]; /* physical place where to put the reply */
    tp_command_call_t *call=(tp_command_call_t *)buf;
    char fn[] = "_tpreturn";
    buffer_obj_t *buffer_info;
    typed_buffer_descr_t *descr;
    NDRX_LOG(log_debug, "%s enter", fn);
    long data_len;
    int return_status=0;
    char reply_to[NDRX_MAX_Q_SIZE+1] = {EOS};
    
    /* client with out last call is acceptable...! */
    if (G_atmi_conf.is_client && !G_last_call.cd)
    {
        /* this is client */
        NDRX_LOG(log_debug, "tpreturn is not available for clients!!!");
        _TPset_error_fmt(TPEPROTO, "tpreturn - not available for clients!!!");
        return; /* <<<< RETURN */
    }

    if (G_last_call.flags & TPNOREPLY)
    {
        NDRX_LOG(log_debug, "No reply expected - return to main()!, "
                "flags; %ld", G_last_call.flags);
        goto return_to_main;
    }

    memset(call, 0, sizeof(*call));

    close_open_client_connections(); /* disconnect any calls when we are clients */

    /* Set descriptor */
    call->cd = G_last_call.cd;

    if (CONV_IN_CONVERSATION==G_accepted_connection.status)
        call->cd-=MAX_CONNECTIONS;

    call->timestamp = G_last_call.timestamp;
    call->callseq = G_last_call.callseq;
    call->data_len = 0;
    call->sysflags = 0; /* reset the flags. */
    
    /* TODO: put our call node id? As source which generated reply? */

    /* Send our queue path back 
    strcpy(call->reply_to, G_atmi_conf.reply_q_str);
    */
    /* Save original reply to path, so that bridge knows what to next */
    strcpy(call->reply_to, G_last_call.reply_to);
    
    /* Mark as service failure. */
    if (TPSUCCESS!=rval)
    {
        return_status|=RETURN_SVC_FAIL;
    }
    
    
#if 0
    - moved to tmisabortonly.
            
    /* If we have global transaction & failed. Then set abort only flag */
    if (G_atmi_xa_curtx.txinfo && G_atmi_xa_curtx.txinfo->tmisabortonly)
    {
        call->sysflags|=SYS_XA_ABORT_ONLY;
    }
#endif
    
    /* work out the XA data */
    if (G_atmi_xa_curtx.txinfo)
    {
        /* Update the list  
        strcpy(call->tmknownrms, G_atmi_xa_curtx.txinfo->tmknownrms);
        strcpy(call->tmxid, G_atmi_xa_curtx.txinfo->tmxid);
         * */
        XA_TX_COPY(call, G_atmi_xa_curtx.txinfo);
    }
    
    /* prepare reply buffer */
    if ((TPFAIL==rval || TPSUCCESS==rval) && NULL!=data)
    {
        /* try convert the data */
        if (NULL==(buffer_info = find_buffer(data)))
        {
            NDRX_LOG(log_error, "Err: No buffer as %p registered in system", data);
            /* set reply fail FLAG */
            call->sysflags |=SYS_FLAG_REPLY_ERROR;
            call->rcode = TPESVCERR;
            ret=FAIL;
        }
        else
        {
            /* Convert back, if convert flags was set */
            if (SYS_SRV_CVT_ANY_SET(G_last_call.sysflags))
            {
                NDRX_LOG(log_debug, "about reverse xcvt...");
                /* Convert buffer back.. */
                if (SUCCEED!=typed_xcvt(&buffer_info, G_last_call.sysflags, TRUE))
                {
                    NDRX_LOG(log_debug, "Failed to convert buffer back to "
                            "callers format: %llx", G_last_call.sysflags);
                    userlog("Failed to convert buffer back to "
                            "callers format: %llx", G_last_call.sysflags);
                    /* set reply fail FLAG */
                    call->sysflags |=SYS_FLAG_REPLY_ERROR;
                    call->rcode = TPESVCERR;
                    ret=FAIL;
                }
            }
            
            if (FAIL!=ret)
            {
                descr = &G_buf_descr[buffer_info->type_id];
                /* build reply data here */
                if (FAIL==descr->pf_prepare_outgoing(descr, data, 
                        len, call->data, &call->data_len, flags))
                {
                    /* set reply fail FLAG */
                    call->sysflags |=SYS_FLAG_REPLY_ERROR;
                    call->rcode = TPESYSTEM;
                    ret=FAIL;
                }
                else
                {
                    call->buffer_type_id = buffer_info->type_id;
                }
            }
        }
    }
    else
    {
        /* no data in reply */
        call->data_len = 0;
    }
    call->rval = rval;
    call->rcode = rcode;

    data_len = sizeof(tp_command_call_t)+call->data_len;
    call->command_id = ATMI_COMMAND_TPREPLY;
    
    /* keep the timer from last call. */
    call->timer = G_last_call.timer;
    
    /* Get the reply order... */
    strcpy(call->callstack, G_last_call.callstack);
    if (SUCCEED!=fill_reply_queue(call->callstack, G_last_call.reply_to, reply_to))
    {
        NDRX_LOG(log_error, "ATTENTION!! Failed to get reply queue");
        goto return_to_main;
    }
    
    /* send the reply back actually */
    NDRX_LOG(log_debug, "Returning to %s cd %d, timestamp %d, callseq: %u, rval: %d, rcode: %ld",
                            reply_to, call->cd, call->timestamp, call->callseq,
                            call->rval, call->rcode);

    /* TODO: Chose the cluster node to send to, Scan the stack, to find
     * the closest reply node... We might event not to pop the stack.
     * But each node searches for closest path, from right to left.
     */
    if (FAIL==generic_q_send(reply_to, (char *)call, data_len, flags))
    {
        NDRX_LOG(log_error, "ATTENTION!! Reply to queue [%s] failed!",
                                            reply_to);
        goto return_to_main;
    }

    /* Wait for ack if we run in conversation */
    if (CONV_IN_CONVERSATION==G_accepted_connection.status)
    {
        get_ack(&G_accepted_connection, flags);

        /* If this is conversation, then we should release conversation queue */
        normal_connection_shutdown(&G_accepted_connection, FALSE);
    }

return_to_main:

    /* Hmm we can free up the data? */
    if (NULL!=data)
    {
        _tpfree(data, NULL);
    }

    /* server thread, no long jump... (thread should kill it self.)*/
    if (!(G_last_call.sysflags & SYS_SRV_THREAD))
    {        
        return_status|=RETURN_TYPE_TPRETURN;
         if (FAIL==ret)
             return_status|=RETURN_FAILED;

        if (G_libatmisrv_flags & ATMI_SRVLIB_NOLONGJUMP)
        {
            NDRX_LOG(log_debug, "%s normal return to main - no longjmp", fn);
            G_atmisrv_reply_type = return_status;
        }
        else
        {
            NDRX_LOG(log_debug, "%s about to jump to main()", fn);
            longjmp(G_server_conf.call_ret_env, return_status);
        }
    }

    return;
}

/**
 * Do asynchronus call.
 * @param svc
 * @param data
 * @param len
 * @param flags - should be managed from parent function (is it real async call
 *                  or tpcall wrapper)
 * @return SUCCEED/FAIL
 */
public void _tpforward (char *svc, char *data,
                long len, long flags)
{
    int ret=SUCCEED;
    char buf[ATMI_MSG_MAX_SIZE];
    tp_command_call_t *call=(tp_command_call_t *)buf;
    typed_buffer_descr_t *descr;
    buffer_obj_t *buffer_info;
    char fn[] = "_tpforward";
    long data_len = MAX_CALL_DATA_SIZE;
    char send_q[NDRX_MAX_Q_SIZE+1];
    long return_status=0;
    int is_bridge;
    
    NDRX_LOG(log_debug, "%s enter", fn);

    memset(call, 0, sizeof(*call)); /* have some safety net */

    /* Cannot do the forward if we are in conversation! */
    if (CONV_IN_CONVERSATION==G_accepted_connection.status ||
            have_open_connection())
    {
        _TPset_error_fmt(TPEPROTO, "tpforward no allowed for conversation server!");
    }

    if (NULL==(buffer_info = find_buffer(data)))
    {
        _TPset_error_fmt(TPEINVAL, "Buffer %p not known to system!", fn);
        ret=FAIL;
        goto out;
    }
    
    descr = &G_buf_descr[buffer_info->type_id];

    /* prepare buffer for call */
    if (SUCCEED!=descr->pf_prepare_outgoing(descr, data, len, call->data, &data_len, flags))
    {
        /* not good - error should be already set */
        ret=FAIL;
        goto out;
    }
    
    /* OK, now fill up the details */
    call->data_len = data_len;

    data_len+=sizeof(tp_command_call_t);

    call->buffer_type_id = (short)buffer_info->type_id; /* < caused core dumps! */
    strcpy(call->reply_to, G_last_call.reply_to); /* <<< main difference from call! */
    call->command_id = ATMI_COMMAND_TPCALL;

    strncpy(call->name, svc, XATMI_SERVICE_NAME_LENGTH);
    call->name[XATMI_SERVICE_NAME_LENGTH] = EOS;
    call->flags = flags;
    call->cd = G_last_call.cd; /* <<< another difference from call! */
    call->timestamp = G_last_call.timestamp;
    call->callseq = G_last_call.callseq;
    strcpy(call->callstack, G_last_call.callstack);
    
    /* work out the XA data */
    if (G_atmi_xa_curtx.txinfo)
    {
        /* Copy TX data */
        XA_TX_COPY(call, G_atmi_xa_curtx.txinfo);
    }
    
#if 0
    - moved ot tmisabortonly.
    /* If we have global transaction & failed. Then set abort only flag 
     * On the other side, receiver should mark it's global tx too
     * as abort only.
     */
    if (G_atmi_xa_curtx.txinfo && G_atmi_xa_curtx.txinfo->tmisabortonly)
    {
        call->sysflags|=SYS_XA_ABORT_ONLY;
    }
#endif
    
    /* Want to keep original call time... */
    memcpy(&call->timer, &G_last_call.timer, sizeof(call->timer));
    
    /* Hmm we can free up the data? - do it here because we still need buffer_info!*/
    if (NULL!=data)
    {
        _tpfree(data, NULL);
    }

    /* Check is service available? */
    if (FALSE==ndrx_shm_get_svc(call->name, send_q, &is_bridge))
    {
        NDRX_LOG(log_error, "Service is not available %s by shm", 
                call->name);
        ret=FAIL;
        _TPset_error_fmt(TPENOENT, "%s: Service is not available %s by shm", 
                fn, call->name);
                /* we should reply back, that call failed, so that client does not wait */
        reply_with_failure(flags, &G_last_call, NULL, NULL, TPESVCERR);
        goto out;
    }
    NDRX_LOG(log_debug, "Forwarding cd %d, timestamp %d, callseq %u to %s, buffer_type_id %hd",
                    call->cd, call->timestamp, call->callseq, send_q, call->buffer_type_id);
        
    if (SUCCEED!=(ret=generic_q_send(send_q, (char *)call, data_len, flags)))
    {
        /* reply FAIL back to caller! */
        int err;

        /* basically we override some conditions here! */
        if (ENOENT==ret)
        {
            err=TPENOENT;
        }
        else
        {
            CONV_ERROR_CODE(ret, err);
        }

        _TPset_error_fmt(err, "%s: Failed to send, os err: %s", fn, strerror(ret));
        ret=FAIL;

        /* we should reply back, that call failed, so that client does not wait */
        reply_with_failure(flags, &G_last_call, NULL, NULL, TPESVCERR);
    }

out:
    NDRX_LOG(log_debug, "%s return %d (information only)", fn, ret);

    /* server thread, no long jump... (thread should kill it self.)*/
    if (!(G_last_call.sysflags & SYS_SRV_THREAD))
    {
        return_status|=RETURN_TYPE_TPFORWARD;
        if (FAIL==ret)
            return_status|=RETURN_FAILED;
        
        if (G_libatmisrv_flags & ATMI_SRVLIB_NOLONGJUMP)
        {
            NDRX_LOG(log_debug, "%s normal return to main - no longjmp", fn);
            G_atmisrv_reply_type = return_status;
        }
        else 
        {
            NDRX_LOG(log_debug, "%s longjmp to main()", fn);
            longjmp(G_server_conf.call_ret_env, return_status);
        }
    }

out_no_jump:
    NDRX_LOG(log_debug, "%s NOT JUMPING!", fn, ret);

    return;
}

/**
 * Task is copied to thread.
 * The main thread goes back to polling.
 */
public void _tpcontinue (void)
{
    long return_status=0;
    return_status|=RETURN_TYPE_THREAD;
    NDRX_LOG(log_debug, "Long jumping to continue!");
    longjmp(G_server_conf.call_ret_env, return_status);
    
    NDRX_LOG(log_debug, "doing nothing after long jmp!!!!");
}
