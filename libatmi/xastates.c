/* 
** State transition handling of XA transactions
**
** @file xastates.c
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
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <memory.h>
#include <errno.h>
#include <dlfcn.h>

#include <atmi.h>
#include <atmi_shm.h>
#include <ndrstandard.h>
#include <ndebug.h>
#include <ndrxd.h>
#include <ndrxdcmn.h>
#include <userlog.h>
#include <xa_cmn.h>
/*---------------------------Externs------------------------------------*/
/*---------------------------Macros-------------------------------------*/
/*---------------------------Enums--------------------------------------*/
/*---------------------------Typedefs-----------------------------------*/
/*---------------------------Globals------------------------------------*/
/*---------------------------Statics------------------------------------*/

/**
 * Static branch driver
 * We will have two drives of all of this:
 * 1. Get new RM status (driven by current stage, status, operation and outcome)
 * 2. Get new TX state (Driven by current TX stage, and RM new status)
 * And the for file TX outcome we should select the stage with lower number...
 */
public rmstatus_driver_t G_rm_status_driver[] =
{  
    /* Driving of the Preparing: */
    {XA_TX_STAGE_PREPARING, XA_RM_STATUS_ACTIVE, XA_OP_PREPARE, XA_OK,     XA_OK,     XA_RM_STATUS_PREP,          XA_TX_STAGE_COMMITTING},
    {XA_TX_STAGE_PREPARING, XA_RM_STATUS_ACTIVE, XA_OP_PREPARE, XA_RDONLY, XA_RDONLY, XA_RM_STATUS_COMMITTED_RO,  XA_TX_STAGE_COMMITTED},
    {XA_TX_STAGE_PREPARING, XA_RM_STATUS_ACTIVE, XA_OP_PREPARE, XA_RBBASE, XA_RBEND,  XA_RM_STATUS_ABORTED,       XA_TX_STAGE_ABORTING},
    {XA_TX_STAGE_PREPARING, XA_RM_STATUS_ACTIVE, XA_OP_PREPARE, XAER_RMERR,XAER_RMERR,XA_RM_STATUS_ACTIVE,        XA_TX_STAGE_PREPARING},
    /* If no transaction, then assume committed, read only: */
    {XA_TX_STAGE_PREPARING, XA_RM_STATUS_ACTIVE, XA_OP_PREPARE, XAER_NOTA, XAER_NOTA, XA_RM_STATUS_COMMITTED_RO,  XA_TX_STAGE_COMMITTING},
    {XA_TX_STAGE_PREPARING, XA_RM_STATUS_ACTIVE, XA_OP_PREPARE, XAER_INVAL,XAER_INVAL,XA_RM_STATUS_ACTIVE,        XA_TX_STAGE_ABORTING},
    /* Driving of the COMMITTING */
    {XA_TX_STAGE_COMMITTING, XA_RM_STATUS_PREP,   XA_OP_COMMIT,  XA_OK,     XA_OK,     XA_RM_STATUS_COMMITTED,     XA_TX_STAGE_COMMITTED},
    {XA_TX_STAGE_COMMITTING, XA_RM_STATUS_PREP,   XA_OP_COMMIT,  XAER_RMERR,XAER_RMERR,XA_RM_STATUS_COMMIT_HAZARD, XA_TX_STAGE_COMMITTED_HAZARD},
    {XA_TX_STAGE_COMMITTING, XA_RM_STATUS_PREP,   XA_OP_COMMIT,  XA_RETRY,  XA_RETRY,  XA_RM_STATUS_PREP,          XA_TX_STAGE_COMMITTING},
    {XA_TX_STAGE_COMMITTING, XA_RM_STATUS_PREP,   XA_OP_COMMIT,  XA_HEURHAZ,XA_HEURHAZ,XA_RM_STATUS_COMMIT_HAZARD, XA_TX_STAGE_COMMITTED_HAZARD},
    {XA_TX_STAGE_COMMITTING, XA_RM_STATUS_PREP,   XA_OP_COMMIT,  XA_HEURCOM,XA_HEURCOM,XA_RM_STATUS_COMMIT_HEURIS, XA_TX_STAGE_COMMITTED_HEURIS},
    {XA_TX_STAGE_COMMITTING, XA_RM_STATUS_PREP,   XA_OP_COMMIT,  XA_HEURRB,XA_HEURRB,  XA_RM_STATUS_ABORT_HEURIS,  XA_TX_STAGE_COMMITTED_HEURIS},
    /* If only one RM, then we might switch back to aborted: */
    {XA_TX_STAGE_COMMITTING, XA_RM_STATUS_PREP,   XA_OP_COMMIT,  XA_HEURMIX,XA_HEURMIX,XA_RM_STATUS_COMMIT_HEURIS, XA_TX_STAGE_ABORTED},
    {XA_TX_STAGE_COMMITTING, XA_RM_STATUS_PREP,   XA_OP_COMMIT,  XA_RBBASE, XA_RBEND,  XA_RM_STATUS_ABORTED,       XA_TX_STAGE_ABORTED},
    {XA_TX_STAGE_COMMITTING, XA_RM_STATUS_PREP,   XA_OP_COMMIT,  XAER_NOTA, XAER_NOTA, XA_RM_STATUS_COMMITTED_RO,  XA_TX_STAGE_COMMITTED},
    /* Aborting:  */
    {XA_TX_STAGE_ABORTING, XA_RM_STATUS_ACTIVE, XA_OP_ROLLBACK, XA_OK,     XA_OK,     XA_RM_STATUS_ABORTED,        XA_TX_STAGE_ABORTED},
    {XA_TX_STAGE_ABORTING, XA_RM_STATUS_PREP,   XA_OP_ROLLBACK, XA_OK,     XA_OK,     XA_RM_STATUS_ABORTED,        XA_TX_STAGE_ABORTED},
    
    {XA_TX_STAGE_ABORTING, XA_RM_STATUS_ACTIVE, XA_OP_ROLLBACK, XA_RDONLY, XA_RDONLY, XA_RM_STATUS_ABORTED,        XA_TX_STAGE_ABORTED},
    {XA_TX_STAGE_ABORTING, XA_RM_STATUS_PREP,   XA_OP_ROLLBACK, XA_RDONLY, XA_RDONLY, XA_RM_STATUS_ABORTED,        XA_TX_STAGE_ABORTED},
    
    {XA_TX_STAGE_ABORTING, XA_RM_STATUS_ACTIVE, XA_OP_ROLLBACK, XAER_RMERR, XAER_RMERR,XA_RM_STATUS_ABORTED,       XA_TX_STAGE_ABORTED},
    {XA_TX_STAGE_ABORTING, XA_RM_STATUS_PREP,   XA_OP_ROLLBACK, XAER_RMERR, XAER_RMERR,XA_RM_STATUS_ABORTED,       XA_TX_STAGE_ABORTED},
    
    {XA_TX_STAGE_ABORTING, XA_RM_STATUS_ACTIVE, XA_OP_ROLLBACK, XAER_RMERR, XAER_RMERR,XA_RM_STATUS_ABORTED,       XA_TX_STAGE_ABORTED},
    {XA_TX_STAGE_ABORTING, XA_RM_STATUS_PREP,   XA_OP_ROLLBACK, XAER_RMERR, XAER_RMERR,XA_RM_STATUS_ABORTED,       XA_TX_STAGE_ABORTED},
    
    {XA_TX_STAGE_ABORTING, XA_RM_STATUS_ACTIVE, XA_OP_ROLLBACK, XA_HEURHAZ, XA_HEURHAZ,XA_RM_STATUS_ABORT_HAZARD,  XA_TX_STAGE_ABORTED_HAZARD},
    {XA_TX_STAGE_ABORTING, XA_RM_STATUS_PREP,   XA_OP_ROLLBACK, XA_HEURHAZ, XA_HEURHAZ,XA_RM_STATUS_ABORT_HAZARD,  XA_TX_STAGE_ABORTED_HAZARD},
    
    {XA_TX_STAGE_ABORTING, XA_RM_STATUS_ACTIVE, XA_OP_ROLLBACK, XA_HEURRB, XA_HEURRB,  XA_RM_STATUS_ABORT_HEURIS,  XA_TX_STAGE_ABORTED_HEURIS},
    {XA_TX_STAGE_ABORTING, XA_RM_STATUS_PREP,   XA_OP_ROLLBACK, XA_HEURRB, XA_HEURRB,  XA_RM_STATUS_ABORT_HEURIS,  XA_TX_STAGE_ABORTED_HEURIS},
    
    {XA_TX_STAGE_ABORTING, XA_RM_STATUS_ACTIVE, XA_OP_ROLLBACK, XA_HEURCOM, XA_HEURCOM,XA_RM_STATUS_COMMIT_HEURIS, XA_TX_STAGE_COMMITTED_HEURIS},
    {XA_TX_STAGE_ABORTING, XA_RM_STATUS_PREP,   XA_OP_ROLLBACK, XA_HEURCOM, XA_HEURCOM,XA_RM_STATUS_COMMIT_HEURIS, XA_TX_STAGE_COMMITTED_HEURIS},
    
    {XA_TX_STAGE_ABORTING, XA_RM_STATUS_ACTIVE, XA_OP_ROLLBACK, XA_HEURMIX, XA_HEURMIX,XA_RM_STATUS_ABORT_HEURIS,  XA_TX_STAGE_ABORTED_HEURIS},
    {XA_TX_STAGE_ABORTING, XA_RM_STATUS_PREP,   XA_OP_ROLLBACK, XA_HEURMIX, XA_HEURMIX,XA_RM_STATUS_ABORT_HEURIS,  XA_TX_STAGE_ABORTED_HEURIS},
    
    {XA_TX_STAGE_ABORTING, XA_RM_STATUS_ACTIVE, XA_OP_ROLLBACK, XAER_RMFAIL, XAER_RMFAIL,XA_RM_STATUS_ACTIVE,      XA_TX_STAGE_ABORTING},
    {XA_TX_STAGE_ABORTING, XA_RM_STATUS_PREP,   XA_OP_ROLLBACK, XAER_RMFAIL, XAER_RMFAIL,XA_RM_STATUS_PREP,        XA_TX_STAGE_ABORTING},
    
    {XA_TX_STAGE_ABORTING, XA_RM_STATUS_ACTIVE, XA_OP_ROLLBACK, XAER_NOTA, XAER_NOTA,    XA_RM_STATUS_ABORTED,     XA_TX_STAGE_ABORTED},
    {XA_TX_STAGE_ABORTING, XA_RM_STATUS_PREP,   XA_OP_ROLLBACK, XAER_NOTA, XAER_NOTA,    XA_RM_STATUS_ABORTED,     XA_TX_STAGE_ABORTED},
    
    {XA_TX_STAGE_ABORTING, XA_RM_STATUS_ACTIVE, XA_OP_ROLLBACK, XAER_RMFAIL, XAER_RMFAIL,XA_RM_STATUS_ACTIVE,      XA_TX_STAGE_ABORTING},
    {XA_TX_STAGE_ABORTING, XA_RM_STATUS_PREP,   XA_OP_ROLLBACK, XAER_RMFAIL, XAER_RMFAIL,XA_RM_STATUS_PREP,        XA_TX_STAGE_ABORTING},

    {XA_TX_STAGE_ABORTING, XA_RM_STATUS_ACTIVE, XA_OP_ROLLBACK, XAER_PROTO, XAER_PROTO,  XA_RM_STATUS_ACTIVE,      XA_TX_STAGE_ABORTING},
    {XA_TX_STAGE_ABORTING, XA_RM_STATUS_PREP,   XA_OP_ROLLBACK, XAER_PROTO, XAER_PROTO,  XA_RM_STATUS_PREP,        XA_TX_STAGE_ABORTING},
    {FAIL}
};

