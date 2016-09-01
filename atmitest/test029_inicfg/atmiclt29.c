/* 
** Basic INI tests...
**
** @file atmiclt29.c
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
#include "test029.h"
#include <inicfg.h>
#include <stdarg.h>     /* va_list, va_start, va_arg, va_end */
#include <exhash.h>
#include <nerror.h>
/*---------------------------Externs------------------------------------*/
/*---------------------------Macros-------------------------------------*/
/*---------------------------Enums--------------------------------------*/
/*---------------------------Typedefs-----------------------------------*/
/*---------------------------Globals------------------------------------*/
/*---------------------------Statics------------------------------------*/
/*---------------------------Prototypes---------------------------------*/

/*
 * Main test case entry
 */
int main(int argc, char** argv)
{
    int ret = SUCCEED;
    ndrx_inicfg_t *cfg;
    ndrx_inicfg_section_keyval_t *out = NULL;
    ndrx_inicfg_section_keyval_t *val;
    ndrx_inicfg_section_t *sections, *iter, *iter_tmp;
    char *iterfilter[] = {"my", NULL};
    int i;
    
    for (i=0; i<10000; i++)
    {
        out = NULL;
        
        if (NULL==(cfg=ndrx_inicfg_new()))
        {
            NDRX_LOG(log_error, "TESTERROR: failed to make inicfg: %s", Nstrerror(Nerror));
            FAIL_OUT(ret);
        }

        /* any sections */
        if (SUCCEED!=ndrx_inicfg_add(cfg, "./cfg_folder1", NULL))
        {
             NDRX_LOG(log_error, "TESTERROR: failed to add resource: %s", Nstrerror(Nerror));
            FAIL_OUT(ret);       
        }

        if (SUCCEED!=ndrx_inicfg_add(cfg, "A_test.ini", NULL))
        {
            NDRX_LOG(log_error, "TESTERROR: failed to make resource: %s", Nstrerror(Nerror));
            FAIL_OUT(ret);        
        }

        if (SUCCEED!=ndrx_inicfg_add(cfg, "B_test.ini", NULL))
        {
            NDRX_LOG(log_error, "TESTERROR: failed to make resource: %s", Nstrerror(Nerror));
            FAIL_OUT(ret);    
        }


        /* resource == NULL, any resource. */
        if (SUCCEED!=ndrx_inicfg_get_subsect(cfg, NULL, "mysection/subsect1/f/f", &out))
        {
            NDRX_LOG(log_error, "TESTERROR: Failed to resolve [mysection/subsect1/f/f]: %s",
                    Nstrerror(Nerror));
            FAIL_OUT(ret);    
        }


        /* get the value from  'out' - should be 4 
         * As firstly it will get the stuff 
         * [mysection/subsect1/f]
         * MYVALUE1=3
         * and then it will get the exact value from B_test.ini
         */

        if (NULL==(val=ndrx_keyval_hash_get(out, "MYVALUE1")))
        {
            NDRX_LOG(log_error, "TESTERROR: Failed to get MYVALUE1!");
            FAIL_OUT(ret);
        }

        if (0!=strcmp(val->val, "4"))
        {
            NDRX_LOG(log_error, "TESTERROR: [mysection/subsect1/f/f] not 4!");
            FAIL_OUT(ret);
        }

        /* free the list */
        ndrx_keyval_hash_free(out);

        /* try some iteration over the config */
        sections = NULL;
        iter = NULL;
        iter_tmp = NULL;

        if (SUCCEED!=ndrx_inicfg_iterate(cfg, NULL, iterfilter, &sections))
        {
            NDRX_LOG(log_error, "TESTERROR: Failed to iterate config!");
            FAIL_OUT(ret);
        }

        /* print the stuff we have in config */

        EXHASH_ITER(hh, sections, iter, iter_tmp)
        {
            NDRX_LOG(log_info, "iter: section [%s]", iter->section);
            
            if (0==strcmp(iter->section, "mytest"))
            {
                /* we should have "THIS" key there */
                if (NULL==(val=ndrx_keyval_hash_get(iter->values, "THIS")))
                {
                    NDRX_LOG(log_error, "TESTERROR: Failed to get THIS of [mytest]!");
                    FAIL_OUT(ret);
                }

                if (0!=strcmp(val->val, "IS TEST"))
                {
                    NDRX_LOG(log_error, "TESTERROR: [mytest]/THIS not "
                            "equal to 'IS TEST' but [%s]!", 
                            val->val);
                    FAIL_OUT(ret);
                }
            }
        }
        
        /* kill the section listing */
        ndrx_inicfg_sections_free(sections);
        
        /* free up the config */
        ndrx_inicfg_free(cfg);
    }
    
out:
    NDRX_LOG(log_info, "Test returns %d", ret);
    return ret;
}

