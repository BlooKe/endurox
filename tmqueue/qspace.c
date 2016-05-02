/* 
** Memory based structures for Q.
**
** @file tmqueue.c
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

#include <ndebug.h>
#include <atmi.h>
#include <atmi_int.h>
#include <typed_buf.h>
#include <ndrstandard.h>
#include <ubf.h>
#include <Exfields.h>

#include <exnet.h>
#include <ndrxdcmn.h>
#include <uuid/uuid.h>

#include "tmqd.h"
#include "../libatmisrv/srv_int.h"
#include "tperror.h"
#include "userlog.h"
#include <xa_cmn.h>
#include "thpool.h"
#include "nstdutil.h"
#include "tmqueue.h"
/*---------------------------Externs------------------------------------*/
/*---------------------------Macros-------------------------------------*/
#define MAX_TOKEN_SIZE          64 /* max key=value buffer size of qdef element */

#define TMQ_QC_NAME             "name"
#define TMQ_QC_SVCNM            "svcnm"
#define TMQ_QC_TRIES            "tries"
#define TMQ_QC_AUTOQ            "autoq"
#define TMQ_QC_WAITINIT         "waitinit"
#define TMQ_QC_WAITRETRY        "waitretry"
#define TMQ_QC_WAITRETRYINC     "waitretryinc"
#define TMQ_QC_WAITRETRYMAX     "waitretrymax"
#define TMQ_QC_MEMONLY          "memonly"

/*---------------------------Enums--------------------------------------*/
/*---------------------------Typedefs-----------------------------------*/
/*---------------------------Globals------------------------------------*/

/* Handler for MSG Hash. */
public tmq_memmsg_t *G_msgid_hash;

/* Handler for Q hash */
public tmq_qhash_t *G_qhash;

/*
 * Any public operations must be locked
 */
MUTEX_LOCKDECL(M_q_lock);

/* Configuration section */
public tmq_qconfig_t *G_qconf; 

MUTEX_LOCKDECL(M_msgid_gen_lock); /* Thread locking for xid generation  */
/*---------------------------Statics------------------------------------*/
/*---------------------------Prototypes---------------------------------*/

/**
 * Setup queue header
 * @param hdr header to setup
 * @param qname queue name
 */
public int tmq_setup_cmdheader_newmsg(tmq_cmdheader_t *hdr, char *qname, 
        short srvid, short nodeid, char *qspace)
{
    int ret = SUCCEED;
    
    strcpy(hdr->qspace, qspace);
    strcpy(hdr->qname, qname);
    hdr->command_code = TMQ_STORCMD_NEWMSG;
    strncpy(hdr->magic, TMQ_MAGIC, TMQ_MAGIC_LEN);
    hdr->nodeid = nodeid;
    hdr->srvid = srvid;
    
    tmq_msgid_gen(hdr->msgid);
    
out:
    return ret;
}


/**
 * Generate new transaction id, native form (byte array)
 * Note this initializes the msgid.
 * @param msgid value to return
 */
public void tmq_msgid_gen(char *msgid)
{
    uuid_t uuid_val;
    short node_id = (short) G_atmi_env.our_nodeid;
    short srv_id = (short) G_srv_id;
   
    memset(msgid, 0, TMMSGIDLEN);
    
    /* Do the locking, so that we get unique xids... */
    MUTEX_LOCK_V(M_msgid_gen_lock);
    uuid_generate(uuid_val);
    MUTEX_UNLOCK_V(M_msgid_gen_lock);
    
    memcpy(msgid, uuid_val, sizeof(uuid_t));
    /* Have an additional infos for transaction id... */
    memcpy(msgid  
            +sizeof(uuid_t)  
            ,(char *)&(node_id), sizeof(short));
    memcpy(msgid  
            +sizeof(uuid_t) 
            +sizeof(short)
            ,(char *)&(srv_id), sizeof(short));    
    
    NDRX_LOG(log_error, "MSGID: struct size: %d", sizeof(uuid_t)+sizeof(short)+ sizeof(short));
}