/**
 * If Stage/State not in list, then assume XA_OP_NOP
 */
public txaction_driver_t G_txaction_driver[] =
{  
    {XA_TX_STAGE_PREPARING, XA_RM_STATUS_ACTIVE, 		XA_OP_PREPARE},
    {XA_TX_STAGE_COMMITTING,XA_RM_STATUS_PREP, 			XA_OP_COMMIT},
    {XA_TX_STAGE_ABORTING,  XA_RM_STATUS_ACTIVE, 		XA_OP_ROLLBACK},
    {XA_TX_STAGE_ABORTING,  XA_RM_STATUS_PREP, 	    	        XA_OP_ROLLBACK},
    {FAIL}
};

/**
 * State descriptors
 */
public txstage_descriptor_t G_state_descriptor[] =
{
{XA_TX_STAGE_NULL,             XA_TX_STAGE_NULL,             XA_TX_STAGE_NULL,            XA_TX_STAGE_NULL, "NULL", FALSE},
{XA_TX_STAGE_ACTIVE,           XA_TX_STAGE_ACTIVE,           XA_TX_STAGE_NULL,            XA_TX_STAGE_NULL, "ACTIVE", FALSE},
{XA_TX_STAGE_ABORTING,         XA_TX_STAGE_ABORTING,         XA_TX_STAGE_ABORTED_HAZARD,  XA_TX_STAGE_ABORTED, "ABORTING", FALSE},
/* Left for compliance: */
{XA_TX_STAGE_ABORTED_HAZARD,   XA_TX_STAGE_ABORTED_HAZARD,   XA_TX_STAGE_ABORTED_HAZARD,  XA_TX_STAGE_ABORTED_HAZARD, "ABORTED_HAZARD", FALSE},
/* Left for compliance: */
{XA_TX_STAGE_ABORTED_HEURIS,   XA_TX_STAGE_ABORTED_HEURIS,   XA_TX_STAGE_ABORTED_HEURIS,  XA_TX_STAGE_ABORTED_HEURIS, "ABORTED_HEURIS", FALSE},
/* Left for compliance: */
{XA_TX_STAGE_ABORTED,          XA_TX_STAGE_ABORTED,          XA_TX_STAGE_ABORTED,         XA_TX_STAGE_ABORTED, "ABORTED", FALSE},
/* This assumes that after preparing follows committing, and this is the group. The completed min is: XA_TX_STAGE_COMMITTED_HAZARD
 * and not XA_TX_STAGE_PREPARING! */
{XA_TX_STAGE_PREPARING,        XA_TX_STAGE_PREPARING,        XA_TX_STAGE_COMMITTED_HAZARD,XA_TX_STAGE_COMMITTED, "PREPARING", TRUE},
{XA_TX_STAGE_COMMITTING,       XA_TX_STAGE_COMMITTING,       XA_TX_STAGE_COMMITTED_HAZARD,XA_TX_STAGE_COMMITTED, "COMMITTING", FALSE},
/* Left for compliance: */
{XA_TX_STAGE_COMMITTED_HAZARD, XA_TX_STAGE_COMMITTED_HAZARD, XA_TX_STAGE_COMMITTED_HAZARD,XA_TX_STAGE_COMMITTED_HAZARD, "COMMITTED_HAZARD", FALSE},
/* Left for compliance: */
{XA_TX_STAGE_COMMITTED_HEURIS, XA_TX_STAGE_COMMITTED_HEURIS, XA_TX_STAGE_COMMITTED_HEURIS,XA_TX_STAGE_COMMITTED_HEURIS, "COMMITTED_HEURIS", FALSE},
/* Left for compliance: */
{XA_TX_STAGE_COMMITTED,        XA_TX_STAGE_COMMITTED,        XA_TX_STAGE_COMMITTED,       XA_TX_STAGE_COMMITTED, "COMMITTED", FALSE},
{FAIL, 0, 0, 0, "FAIL"}
};

