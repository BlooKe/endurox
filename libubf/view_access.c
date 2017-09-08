/* 
** Dynamic VIEW access functions
**
** @file view_access.c
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
#include <errno.h>
#include <dirent.h>
#include <limits.h>

#include <ndrstandard.h>
#include <ubfview.h>
#include <ndebug.h>

#include <userlog.h>
#include <view_cmn.h>
#include <atmi_tls.h>
#include <cf.h>
#include "Exfields.h"
/*---------------------------Externs------------------------------------*/
/*---------------------------Macros-------------------------------------*/
/*---------------------------Enums--------------------------------------*/
/*---------------------------Typedefs-----------------------------------*/
/*---------------------------Globals------------------------------------*/
/*---------------------------Statics------------------------------------*/
/*---------------------------Prototypes---------------------------------*/

/**
 * Return the VIEW field according to user type
 * In case of NULL value, we do not return the given occurrence.
 * If the C_count is less than given occurrence and BVNEXT_NOTNULL is set, then
 * field not found will be returned.
 * 
 * @param cstruct instance of the view object
 * @param view view name
 * @param cname field name in view
 * @param occ array occurrence of the field (if count > 0)
 * @param buf user buffer to install data to
 * @param len on input user buffer len, on output bytes written to (for carray only)
 * on input for string too.
 * @param usrtype of buf see BFLD_* types
 * @param mode BVNEXT_ALL or BVNEXT_NOTNULL
 * @return 0 on success, or -1 on fail. 
 * 
 * The following errors possible:
 * - BBADVIEW view not found
 * - BNOCNAME field not found
 * - BNOTPRES field is NULL
 * - BEINVAL cstruct/view/cname/buf is NULL
 * - BEINVAL - invalid usrtype
 * - BEINVAL - invalid occurrence
 * - BNOSPACE - no space in buffer (the data is larger than user buf specified)
 */

expublic int ndrx_CBvget_int(char *cstruct, ndrx_typedview_t *v,
	ndrx_typedview_field_t *f, BFLDOCC occ, char *buf, BFLDLEN *len, 
			     int usrtype, long flags)
{
	int ret = EXSUCCEED;
	int dim_size = f->fldsize/f->count;
	char *fld_offs = cstruct+f->offset+occ*dim_size;
	char *cvn_buf;
	short *C_count;
	short C_count_stor;
	unsigned short *L_length;
	unsigned short L_length_stor;
    
	UBF_LOG(log_debug, "%s enter, get %s.%s occ=%d", __func__,
		v->vname, f->cname, occ);
	
	NDRX_VIEW_COUNT_SETUP;
		
	if (flags & BVACCESS_NOTNULL)
	{
		if (ndrx_Bvnull_int(v, f, occ, cstruct))
		{
			NDRX_LOG(log_debug, "Field is NULL");
			ndrx_Bset_error_fmt(BNOTPRES, "%s.%s occ=%d is NULL", 
					 v->vname, f->cname, occ);
			EXFAIL_OUT(ret);
		}
		
		if (*C_count-1<occ+1)
		{
			NDRX_LOG(log_debug, "%s.%s count field is set to %hu, "
				"but requesting occ=%d (zero based) - NOT PRES",
				 v->vname, f->cname, *C_count, occ);
			ndrx_Bset_error_fmt(BNOTPRES, "%s.%s count field is set to %hu, "
				"but requesting occ=%d (zero based) - NOT PRES",
				 v->vname, f->cname, *C_count, occ);
			EXFAIL_OUT(ret);
		}
	}
	
	/* Will request type convert now */
	NDRX_VIEW_LEN_SETUP;
	
	cvn_buf = ndrx_ubf_convert(f->typecode_full, CNV_DIR_OUT, fld_offs, *L_length,
                                    usrtype, buf, len);
        if (NULL==cvn_buf)
        {
            UBF_LOG(log_error, "%s: failed to convert data!", __func__);
            /* Error should be provided by conversation function */
            EXFAIL_OUT(ret);
        }
	 
out:
	UBF_LOG(log_debug, "%s return %d", __func__, ret);
	
	return ret;
}