/**
 * Load the key config parameter
 * @param qconf
 * @param key
 * @param value
 */
private int load_param(tmq_qconfig_t * qconf, char *key, char *value)
{
    int ret = SUCCEED; 
    
    NDRX_LOG(log_info, "loading q param: [%s] = [%s]", key, value);
    
    /* - Not used.
    if (0==strcmp(key, TMQ_QC_NAME))
    {
        strncpy(qconf->name, value, sizeof(qconf->name)-1);
        qconf->name[sizeof(qconf->name)-1] = EOS;
    }
    else  */
    if (0==strcmp(key, TMQ_QC_SVCNM))
    {
        strncpy(qconf->svcnm, value, sizeof(qconf->svcnm)-1);
        qconf->svcnm[sizeof(qconf->svcnm)-1] = EOS;
    }
    else if (0==strcmp(key, TMQ_QC_TRIES))
    {
        int ival = atoi(value);
        if (!nstdutil_isint(value) || ival < 0)
        {
            NDRX_LOG(log_error, "Invalid value [%s] for key [%s] (must be int>=0)", 
                    value, key);
            FAIL_OUT(ret);
        }
        
        qconf->tries = ival;
    }
    else if (0==strcmp(key, TMQ_QC_AUTOQ))
    {
        qconf->autoq = FALSE;
        
        if (value[0]=='y' || value[0]=='Y')
        {
            qconf->autoq = TRUE;
        }
    }
    else if (0==strcmp(key, TMQ_QC_WAITINIT))
    {
        int ival = atoi(value);
        if (!nstdutil_isint(value) || ival < 0)
        {
            NDRX_LOG(log_error, "Invalid value [%s] for key [%s] (must be int>=0)", 
                    value, key);
            FAIL_OUT(ret);
        }
        
        qconf->waitinit = ival;
    }
    else if (0==strcmp(key, TMQ_QC_WAITRETRY))
    {
        int ival = atoi(value);
        if (!nstdutil_isint(value) || ival < 0)
        {
            NDRX_LOG(log_error, "Invalid value [%s] for key [%s] (must be int>=0)", 
                    value, key);
            FAIL_OUT(ret);
        }
        
        qconf->waitretry = ival;
    }
    else if (0==strcmp(key, TMQ_QC_WAITRETRYINC))
    {
        int ival = atoi(value);
        if (!nstdutil_isint(value) || ival < 0)
        {
            NDRX_LOG(log_error, "Invalid value [%s] for key [%s] (must be int>=0)", 
                    value, key);
            FAIL_OUT(ret);
        }
        
        qconf->waitretryinc = ival;
    }
    else if (0==strcmp(key, TMQ_QC_WAITRETRYMAX))
    {
        int ival = atoi(value);
        if (!nstdutil_isint(value) || ival < 0)
        {
            NDRX_LOG(log_error, "Invalid value [%s] for key [%s] (must be int>=0)", 
                    value, key);
            FAIL_OUT(ret);
        }
        
        qconf->waitretrymax = ival;
    }
    else if (0==strcmp(key, TMQ_QC_MEMONLY))
    {
        qconf->memonly = FALSE;
        if (value[0]=='y' || value[0]=='Y')
        {
            qconf->memonly = TRUE;
        }
    }
    else
    {
        NDRX_LOG(log_error, "Unknown Q config setting = [%s]", key);
        FAIL_OUT(ret);
    }
    
out:

    return ret;
}

/**
 * Get Q config by name
 * @param name queue name
 * @return NULL or ptr to q config.
 */
private tmq_qconfig_t * tmq_qconf_get(char *qname)
{
    tmq_qconfig_t *ret = NULL;
    
    HASH_FIND_STR( G_qconf, qname, ret);
    
    return ret;
}

/**
 * Return Q config with default if not found
 * @param qname qname
 * @return  NULL or ptr to config
 */
