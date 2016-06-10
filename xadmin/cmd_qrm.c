/* 
** Remove all matched queues
**
** @file cmd_qrm.c
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
#include <sys/param.h>

#include <ndrstandard.h>
#include <ndebug.h>

#include <ndrx.h>
#include <ndrxdcmn.h>
#include <atmi_int.h>
#include <gencall.h>
#include <nclopt.h>
#include <sys_unix.h>
/*---------------------------Externs------------------------------------*/
/*---------------------------Macros-------------------------------------*/
/*---------------------------Enums--------------------------------------*/
/*---------------------------Typedefs-----------------------------------*/
/*---------------------------Globals------------------------------------*/
/*---------------------------Statics------------------------------------*/
/*---------------------------Prototypes---------------------------------*/

/**
 * Remove specific q
 * @param p_cmd_map
 * @param argc
 * @param argv
 * @return SUCCEED
 */
public int cmd_qrm(cmd_mapping_t *p_cmd_map, int argc, char **argv, int *p_have_next)
{
    int ret=SUCCEED;
    int i;
    string_list_t* qlist = NULL;
    
    if (argc>=2)
    {
        if (NULL!=(qlist = ndrx_sys_mqueue_list_make(G_atmi_env.qpath, &ret)))
        {
            for (i=1; i<argc; i++)
            {
                if (0==strcmp(qlist->qname, argv[i]))
                {
                    printf("Removing [%s] ...", qlist->qname);
                
                    if (SUCCEED==ndrx_mq_unlink(qlist->qname))
                    {
                        printf("SUCCEED\n");
                    }
                    else
                    {
                        printf("FAIL\n");
                    }
                }
            }
        }
    }
    else
    {
        FAIL_OUT(ret);
    }
    
out:

    ndrx_string_list_free(qlist);
    return ret;
}


/**
 * Remove all queues
 * @param p_cmd_map
 * @param argc
 * @param argv
 * @return SUCCEED
 */
public int cmd_qrmall(cmd_mapping_t *p_cmd_map, int argc, char **argv, int *p_have_next)
{
    int ret=SUCCEED;
    int i;
    string_list_t* qlist = NULL;
    
    if (argc>=2)
    {
        if (NULL!=(qlist = ndrx_sys_mqueue_list_make(G_atmi_env.qpath, &ret)))
        {
            for (i=1; i<argc; i++)
            {
                if (NULL!=strstr(qlist->qname, argv[i]))
                {
                    printf("Removing [%s] ...", qlist->qname);
             

                    if (SUCCEED==ndrx_mq_unlink(qlist->qname))
                    {
                        printf("SUCCEED\n");
                    }
                    else
                    {
                        printf("FAIL\n");
                    }
                }
            }
        }
    }
    else
    {
        FAIL_OUT(ret);
    }
    
out:

    ndrx_string_list_free(qlist);
    return ret;
}