/**
 * Needs new table: state-to-tpreturn code mapper. 
 */
public txstate2tperrno_t G_txstage2tperrno[] =
{
{XA_TX_STAGE_NULL,             XA_OP_COMMIT, TPESYSTEM},
{XA_TX_STAGE_ACTIVE,           XA_OP_COMMIT, TPESYSTEM},
{XA_TX_STAGE_ABORTING,         XA_OP_COMMIT, TPEHAZARD},
{XA_TX_STAGE_ABORTED_HAZARD,   XA_OP_COMMIT, TPEHAZARD},
{XA_TX_STAGE_ABORTED_HEURIS,   XA_OP_COMMIT, TPEHEURISTIC},
/* Extra check on exit, if abort was requested, then translate to SUCCEED */
{XA_TX_STAGE_ABORTED,          XA_OP_COMMIT, TPEABORT},
{XA_TX_STAGE_PREPARING,        XA_OP_COMMIT, TPESYSTEM},
{XA_TX_STAGE_COMMITTING,       XA_OP_COMMIT, TPEHAZARD},
{XA_TX_STAGE_COMMITTED_HAZARD, XA_OP_COMMIT, TPEHAZARD},
{XA_TX_STAGE_COMMITTED_HEURIS, XA_OP_COMMIT, TPEHEURISTIC},
{XA_TX_STAGE_COMMITTED,        XA_OP_COMMIT, SUCCEED},

{XA_TX_STAGE_NULL,             XA_OP_ROLLBACK, TPESYSTEM},
{XA_TX_STAGE_ACTIVE,           XA_OP_ROLLBACK, TPESYSTEM},
{XA_TX_STAGE_ABORTING,         XA_OP_ROLLBACK, TPEHAZARD},
{XA_TX_STAGE_ABORTED_HAZARD,   XA_OP_ROLLBACK, TPEHAZARD},
{XA_TX_STAGE_ABORTED_HEURIS,   XA_OP_ROLLBACK, TPEHEURISTIC},
/* Extra check on exit, if abort was requested, then translate to SUCCEED */
{XA_TX_STAGE_ABORTED,          XA_OP_ROLLBACK, SUCCEED},
{XA_TX_STAGE_PREPARING,        XA_OP_ROLLBACK, TPESYSTEM},
{XA_TX_STAGE_COMMITTING,       XA_OP_ROLLBACK, TPEHAZARD},
{XA_TX_STAGE_COMMITTED_HAZARD, XA_OP_ROLLBACK, TPEHAZARD},
{XA_TX_STAGE_COMMITTED_HEURIS, XA_OP_ROLLBACK, TPEHEURISTIC},
{XA_TX_STAGE_COMMITTED,        XA_OP_ROLLBACK, TPEHEURISTIC},

{FAIL}
};