private tmq_qconfig_t * tmq_qconf_get_with_default(char *qname)
{
    
    tmq_qconfig_t * ret = tmq_qconf_get(qname);
    
    if  (NULL==ret)
    {
        NDRX_LOG(log_warn, "Q config [%s] not found, trying to default to [%s]", 
                qname, TMQ_DEFAULT_Q);
        if (NULL==(ret = tmq_qconf_get(TMQ_DEFAULT_Q)))
        {
            NDRX_LOG(log_error, "Default Q config [%s] not found!", TMQ_DEFAULT_Q);
            userlog("Default Q config [%s] not found! Please add !", TMQ_DEFAULT_Q);
        }
    }
            
    return ret;
}

/**
 * Remove queue probably then existing messages will fall back to default Q
 * @param name
 * @return 
 */
private int tmq_qconf_delete(char *qname)
{
    int ret = SUCCEED;
    tmq_qconfig_t *qconf;
    
    if (NULL!=(qconf=tmq_qconf_get(qname)))
    {
        HASH_DEL( G_qconf, qconf);
        free(qconf);
    }
    else
    {
        NDRX_LOG(log_warn, "[%s] - queue not found", qname);
    }
    
out:
    return ret;
}

/**
 * Reload the config of queues
 * @param cf
 * @return 
 */
public int tmq_reload_conf(char *cf)
{
    FILE *f = NULL;
    char *line = NULL;
    size_t len = 0;
    int ret = SUCCEED;
    ssize_t read;
    if (NULL==(f=fopen(cf, "r")))
    {
        NDRX_LOG(log_error, "Failed to open [%s]:%s", cf, strerror(errno));
        FAIL_OUT(ret);
    }
    
    while (FAIL!=(read = getline(&line, &len, f))) 
    {
        nstdutil_str_strip(line, " \n\r\t");
        
        /* Ignore comments & newlines */
        if ('#'==*line || EOS==*line)
        {
            continue;
        }
        
        if (SUCCEED!=tmq_qconf_addupd(line))
        {
            FAIL_OUT(ret);
        }
    }
    free(line);
    
    
out:
    
    if (NULL!=f)
    {
        fclose(f);
    }
}

/**
 * Add queue definition. Support also update
 * We shall support Q update too...
 * Syntax: -q VISA,svcnm=VISAIF,autoq=y|n,waitinit=30,waitretry=10,waitretryinc=5,waitretrymax=40,memonly=y|n
 * @param qdef
 * @return  SUCCEED/FAIL
 */