/**
 * Wrapper for function for ndrx_CBvget_int, just to resolve the objects
 * @param cstruct
 * @param view
 * @param cname
 * @param occ
 * @param buf
 * @param len
 * @param usrtype
 * @param mode
 * @return 
 */
expublic int ndrx_CBvget(char *cstruct, char *view, char *cname, BFLDOCC occ, 
			 char *buf, BFLDLEN *len, int usrtype, long flags)
{
	int ret = EXFALSE;
	ndrx_typedview_t *v = NULL;
	ndrx_typedview_field_t *f = NULL;
	
	/* resolve view & field, call ndrx_Bvnull_int */
    
	if (NULL==(v = ndrx_view_get_view(view)))
	{
	    ndrx_Bset_error_fmt(BBADVIEW, "View [%s] not found!", view);
	    EXFAIL_OUT(ret);
	}

	if (NULL==(f = ndrx_view_get_field(v, cname)))
	{
	    ndrx_Bset_error_fmt(BNOCNAME, "Field [%s] of view [%s] not found!", 
		    cname, v->vname);
	    EXFAIL_OUT(ret);
	}

	if (occ > f->count-1 || occ<0)
	{
	    ndrx_Bset_error_fmt(BEINVAL, "Invalid occurrence requested for field "
		    "%s.%s, count=%d occ=%d (zero base)",
		    v->vname, f->cname, f->count, occ);
	    EXFAIL_OUT(ret);
	}
    
	if (EXFAIL==(ret=ndrx_CBvget_int(cstruct, v, f, occ, buf, len, 
					 usrtype, flags)))
	{
		/* error must be set */
		NDRX_LOG(log_error, "ndrx_CBvget_int failed");
		EXFAIL_OUT(ret);
	}
	
out:
	
	return ret;
}

/**
 * Return the VIEW field according to user type
 * If "C" (count flag) was used, and is less than occ+1, then C_<field> is incremented
 * to occ+1.
 * 
 * In case if "L" (length) was present and this is carray buffer, then L flag will
 * be set
 * 
 * @param cstruct instance of the view object
 * @param view view name
 * @param cname field name in view
 * @param occ array occurrence of the field (if count > 0)
 * @param buf data to set
 * @param len used only for carray, to indicate the length of the data
 * @param usrtype of buf see BFLD_* types
 * @return 0 on success, or -1 on fail. 
 * 
 * The following errors possible:
 * - BBADVIEW view not found
 * - BNOCNAME field not found
 * - BNOTPRES field not present (invalid occ)
 * - BEINVAL cstruct/view/cname/buf is NULL
 * - BEINVAL invalid usrtype
 * - BNOSPACE the view field is shorter than data received
 */
expublic int ndrx_CBvset_int(char *cstruct, ndrx_typedview_t *v, 
		ndrx_typedview_field_t *f, BFLDOCC occ, char *buf, 
			     BFLDLEN len, int usrtype)
{
	int ret = EXSUCCEED;
	int dim_size = f->fldsize/f->count;
	char *fld_offs = cstruct+f->offset+occ*dim_size;
	char *cvn_buf;
	short *C_count;
	short C_count_stor;
	unsigned short *L_length;
	unsigned short L_length_stor;
   
	BFLDLEN setlen;
	UBF_LOG(log_debug, "%s enter, get %s.%s occ=%d", __func__,
		v->vname, f->cname, occ);
	
	NDRX_VIEW_COUNT_SETUP;
	
	/* Length output buffer */
	NDRX_VIEW_LEN_SETUP;

	setlen = dim_size;
	
	cvn_buf = ndrx_ubf_convert(usrtype, CNV_DIR_OUT, buf, len,
                                    f->typecode_full, fld_offs, &setlen);
	
	if (NULL==cvn_buf)
        {
            UBF_LOG(log_error, "%s: failed to convert data!", __func__);
            /* Error should be provided by conversation function */
            EXFAIL_OUT(ret);
        }
	
	*L_length = setlen;
	
out:	
	UBF_LOG(log_debug, "%s return %d", __func__, ret);
	
	return ret;
}

/**
 * Set the field wrapper for internal method above.
 * @param cstruct
 * @param view
 * @param cname
 * @param occ
 * @param buf
 * @param len
 * @param usrtype
 * @return 
 */
