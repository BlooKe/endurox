/* 
** API functions for Events:
** - tpsubscribe
** - tpunsubscribe
** - tppost
**
** @file tpevents.c
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
#include <stdlib.h>
#include <memory.h>

#include <ubf.h>
#include <atmi.h>
#include <atmi_int.h>
#include <ndebug.h>
#include <ndrstandard.h>
#include <Exfields.h>
#include <tperror.h>
/*---------------------------Externs------------------------------------*/
/*---------------------------Macros-------------------------------------*/
/*---------------------------Enums--------------------------------------*/
/*---------------------------Typedefs-----------------------------------*/
/*---------------------------Globals------------------------------------*/
/*---------------------------Statics------------------------------------*/
/*---------------------------Prototypes---------------------------------*/


/**
 * Internal version of tpsubscribe.
 * Actually do the main logic of the function
 */
long _tpsubscribe(char *eventexpr, char *filter, TPEVCTL *ctl, long flags)
{
    long ret=EXSUCCEED;
    UBFH *p_ub = NULL;
    char *fn = "_tpsubscribe";
    char *ret_buf;
    long ret_len;

    NDRX_LOG(log_debug, "%s enter", fn);

    if (NULL==eventexpr || EXEOS==eventexpr[0])
    {
        _TPset_error_fmt(TPEINVAL, "eventexpr cannot be null/empty!");
        ret=EXFAIL;
        goto out;
    }
    /* Check the lenght */
    if (strlen(eventexpr)>255)
    {
        _TPset_error_fmt(TPEINVAL, "eventexpre longer than 255 bytes!");
        ret=EXFAIL;
        goto out;
    }

    if (NULL==ctl)
    {
        _TPset_error_fmt(TPEINVAL, "ctl cannot be null/empty!");
        ret=EXFAIL;
        goto out;
    }

    if (EXEOS==ctl->name1[0])
    {
        _TPset_error_fmt(TPEINVAL, "ctl->name1 cannot be null!");
        ret=EXFAIL;
        goto out;
    }

    /* All ok, we can now allocate the buffer... */
    if (NULL==(p_ub = (UBFH *)tpalloc("UBF", NULL, 1024)))
    {
        NDRX_LOG(log_error, "%s: failed to allocate 1024", fn);
        ret=EXFAIL;
        goto out;
    }

    /* Set up paramters */
    if (EXFAIL==Badd(p_ub, EV_MASK, (char *)eventexpr, 0L))
    {
        _TPset_error_fmt(TPESYSTEM, "Failed to set EV_MASK/eventexpr: [%s]", Bstrerror(Berror));
        ret=EXFAIL;
        goto out;
    }

    /* Check the len */
    if (NULL!=filter && EXEOS!=filter[0] && strlen(filter)>255)
    {
        _TPset_error_fmt(TPEINVAL, "filter longer than 255 bytes!");
        ret=EXFAIL;
    }

    /* setup filter argument is set */
    if (NULL!=filter && EXEOS!=filter[0] &&
            EXFAIL==Badd(p_ub, EV_FILTER, filter, 0L))
    {
        _TPset_error_fmt(TPESYSTEM, "Failed to set EV_FILTER/filter: [%s]",
                                            Bstrerror(Berror));
        ret=EXFAIL;
        goto out;
    }


    if (EXFAIL==CBadd(p_ub, EV_FLAGS, (char *)&ctl->flags, 0L, BFLD_LONG))
    {
        _TPset_error_fmt(TPESYSTEM, "Failed to set EV_FLAGS/flags: [%s]",
                                            Bstrerror(Berror));
        ret=EXFAIL;
        goto out;
    }

    if (EXFAIL==CBadd(p_ub, EV_SRVCNM, ctl->name1, 0L, BFLD_STRING))
    {
        _TPset_error_fmt(TPESYSTEM, "Failed to set EV_SRVCNM/name1: [%s]",
                                            Bstrerror(Berror));
        ret=EXFAIL;
        goto out;
    }

    if (EXFAIL!=(ret=tpcall(NDRX_SYS_SVC_PFX "TPEVSUBS", (char *)p_ub, 0L, &ret_buf, &ret_len, flags)))
    {
        ret=tpurcode; /* Return code - count of events applied */
    }

out:
    /* free up any allocated resources */
    if (NULL!=p_ub)
    {
        atmi_error_t err;
        /* Save the original error/needed later! */
        _TPsave_error(&err);
        tpfree((char*)p_ub);
        _TPrestore_error(&err);
    }

    NDRX_LOG(log_debug, "%s returns %ld", fn, ret);
    
    return ret;
}

/**
 * Internal version of tpunsubscribe.
 * Actually do the main logic of the function
 */
 long _tpunsubscribe(long subscription, long flags)
{
    long ret=EXSUCCEED;
    UBFH *p_ub = NULL;
    char *fn = "_tpunsubscribe";
    char *ret_buf;
    long ret_len;

    NDRX_LOG(log_debug, "%s enter", fn);

    /* All ok, we can now allocate the buffer... */
    if (NULL==(p_ub = (UBFH *)tpalloc("UBF", NULL, 512)))
    {
        NDRX_LOG(log_error, "%s: failed to allocate 512", fn);
        ret=EXFAIL;
        goto out;
    }

    /* Check the validity of arguments */
    if (subscription<-1)
    {
        _TPset_error_fmt(TPEINVAL, "%s: subscription %ld cannot be < -1",
                                    fn, subscription);
        ret=EXFAIL;
        goto out;
    }
    
    if (EXFAIL==CBadd(p_ub, EV_SUBSNR, (char *)&subscription, 0L, BFLD_LONG))
    {
        _TPset_error_fmt(TPESYSTEM, "Failed to set EV_SUBSNR/flags: [%s]",
                                            Bstrerror(Berror));
        ret=EXFAIL;
        goto out;
    }

    if (EXFAIL!=(ret=tpcall(NDRX_SYS_SVC_PFX "TPEVUNSUBS", (char *)p_ub, 0L, &ret_buf, &ret_len, flags)))
    {
        ret=tpurcode; /* Return code - count of events applied */
    }

out:
    /* free up any allocated resources */
    if (NULL!=p_ub)
    {
        atmi_error_t err;
        /* Save the original error/needed later! */
        _TPsave_error(&err);
        tpfree((char*)p_ub);
        _TPrestore_error(&err);
    }

    NDRX_LOG(log_debug, "%s returns %ld", fn, ret);
    return ret;
}

/**
 *
 * @param eventname
 * @param data
 * @param len
 * @param flags
 * @return
 */
int _tppost(char *eventname, char *data, long len, long flags)
{
    int ret=EXSUCCEED;
    char fn[]="_tppost";
    char *ret_buf;
    long ret_len;
    
    NDRX_LOG(log_debug, "%s enter", fn);

    if (NULL==eventname || EXEOS==eventname[0])
    {
        _TPset_error_fmt(TPEINVAL, "%s: eventname cannot be null/empty", fn);
        ret=EXFAIL;
        goto out;
    }

    /* Post the */
    if (EXFAIL!=(ret=tpcallex(NDRX_SYS_SVC_PFX EV_TPEVPOST, 
            data, len, &ret_buf, &ret_len, flags, eventname, EXFAIL, 0)))
    {
        ret=tpurcode; /* Return code - count of events applied */
    }

out:

    NDRX_LOG(log_debug, "%s returns %d", fn, ret);
    return ret;
}