public int tmq_qconf_addupd(char *qconfstr)
{
    tmq_qconfig_t * qconf;
    tmq_qconfig_t * dflt;
    char * p;
    char * p2;
    int got_default = FALSE;
    int qupdate = FALSE;
    char buf[MAX_TOKEN_SIZE];
    int ret = SUCCEED;
    
    NDRX_LOG(log_info, "Add new Q: [%s]", qconfstr);
    
    if (NULL==qconf)
    {
        NDRX_LOG(log_error, "Malloc failed for tmq_qconfig_t!");
        FAIL_OUT(ret);
    }
    
    MUTEX_LOCK_V(M_q_lock);
    
    p = strtok (qconfstr,",");
    
    if (NULL!=p)
    {
        NDRX_LOG(log_info, "Got token: [%s]", p);
        strncpy(buf, p, sizeof(qconf->qname)-1);
        buf[sizeof(qconf->qname)-1] = EOS;
        
        NDRX_LOG(log_debug, "Q name: [%s]", buf);
        
        if (NULL== (qconf = tmq_qconf_get(buf)))
        {
            NDRX_LOG(log_info, "Q not found, adding: [%s]", buf);
            qconf= calloc(1, sizeof(tmq_qconfig_t));
                    
            /* Try to load initial config from @ (TMQ_DEFAULT_Q) Q */
            if (NULL!=(dflt=tmq_qconf_get(TMQ_DEFAULT_Q)))
            {
                memcpy(qconf, dflt, sizeof(*dflt));
                got_default = TRUE;
            }
            
            strcpy(qconf->qname, buf);
        }
        else
        {
            NDRX_LOG(log_info, "Q found, updating: [%s]", buf);
            qupdate = TRUE;
        }
    }
    else
    {
        NDRX_LOG(log_error, "Missing Q name");
        FAIL_OUT(ret);
    }
    
    p = strtok (NULL, ","); /* continue... */
    
    while (p != NULL)
    {
        NDRX_LOG(log_info, "Got pair [%s]", p);
        
        strncpy(buf, p, sizeof(buf)-1);
        buf[sizeof(buf)-1] = EOS;
        
        p2 = strchr(buf, '=');
        
        if (NULL == p2)
        {
            NDRX_LOG(log_error, "Invalid key=value token [%s] expected '='", buf);
            
            userlog("Error defining queue (%s) expected in '=' in token (%s)", 
                    qconfstr, buf);
            FAIL_OUT(ret);
        }
        *p2 = EOS;
        p2++;
        
        if (EOS==*p2)
        {
            NDRX_LOG(log_error, "Empty value for token [%s]", buf);
            userlog("Error defining queue (%s) invalid value for token (%s)", 
                    qconfstr, buf);
            FAIL_OUT(ret);
        }
        
        /*
         * Load the value into structure
         */
        if (SUCCEED!=load_param(qconf, buf, p2))
        {
            NDRX_LOG(log_error, "Failed to load token [%s]/[%s]", buf, p2);
            userlog("Error defining queue (%s) failed to load token [%s]/[%s]", 
                    buf, p2);
            FAIL_OUT(ret);
        }
        
        p = strtok (NULL, ",");
    }
    
    /* Validate the config... */
    
    if (0==strcmp(qconf->qname, TMQ_DEFAULT_Q) && got_default)
    {
        NDRX_LOG(log_error, "Missing [%s] param", TMQ_QC_NAME);
        /* TODO: Return some diagnostics... => EX_QDIAGNOSTIC invalid qname */
        FAIL_OUT(ret);
    }
    /* If autoq, then service must be set. */

    if (!qupdate)
    {
        HASH_ADD_STR( G_qconf, qname, qconf );
    }
    
out:

    /* kill the record if invalid. */
    if (SUCCEED!=ret && NULL!=qconf && !qupdate)
    {
        NDRX_LOG(log_warn, "qconf -> free");
        free(qconf);
    }

    MUTEX_UNLOCK_V(M_q_lock);
    return ret;

}

/**
 * Get QHASH record for particular q
 * @param qname
 * @return 
 */
private tmq_qhash_t * tmq_qhash_get(char *qname)
{
    tmq_qhash_t * ret = NULL;
   
    HASH_FIND_STR( G_qhash, qname, ret);    
    
    return ret;
}

/**
 * Get new qhash entry + add it to hash.
 * @param qname
 * @return 
 */
private tmq_qhash_t * tmq_qhash_new(char *qname)
{
    tmq_qhash_t * ret = calloc(1, sizeof(tmq_qhash_t));
    
    if (NULL==ret)
    {
        NDRX_LOG(log_error, "Failed to alloc tmq_qhash_t: %s", strerror(errno));
        userlog("Failed to alloc tmq_qhash_t: %s", strerror(errno));
    }
    
    HASH_ADD_STR( G_qhash, qname, ret );
    
    return ret;
}

/**
 * Add message to queue
 * Think about TPQLOCKED so that other thread does not get message in progress..
 * In two phase commit mode, we need to unlock message only when it is enqueued on disk.
 * 
 * @param msg
 * @return 
 */