/*---------------------------Prototypes---------------------------------*/

/**
 * Get next RM status/txstage by current operation.
 * @param txstage - current tx stage
 * @param rmstatus - current RM status
 * @param op - operation done
 * @param op_retcode - operation return code
 * @return NULL or transition descriptor
 */
public rmstatus_driver_t* xa_status_get_next_by_op(short txstage, char rmstatus, 
                                                    int op, int op_retcode)
{
    rmstatus_driver_t *ret = G_rm_status_driver;
    
    while (FAIL!=ret->txstage)
    {
        if (ret->txstage == txstage &&
                ret->rmstatus == rmstatus &&
                ret->op == op &&
                op_retcode>=ret->min_retcode && 
                op_retcode<=ret->max_retcode
            )
        {
            break;
        }
        ret++;
    } 
    
    if (FAIL==ret->txstage)
    {
        ret=NULL;
    }
    
    return ret;
}

/**
 * If we got NOP on current stage action for RM, then we lookup
 * this table, to get it's wote for next txstage.
 * @param txstage - current tx stage
 * @param next_rmstatus - RM status (after NOP...)
 * @return NULL or transition descriptor
 */
public rmstatus_driver_t* xa_status_get_next_by_new_status(short   txstage, 
                                                    char next_rmstatus)
{
    rmstatus_driver_t *ret = G_rm_status_driver;
    
    while (FAIL!=ret->txstage)
    {
        if (ret->txstage == txstage &&
                ret->next_rmstatus == next_rmstatus
            )
        {
            break;
        }
        ret++;
    }
    
    if (FAIL==ret->txstage)
    {
        ret=NULL;
    }
    
    return ret;
}

