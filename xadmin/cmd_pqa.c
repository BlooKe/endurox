/* 
** `pqa' print queues, all
**
** @file cmd_pqa.c
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
#include <errno.h>
#include <memory.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ndrstandard.h>
#include <ndebug.h>

#include <ndrx.h>
#include <ndrxdcmn.h>
#include <atmi_int.h>
#include <gencall.h>

#include <ntimer.h>
#include <nclopt.h>
/*---------------------------Externs------------------------------------*/
/*---------------------------Macros-------------------------------------*/
/*---------------------------Enums--------------------------------------*/
/*---------------------------Typedefs-----------------------------------*/
/*---------------------------Globals------------------------------------*/
/*---------------------------Statics------------------------------------*/
/*---------------------------Prototypes---------------------------------*/


/**
 * Print header
 * @return
 */
private void print_hdr(void)
{
    fprintf(stderr, "Msg queued Q name\n");
    fprintf(stderr, "---------- ---------------------------------"
                    "------------------------------------\n");
}

/**
 * Get service listings
 * @param p_cmd_map
 * @param argc
 * @param argv
 * @return SUCCEED
 */
public int cmd_pqa(cmd_mapping_t *p_cmd_map, int argc, char **argv, int *p_have_next)
{
    int ret=SUCCEED;
    struct dirent **namelist;
    int n;
    short print_all = FALSE;
    struct mq_attr att;
    char q[512];
    
    ncloptmap_t clopt[] =
    {
        {'a', BFLD_SHORT, (void *)&print_all, 0, 
                                NCLOPT_OPT | NCLOPT_TRUEBOOL, "Print all"},
        {0}
    };
    
    /* parse command line */
    if (nstd_parse_clopt(clopt, TRUE,  argc, argv))
    {
        fprintf(stderr, "Invalid options, see `help'.");
        FAIL_OUT(ret);
    }
    
    /* Print header at first step! */
    print_hdr();
    
    n = scandir(G_config.qpath, &namelist, 0, alphasort);
    if (n < 0)
    {
        NDRX_LOG(log_error, "Failed to open queue directory: %s", 
                strerror(errno));
        ret=FAIL;
        goto out;
    }
    else 
    {
        while (n--)
        {
            if (0==strcmp(namelist[n]->d_name, ".") || 
                        0==strcmp(namelist[n]->d_name, ".."))
                continue;
            
            if (!print_all)
            {
                /* if not print all, then skip this queue */
                if (0!=strncmp(namelist[n]->d_name, 
                        G_config.qprefix+1, strlen(G_config.qprefix)-1))
                {
                    continue;
                }
            }
            
            strcpy(q, "/");
            strcat(q, namelist[n]->d_name);
            
            if (SUCCEED!=ndrx_get_q_attr(q, &att))
            {
                /* skip this one... */
                continue;
            }
            
            fprintf(stdout, "%-10d %s\n", (int)att.mq_curmsgs, q);
            
            free(namelist[n]);
        }
        free(namelist);
    }
    
out:
    return ret;
}
