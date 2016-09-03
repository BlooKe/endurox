/* 
** Common-config server tests
**
** @file atmiclt30.c
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

#include <atmi.h>
#include <ubf.h>
#include <ndebug.h>
#include <test.fd.h>
#include <ndrstandard.h>
#include <Exfields.h>
/*---------------------------Externs------------------------------------*/
/*---------------------------Macros-------------------------------------*/
/*---------------------------Enums--------------------------------------*/
/*---------------------------Typedefs-----------------------------------*/
/*---------------------------Globals------------------------------------*/
/*---------------------------Statics------------------------------------*/
/*---------------------------Prototypes---------------------------------*/

/*
 * Do the test call to the server
 */
int main(int argc, char** argv) {

    UBFH *p_ub;
    long rsplen;
    int i;
    int ret=SUCCEED;
    long revent;
    int received = 0;
    int occ1, occ2;
    int cd;
    
    /**************************************************************************
     * Get one section, with type checks (OK)
     **************************************************************************/
    if (NULL==(p_ub= (UBFH *)tpalloc("UBF", NULL, 1024)))
    {
        NDRX_LOG(log_error, "TESTERROR: buffer alloc failed: %s", tpstrerror(tperrno));
        FAIL_OUT(ret);
    }
    
    if (SUCCEED!=Bchg(p_ub, EX_CC_LOOKUPSECTION, 0, "my_app", 0L) ||
            
            SUCCEED!=Badd(p_ub, EX_CC_FORMAT_KEY,"string_setting", 0L) ||
            SUCCEED!=Badd(p_ub, EX_CC_FORMAT_FORMAT, "s..6", 0L) ||
            
            SUCCEED!=Badd(p_ub, EX_CC_FORMAT_KEY, "float_setting", 0L) ||
            SUCCEED!=Badd(p_ub, EX_CC_FORMAT_FORMAT, "n1..5", 0L) ||
            
            SUCCEED!=Badd(p_ub, EX_CC_FORMAT_KEY, "integer_setting", 0L) ||
            SUCCEED!=Badd(p_ub, EX_CC_FORMAT_FORMAT, "i..6", 0L) ||
            
            SUCCEED!=Badd(p_ub, EX_CC_FORMAT_KEY, "true_setting", 0L) ||
            SUCCEED!=Badd(p_ub, EX_CC_FORMAT_FORMAT, "t", 0L) ||
            
            SUCCEED!=Badd(p_ub, EX_CC_FORMAT_KEY, "false_setting", 0L) ||
            SUCCEED!=Badd(p_ub, EX_CC_FORMAT_FORMAT, "t", 0L)
            )
    {
        NDRX_LOG(log_error, "TESTERROR: Failed to setup buffer: %s", Bstrerror(Berror));
        FAIL_OUT(ret);
    }   
    
    /* call the server */
    if (FAIL == tpcall("@CCONF", (char *)p_ub, 0L, (char **)&p_ub, &rsplen,0))
    {
        NDRX_LOG(log_error, "TESTERROR: @CCONF failed: %s", tpstrerror(tperrno));
        FAIL_OUT(ret);
    }
    
    /* verify results... */
    
    /* key: string_setting */
    
    if (FAIL==(occ1=CBfindocc (p_ub, EX_CC_KEY, "string_setting", 0, BFLD_STRING)))
    {
        NDRX_LOG(log_error, "TESTERROR: EX_CC_KEY with value [string_setting] "
                "not found: %s", Bstrerror(Berror));
        FAIL_OUT(ret);
    }
    
    if (FAIL==(occ2=CBfindocc (p_ub, EX_CC_VALUE, "value1", 0, BFLD_STRING)))
    {
        NDRX_LOG(log_error, "TESTERROR: EX_CC_VALUE with value [value1] "
                "not found: %s", Bstrerror(Berror));
        FAIL_OUT(ret);
    }
    
    if (occ1!=occ2)
    {
        NDRX_LOG(log_error, "Invalid occurrences: %d vs %d", occ1, occ2);
        FAIL_OUT(ret);
    }
    
    /* key: float_setting */
    
    if (FAIL==(occ1=CBfindocc (p_ub, EX_CC_KEY, "float_setting", 0, BFLD_STRING)))
    {
        NDRX_LOG(log_error, "TESTERROR: EX_CC_KEY with value [float_setting] "
                "not found: %s", Bstrerror(Berror));
        FAIL_OUT(ret);
    }
    
    if (FAIL==(occ2=CBfindocc (p_ub, EX_CC_VALUE, "-1.99", 0, BFLD_STRING)))
    {
        NDRX_LOG(log_error, "TESTERROR: EX_CC_VALUE with value [-1.99] "
                "not found: %s", Bstrerror(Berror));
        FAIL_OUT(ret);
    }
    
    if (occ1!=occ2)
    {
        NDRX_LOG(log_error, "Invalid occurrences: %d vs %d", occ1, occ2);
        FAIL_OUT(ret);
    }
    
    
    /* integer_setting=100001 */
    
    if (FAIL==(occ1=CBfindocc (p_ub, EX_CC_KEY, "integer_setting", 0, BFLD_STRING)))
    {
        NDRX_LOG(log_error, "TESTERROR: EX_CC_KEY with value [integer_setting] "
                "not found: %s", Bstrerror(Berror));
        FAIL_OUT(ret);
    }
    
    if (FAIL==(occ2=CBfindocc (p_ub, EX_CC_VALUE, "100001", 0, BFLD_STRING)))
    {
        NDRX_LOG(log_error, "TESTERROR: EX_CC_VALUE with value [100001] "
                "not found: %s", Bstrerror(Berror));
        FAIL_OUT(ret);
    }
    
    if (occ1!=occ2)
    {
        NDRX_LOG(log_error, "Invalid occurrences: %d vs %d", occ1, occ2);
        FAIL_OUT(ret);
    }
    
    
    /* true_setting=True */
    
    if (FAIL==(occ1=CBfindocc (p_ub, EX_CC_KEY, "true_setting", 0, BFLD_STRING)))
    {
        NDRX_LOG(log_error, "TESTERROR: EX_CC_KEY with value [true_setting] "
                "not found: %s", Bstrerror(Berror));
        FAIL_OUT(ret);
    }
    
    if (FAIL==(occ2=CBfindocc (p_ub, EX_CC_VALUE, "True", 0, BFLD_STRING)))
    {
        NDRX_LOG(log_error, "TESTERROR: EX_CC_VALUE with value [True] "
                "not found: %s", Bstrerror(Berror));
        FAIL_OUT(ret);
    }
    
    if (occ1!=occ2)
    {
        NDRX_LOG(log_error, "Invalid occurrences: %d vs %d", occ1, occ2);
        FAIL_OUT(ret);
    }
    
    
    
    /* false_setting=no */
     
    if (FAIL==(occ1=CBfindocc (p_ub, EX_CC_KEY, "false_setting", 0, BFLD_STRING)))
    {
        NDRX_LOG(log_error, "TESTERROR: EX_CC_KEY with value [false_setting] "
                "not found: %s", Bstrerror(Berror));
        FAIL_OUT(ret);
    }
    
    if (FAIL==(occ2=CBfindocc (p_ub, EX_CC_VALUE, "no", 0, BFLD_STRING)))
    {
        NDRX_LOG(log_error, "TESTERROR: EX_CC_VALUE with value [no] "
                "not found: %s", Bstrerror(Berror));
        FAIL_OUT(ret);
    }
    
    if (occ1!=occ2)
    {
        NDRX_LOG(log_error, "Invalid occurrences: %d vs %d", occ1, occ2);
        FAIL_OUT(ret);
    }
    
    tpfree((char *)p_ub);
    
    
    /**************************************************************************
     * Get one section, with type checks (FAIL)
     **************************************************************************/
    if (NULL==(p_ub= (UBFH *)tpalloc("UBF", NULL, 1024)))
    {
        NDRX_LOG(log_error, "TESTERROR: buffer alloc failed: %s", tpstrerror(tperrno));
        FAIL_OUT(ret);
    }
    
    if (SUCCEED!=Bchg(p_ub, EX_CC_LOOKUPSECTION, 0, "xapp", 0L) ||
            
            SUCCEED!=Bchg(p_ub, EX_CC_FORMAT_KEY,0, "xstring_setting", 0L) ||
            SUCCEED!=Bchg(p_ub, EX_CC_FORMAT_FORMAT, 0, "s..6", 0L))
    {
        NDRX_LOG(log_error, "TESTERROR: buffer failed to setup: %s", 
                Bstrerror(Berror));
        FAIL_OUT(ret);
    }
    
    if (SUCCEED == tpcall("@CCONF", (char *)p_ub, 0L, (char **)&p_ub, &rsplen,0))
    {
        NDRX_LOG(log_error, "TESTERROR: @CCONF must FAIL!");
        FAIL_OUT(ret);
    }
    else
    {
        /* check the error codes 
         * Should be something like this:
        EX_NERROR_CODE  7 - invalid format
        EX_NERROR_MSG   xstring_setting
         * */
             
        if (FAIL==(occ1=CBfindocc (p_ub, EX_NERROR_CODE, "7", 0, BFLD_STRING)))
        {
            NDRX_LOG(log_error, "TESTERROR: EX_NERROR_CODE with value [7] "
                    "not found: %s", Bstrerror(Berror));
            FAIL_OUT(ret);
        }

        if (FAIL==(occ2=CBfindocc (p_ub, EX_NERROR_MSG, "xstring_setting", 0, BFLD_STRING)))
        {
            NDRX_LOG(log_error, "TESTERROR: EX_CC_VALUE with value [xstring_setting] "
                    "not found: %s", Bstrerror(Berror));
            FAIL_OUT(ret);
        }

        if (occ1!=occ2)
        {
            NDRX_LOG(log_error, "Invalid occurrences: %d vs %d", occ1, occ2);
            FAIL_OUT(ret);
        }
    }
    
    Bdelall(p_ub, EX_NERROR_CODE);
    Bdelall(p_ub, EX_NERROR_MSG);
    
    if (SUCCEED!=Bchg(p_ub, EX_CC_LOOKUPSECTION, 0, "xapp", 0L) ||
            
            SUCCEED!=Bchg(p_ub, EX_CC_FORMAT_KEY,0, "xfloat_setting", 0L) ||
            SUCCEED!=Bchg(p_ub, EX_CC_FORMAT_FORMAT, 0, "n1..1", 0L))
    {
        NDRX_LOG(log_error, "TESTERROR: buffer failed to setup: %s", 
                Bstrerror(Berror));
        FAIL_OUT(ret);
    }
    
    if (SUCCEED == tpcall("@CCONF", (char *)p_ub, 0L, (char **)&p_ub, &rsplen,0))
    {
        NDRX_LOG(log_error, "TESTERROR: @CCONF must FAIL!");
        FAIL_OUT(ret);
    }
    else
    {
        /* check the error codes 
         * Should be something like this:
        EX_NERROR_CODE  7 - invalid format
        EX_NERROR_MSG   xstring_setting
         * */
             
        if (FAIL==(occ1=CBfindocc (p_ub, EX_NERROR_CODE, "7", 0, BFLD_STRING)))
        {
            NDRX_LOG(log_error, "TESTERROR: EX_NERROR_CODE with value [7] "
                    "not found: %s", Bstrerror(Berror));
            FAIL_OUT(ret);
        }

        if (FAIL==(occ2=CBfindocc (p_ub, EX_NERROR_MSG, "xfloat_setting", 0, BFLD_STRING)))
        {
            NDRX_LOG(log_error, "TESTERROR: EX_CC_VALUE with value [xfloat_setting] "
                    "not found: %s", Bstrerror(Berror));
            FAIL_OUT(ret);
        }

        if (occ1!=occ2)
        {
            NDRX_LOG(log_error, "Invalid occurrences: %d vs %d", occ1, occ2);
            FAIL_OUT(ret);
        }
    }
    
    
    Bdelall(p_ub, EX_NERROR_CODE);
    Bdelall(p_ub, EX_NERROR_MSG);
    
    if (SUCCEED!=Bchg(p_ub, EX_CC_LOOKUPSECTION, 0, "xapp", 0L) ||
            
            SUCCEED!=Bchg(p_ub, EX_CC_FORMAT_KEY,0, "xinteger_setting", 0L) ||
            SUCCEED!=Bchg(p_ub, EX_CC_FORMAT_FORMAT, 0, "i..10", 0L))
    {
        NDRX_LOG(log_error, "TESTERROR: buffer failed to setup: %s", 
                Bstrerror(Berror));
        FAIL_OUT(ret);
    }
    
    if (SUCCEED == tpcall("@CCONF", (char *)p_ub, 0L, (char **)&p_ub, &rsplen,0))
    {
        NDRX_LOG(log_error, "TESTERROR: @CCONF must FAIL!");
        FAIL_OUT(ret);
    }
    else
    {
        /* check the error codes 
         * Should be something like this:
        EX_NERROR_CODE  7 - invalid format
        EX_NERROR_MSG   xstring_setting
         * */
             
        if (FAIL==(occ1=CBfindocc (p_ub, EX_NERROR_CODE, "7", 0, BFLD_STRING)))
        {
            NDRX_LOG(log_error, "TESTERROR: EX_NERROR_CODE with value [7] "
                    "not found: %s", Bstrerror(Berror));
            FAIL_OUT(ret);
        }

        if (FAIL==(occ2=CBfindocc (p_ub, EX_NERROR_MSG, "xinteger_setting", 0, BFLD_STRING)))
        {
            NDRX_LOG(log_error, "TESTERROR: EX_CC_VALUE with value [xinteger_setting] "
                    "not found: %s", Bstrerror(Berror));
            FAIL_OUT(ret);
        }

        if (occ1!=occ2)
        {
            NDRX_LOG(log_error, "Invalid occurrences: %d vs %d", occ1, occ2);
            FAIL_OUT(ret);
        }
    }
    
    
    Bdelall(p_ub, EX_NERROR_CODE);
    Bdelall(p_ub, EX_NERROR_MSG);
    
    if (SUCCEED!=Bchg(p_ub, EX_CC_LOOKUPSECTION, 0, "xapp", 0L) ||
            
            SUCCEED!=Bchg(p_ub, EX_CC_FORMAT_KEY,0, "xtrue_setting", 0L) ||
            SUCCEED!=Bchg(p_ub, EX_CC_FORMAT_FORMAT, 0, "t", 0L))
    {
        NDRX_LOG(log_error, "TESTERROR: buffer failed to setup: %s", 
                Bstrerror(Berror));
        FAIL_OUT(ret);
    }
    
    if (SUCCEED == tpcall("@CCONF", (char *)p_ub, 0L, (char **)&p_ub, &rsplen,0))
    {
        NDRX_LOG(log_error, "TESTERROR: @CCONF must FAIL!");
        FAIL_OUT(ret);
    }
    else
    {
        /* check the error codes 
         * Should be something like this:
        EX_NERROR_CODE  7 - invalid format
        EX_NERROR_MSG   xstring_setting
         * */
             
        if (FAIL==(occ1=CBfindocc (p_ub, EX_NERROR_CODE, "7", 0, BFLD_STRING)))
        {
            NDRX_LOG(log_error, "TESTERROR: EX_NERROR_CODE with value [7] "
                    "not found: %s", Bstrerror(Berror));
            FAIL_OUT(ret);
        }

        if (FAIL==(occ2=CBfindocc (p_ub, EX_NERROR_MSG, "xtrue_setting", 0, BFLD_STRING)))
        {
            NDRX_LOG(log_error, "TESTERROR: EX_CC_VALUE with value [xtrue_setting] "
                    "not found: %s", Bstrerror(Berror));
            FAIL_OUT(ret);
        }

        if (occ1!=occ2)
        {
            NDRX_LOG(log_error, "Invalid occurrences: %d vs %d", occ1, occ2);
            FAIL_OUT(ret);
        }
    }
    
    
    Bdelall(p_ub, EX_NERROR_CODE);
    Bdelall(p_ub, EX_NERROR_MSG);
    
    if (SUCCEED!=Bchg(p_ub, EX_CC_LOOKUPSECTION, 0, "xapp", 0L) ||
            
            SUCCEED!=Bchg(p_ub, EX_CC_FORMAT_KEY,0, "xfalse_setting", 0L) ||
            SUCCEED!=Bchg(p_ub, EX_CC_FORMAT_FORMAT, 0, "t", 0L))
    {
        NDRX_LOG(log_error, "TESTERROR: buffer failed to setup: %s", 
                Bstrerror(Berror));
        FAIL_OUT(ret);
    }
    
    if (SUCCEED == tpcall("@CCONF", (char *)p_ub, 0L, (char **)&p_ub, &rsplen,0))
    {
        NDRX_LOG(log_error, "TESTERROR: @CCONF must FAIL!");
        FAIL_OUT(ret);
    }
    else
    {
        /* check the error codes 
         * Should be something like this:
        EX_NERROR_CODE  7 - invalid format
        EX_NERROR_MSG   xstring_setting
         * */
             
        if (FAIL==(occ1=CBfindocc (p_ub, EX_NERROR_CODE, "7", 0, BFLD_STRING)))
        {
            NDRX_LOG(log_error, "TESTERROR: EX_NERROR_CODE with value [7] "
                    "not found: %s", Bstrerror(Berror));
            FAIL_OUT(ret);
        }

        if (FAIL==(occ2=CBfindocc (p_ub, EX_NERROR_MSG, "xfalse_setting", 0, BFLD_STRING)))
        {
            NDRX_LOG(log_error, "TESTERROR: EX_CC_VALUE with value [xfalse_setting] "
                    "not found: %s", Bstrerror(Berror));
            FAIL_OUT(ret);
        }

        if (occ1!=occ2)
        {
            NDRX_LOG(log_error, "Invalid occurrences: %d vs %d", occ1, occ2);
            FAIL_OUT(ret);
        }
    }
    
    
    /**************************************************************************
     * Check for mandatory field
     **************************************************************************/
    Bdelall(p_ub, EX_NERROR_CODE);
    Bdelall(p_ub, EX_NERROR_MSG);
    
    if (SUCCEED!=Bchg(p_ub, EX_CC_LOOKUPSECTION, 0, "x2app", 0L) ||
            
            SUCCEED!=Bchg(p_ub, EX_CC_MANDATORY,0, "xtrue_setting", 0L))
    {
        NDRX_LOG(log_error, "TESTERROR: buffer failed to setup: %s", 
                Bstrerror(Berror));
        FAIL_OUT(ret);
    }
    
    if (SUCCEED == tpcall("@CCONF", (char *)p_ub, 0L, (char **)&p_ub, &rsplen,0))
    {
        NDRX_LOG(log_error, "TESTERROR: @CCONF must FAIL!");
        FAIL_OUT(ret);
    }
    else
    {
        /* check the error codes 
         * Should be something like this:
        EX_NERROR_CODE  6 - mandatory field missing
        EX_NERROR_MSG   xtrue_setting
         * */
             
        if (FAIL==(occ1=CBfindocc (p_ub, EX_NERROR_CODE, "6", 0, BFLD_STRING)))
        {
            NDRX_LOG(log_error, "TESTERROR: EX_NERROR_CODE with value [6] "
                    "not found: %s", Bstrerror(Berror));
            FAIL_OUT(ret);
        }

        if (FAIL==(occ2=CBfindocc (p_ub, EX_NERROR_MSG, "xtrue_setting", 0, BFLD_STRING)))
        {
            NDRX_LOG(log_error, "TESTERROR: EX_CC_VALUE with value [xtrue_setting] "
                    "not found: %s", Bstrerror(Berror));
            FAIL_OUT(ret);
        }

        if (occ1!=occ2)
        {
            NDRX_LOG(log_error, "Invalid occurrences: %d vs %d", occ1, occ2);
            FAIL_OUT(ret);
        }
    }
    
    /**************************************************************************
     * Call the section listing
     **************************************************************************/
    
    if (SUCCEED!=tpterm())
    {
        NDRX_LOG(log_error, "tpterm failed with: %s", tpstrerror(tperrno));
        ret=FAIL;
        goto out;
    }
    
out:
    return ret;
}