expublic int ndrx_CBvset(char *cstruct, char *view, char *cname, BFLDOCC occ, 
			 char *buf, BFLDLEN len, int usrtype)
{
	int ret = EXFALSE;
	ndrx_typedview_t *v = NULL;
	ndrx_typedview_field_t *f = NULL;
	
	/* resolve view & field, call ndrx_Bvnull_int */
    
	if (NULL==(v = ndrx_view_get_view(view)))
	{
	    ndrx_Bset_error_fmt(BBADVIEW, "View [%s] not found!", view);
	    EXFAIL_OUT(ret);
	}

	if (NULL==(f = ndrx_view_get_field(v, cname)))
	{
	    ndrx_Bset_error_fmt(BNOCNAME, "Field [%s] of view [%s] not found!", 
		    cname, v->vname);
	    EXFAIL_OUT(ret);
	}

	if (occ > f->count-1 || occ<0)
	{
	    ndrx_Bset_error_fmt(BEINVAL, "Invalid occurrence requested for field "
		    "%s.%s, count=%d occ=%d (zero base)",
		    v->vname, f->cname, f->count, occ);
	    EXFAIL_OUT(ret);
	}
    
	if (EXFAIL==(ret=ndrx_CBvset_int(cstruct, v, f, occ, buf, len, usrtype)))
	{
		/* error must be set */
		NDRX_LOG(log_error, "ndrx_CBvset_int failed");
		EXFAIL_OUT(ret);
	}
	
out:
	return ret;
}

/**
 * Return size of the given view in bytes
 * @param view view name
 * @return >=0 on success, -1 on FAIL.
 * Errors:
 * BBADVIEW view not found
 * BEINVAL view field is NULL
 */
expublic long ndrx_Bvsizeof(char *view)
{
	return EXFAIL;
}

/**
 * Return the occurrences set in buffer. This will either return C_ count field
 * set, or will return max array size, this does not test field against NULL or not.
 * anything, 
 * @param cstruct instance of the view object
 * @param view view name
 * @param cname field name in view
 * @return occurrences
 *  The following errors possible:
 * - BBADVIEW view not found
 * - BNOCNAME field not found
 * - BEINVAL cstruct/view/cname/buf is NULL
 * 
 */
expublic BFLDOCC ndrx_Bvoccur(char *cstruct, char *view, char *cname)
{
	return EXFAIL;
}

/**
 * Set C_ occurrence indicator field, if "C" flag was set
 * If flag is not present, function succeeds and does not return error, but
 * data also is not changed.
 * @param cstruct instance of the view object
 * @param view view name
 * @param cname field name in view
 * @param occ occurrences
 * @return 0 on success or -1 on failure
 *  The following errors possible:
 * - BBADVIEW view not found
 * - BNOCNAME field not found
 * - BEINVAL cstruct/view/cname/buf is NULL
 * - BNOTPRES occ out of bounds
 */
expublic int ndrx_Bvsetoccur(char *cstruct, char *view, char *cname, BFLDOCC occ)
{
	return EXFAIL;
}

/**
 * This will iterate over the view buffer and will return the value in buf/len
 * optionally.
 * @param state Save state for scanning. For start scan, do memset 0 on state
 * @param cstruct instance of the view object
 * @param view view name
 * @param cname output cname field
 * @param cname_len optional buffer len indicator of cname
 * @param fldtype return the type of the field
 * @param occ occurrence return
 * @param is_null return TRUE if value is NULL (tested only if BNEXT_NOTNULL not set)
 * @param buf data copied to buf (optional if not NULL)
 * @param len buffer len (if set on input validate the input len) on output bytes copied
 * @param usrtype in case if not -1, the data will be converted to destination type
 * otherwise raw data is copied out...
 * including NULL, BNEXT_NOTNULL - return only non NULL values
 */
expublic int ndrx_Bvnext (Bvnext_state_t *state, char *cstruct, 
		char *view, char *cname, BFLDLEN * cname_len, int *fldtype, 
			  BFLDOCC *occ, int *is_null,
			  char *buf, BFLDLEN *len, long mode, int usrtype)
{
	/* will use conv_same to return data in user buffer */
	return EXFAIL;
}