/**
 * Get operation to be done for current RM in given state
 * @param txstage current tx stage
 * @param rmstatus curren RM status
 * @return operation to be done or NOP
 */
public int xa_status_get_op(short txstage, char rmstatus)
{
    txaction_driver_t *ret = G_txaction_driver;
    
    while (FAIL!=ret->txstage)
    {
        if (ret->txstage == txstage &&
                ret->rmstatus == rmstatus
            )
        {
            break;
        }
        ret++;
    }
    
    if (FAIL!=ret->txstage)
    {
        return ret->op;
    }
    else
    {
        return XA_OP_NOP;
    }
}

/**
 * Get stage descriptor
 * @param txstage
 * @return NULL or stage descriptor
 */
public txstage_descriptor_t* xa_stage_get_descr(short txstage)
{
    txstage_descriptor_t* ret = G_state_descriptor;
    
    while (FAIL!=ret->txstage)
    {
        if (ret->txstage == txstage)
        {
            break;
        }
        ret++;
    }
    
    if (FAIL==ret->txstage)
        ret = NULL;
    
    return ret;
}

/**
 * Translate stage to tperrno
 * @param txstage
 * @return 
 */
public int xa_txstage2tperrno(short txstage, int master_op)
{
    txstate2tperrno_t* ret = G_txstage2tperrno;
    
    while (FAIL!=ret->txstage)
    {
        if (ret->txstage == txstage &&
                ret->master_op == master_op
                )
        {
            break;
        }
        ret++;
    }
    
    if (FAIL==ret->txstage)
        return TPESYSTEM;
    
    return ret->tpe;
}