public int tmq_msg_add(tmq_msg_t *msg)
{
    int ret = SUCCEED;
    tmq_qhash_t *qhash;
    tmq_memmsg_t *mmsg = calloc(1, sizeof(tmq_memmsg_t));
    tmq_qconfig_t * qconf;
    char msgid_str[TMMSGIDLEN_STR+1];
    
    MUTEX_LOCK_V(M_q_lock);
    
    qhash = tmq_qhash_get(msg->hdr.qname);
    qconf = tmq_qconf_get_with_default(msg->hdr.qname);
    
    if (NULL==mmsg)
    {
        NDRX_LOG(log_error, "Failed to alloc tmq_memmsg_t: %s", strerror(errno));
        userlog("Failed to alloc tmq_memmsg_t: %s", strerror(errno));
        FAIL_OUT(ret);
    }
    
    if (NULL==qconf)
    {
        NDRX_LOG(log_error, "queue config not found! Cannot enqueue!");
        userlog("queue config not found! Cannot enqueue!");
        FAIL_OUT(ret);
    }
    
    memcpy(&mmsg->msg, msg, sizeof(*msg));
    
    /* Get the entry for hash of queues: */
    if (NULL==qhash && NULL==(qhash=tmq_qhash_new(msg->hdr.qname)))
    {
        NDRX_LOG(log_error, "Failed to get/create qhash entry for Q [%s]", 
                msg->hdr.qname);
        FAIL_OUT(ret);
    }
    
    /* Add the message to end of the queue */
    DL_APPEND(qhash->q, mmsg);    
    
    /* Add the hash of IDs */
    HASH_ADD_STR( G_msgid_hash, msgid_str, mmsg);
    
    /* Decide do we need to add the msg to disk?! 
     * Needs to send a command to XA sub-system to prepare msg/command to disk,
     * if it is not memory only.
     * So next step todo is to write xa command handler & dumping commands to disk.
     */
    if (!qconf->memonly)
    {
        if (SUCCEED!=tmq_storage_write_cmd_newmsg(&mmsg->msg))
        {
            NDRX_LOG(log_error, "Failed to add message to persistent store!");
            FAIL_OUT(ret);
        }
    }
    else
    {
        NDRX_LOG(log_info, "Mem only Q, not persisting.");   
    }
    
    NDRX_LOG(log_debug, "Message with id [%s] successfully enqueued to [%s] queue",
            tmq_msgid_serialize(msg->hdr.msgid, msgid_str), msg->hdr.qname);
    
out:

    if (SUCCEED!=ret && mmsg!=NULL)
    {
        free(mmsg);
    }

    MUTEX_UNLOCK_V(M_q_lock);

    return ret;
}

/**
 * Get the fifo message from Q
 * @param qname queue to lookup.
 * @return NULL (no msg), or ptr to msg
 */
public tmq_msg_t * tmq_msg_dequeue_fifo(char *qname)
{
    tmq_qhash_t *qhash;
    tmq_memmsg_t *node;
    tmq_msg_t * ret = NULL;
    union tmq_block block;
    char msgid_str[TMMSGIDLEN_STR+1];
    
    NDRX_LOG(log_debug, "FIFO dequeue for [%s]", qname);
    MUTEX_LOCK_V(M_q_lock);
    
    /* Find the non locked message in memory */
    
    /* Lock the message for current thread. 
     * The thread will later issue xA command either:
     * - Increase counter
     * - Remove the message.
     */
    if (NULL==(qhash = tmq_qhash_get(qname)))
    {
        NDRX_LOG(log_warn, "Q [%s] is NULL/empty", qname);
        goto out;
    }
    
    /* Start from first one & loop over the list while 
     * - we get to the first non-locked message
     * - or we get to the end with no msg, then return FAIL.
     */
    node = qhash->q;
    
    do
    {
        if (!node->msg.lockthreadid)
        {
            ret = &node->msg;
            break;
        }
        node = node->next;
    }
    while (NULL!=node && node->next!=qhash->q->next);
    
    if (NULL==ret)
    {
        NDRX_LOG(log_warn, "Q [%s] is empty or all msgs locked", qname);
        goto out;
    }
    
    /* Write some stuff to log */
    
    tmq_msgid_serialize(ret->hdr.msgid, msgid_str);
    NDRX_LOG(log_info, "Dequeued message: [%s]", msgid_str);
    NDRX_DUMP(log_debug, "Dequeued message", ret->msg, ret->len);
    
    /* Lock the message */
    ret->lockthreadid = ndrx_gettid();
    
    /* Issue command for msg remove */
    memset(&block, 0, sizeof(block));    
    memcpy(&block.hdr, &ret->hdr, sizeof(ret->hdr));
    
    block.hdr.command_code = TMQ_STORCMD_DEL;
    
            
    if (SUCCEED!=tmq_storage_write_cmd_block(&block, "Removing dequeued message"))
    {
        NDRX_LOG(log_error, "Failed to remove msg...");
        /* unlock msg... */
        ret->lockthreadid = 0;
        ret = NULL;
        goto out;
    }
    
out:
    MUTEX_UNLOCK_V(M_q_lock);
    return ret;
}

