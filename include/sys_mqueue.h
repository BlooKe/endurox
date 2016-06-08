/* 
** Message queue abstractions
**
** @file sys_mqueue.h
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
#ifndef _SYS_MQUEUE_H
#define	_SYS_MQUEUE_H

#ifdef	__cplusplus
extern "C" {
#endif
/*---------------------------Includes-----------------------------------*/
#include <config.h>
#include <mqueue.h>

/*---------------------------Externs------------------------------------*/
/*---------------------------Macros-------------------------------------*/
#define  ndrx_mq_timedreceive mq_timedreceive
#define  ndrx_mq_timedsend    mq_timedsend
#define  ndrx_mq_close        mq_close
#define  ndrx_mq_getattr      mq_getattr
#define  ndrx_mq_notify       mq_notify
#define  ndrx_mq_receive      mq_receive
#define  ndrx_mq_send         mq_send
#define  ndrx_mq_setattr      mq_setattr
    
#if USE_FS_REGISTRY

extern mqd_t ndrx_mq_open_with_registry(const char *name, int oflag, mode_t mode, struct mq_attr *attr);
extern int ndrx_mq_unlink_with_registry (const char *name);

#define  ndrx_mq_open         ndrx_mq_open_with_registry
#define  ndrx_mq_unlink       ndrx_mq_unlink_with_registry

#else
    
#define  ndrx_mq_open         mq_open
#define  ndrx_mq_unlink       mq_unlink
    
    
#endif
    

/*---------------------------Enums--------------------------------------*/
/*---------------------------Typedefs-----------------------------------*/
/*---------------------------Globals------------------------------------*/
/*---------------------------Statics------------------------------------*/
/*---------------------------Prototypes---------------------------------*/
    
#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MQUEUE_H */

