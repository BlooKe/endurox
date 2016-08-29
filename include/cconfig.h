/* 
** Enduro/X common-config
**
** @file cconfig.h
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
#ifndef _CCONFIG_H
#define	_CCONFIG_H

#ifdef	__cplusplus
extern "C" {
#endif

/*---------------------------Includes-----------------------------------*/
#include <sys/stat.h>
#include <ndrxdcmn.h>
#include <stdint.h>
#include <ntimer.h>
#include <uthash.h>
#include <sys_unix.h>
/*---------------------------Externs------------------------------------*/
/*---------------------------Macros-------------------------------------*/
#define NDRX_CCONFIG5 "NDRX_CCONFIG5"
#define NDRX_CCONFIG4 "NDRX_CCONFIG4"
#define NDRX_CCONFIG3 "NDRX_CCONFIG3"
#define NDRX_CCONFIG2 "NDRX_CCONFIG2"
#define NDRX_CCONFIG1 "NDRX_CCONFIG1"
#define NDRX_CCONFIG  "NDRX_CCONFIG"
    
#define NDRX_CCTAG "NDRX_CCTAG" /* common-config tag */
/*---------------------------Enums--------------------------------------*/
/*---------------------------Typedefs-----------------------------------*/
/*---------------------------Globals------------------------------------*/
/*---------------------------Statics------------------------------------*/
/*---------------------------Prototypes---------------------------------*/
#ifdef	__cplusplus
}
#endif

#endif	/* _CCONFIG_H */