/**
 * Get message by msgid
 * TODO: locking...
 * @param msgid
 * @return 
 */
private tmq_memmsg_t* tmq_get_msg_by_msgid(char *msgid_str)
{
    tmq_memmsg_t *ret;
    
    HASH_FIND_STR( G_msgid_hash, msgid_str, ret);
    
    return ret;
}

/**
 * Remove mem message
 * TODO: Needs semaphore locking.
 * @param msg
 */
private void tmq_remove_msg(tmq_memmsg_t *mmsg)
{
    char msgid_str[TMMSGIDLEN_STR+1];   
    tmq_msgid_serialize(mmsg->msg.hdr.msgid, msgid_str);
    
    tmq_qhash_t *qhash = tmq_qhash_get(mmsg->msg.hdr.qname);
    
    NDRX_LOG(log_info, "Removing msgid [%s] from [%s] q", msgid_str, mmsg->msg.hdr.qname);
    if (NULL!=qhash)
    {
        /* Add the message to end of the queue */
        DL_DELETE(qhash->q, mmsg);    
    }
    
    /* Add the hash of IDs */
    HASH_DEL( G_msgid_hash, mmsg);
}

/**
 * Unlock message by updated block
 * We can:
 * - update content + unlock
 * - or remove the message
 * @param p_b
 * @return 
 */
public int tmq_unlock_msg(union tmq_upd_block *b)
{
    int ret = SUCCEED;
    char msgid_str[TMMSGIDLEN_STR+1];
    tmq_memmsg_t* mmsg;
    
    tmq_msgid_serialize(b->hdr.msgid, msgid_str);
    
    NDRX_LOG(log_info, "Unlocking/updating: %s", msgid_str);
    
    MUTEX_LOCK_V(M_q_lock);
    
    mmsg = tmq_get_msg_by_msgid(msgid_str);
    
    if (NULL==mmsg)
    {   
        NDRX_LOG(log_error, "Message not found: [%s] - no update", msgid_str);
        FAIL_OUT(ret);
    }
    
    switch (b->hdr.command_code)
    {
        case TMQ_STORCMD_DEL:
            NDRX_LOG(log_info, "Removing message...");
            tmq_remove_msg(mmsg);
            mmsg = NULL;
            break;
        case TMQ_STORCMD_UPD:
            UPD_MSG((&mmsg->msg), (&b->upd));
            mmsg->msg.lockthreadid = 0;
        /* And still we want unblock: */
        case TMQ_STORCMD_NEWMSG:
        case TMQ_STORCMD_UNLOCK:
            NDRX_LOG(log_info, "Unlocking message...");
            mmsg->msg.lockthreadid = 0;
            break;
        default:
            NDRX_LOG(log_info, "Unknown command [%c]", b->hdr.command_code);
            FAIL_OUT(ret);
            break; 
    }
    
out:
    MUTEX_UNLOCK_V(M_q_lock);
    return ret;
}

