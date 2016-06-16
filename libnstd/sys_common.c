/* 
** Commons for system abstractions
**
** @file sys_common.c
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

/*---------------------------Includes-----------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <unistd.h>
#include <stdarg.h>
#include <ctype.h>
#include <memory.h>
#include <errno.h>
#include <signal.h>
#include <limits.h>
#include <pthread.h>
#include <string.h>
#include <dirent.h>

#include <ndrstandard.h>
#include <ndebug.h>
#include <nstdutil.h>
#include <limits.h>

#include <sys_unix.h>

#include <utlist.h>
#include <pwd.h>



/*---------------------------Externs------------------------------------*/
/*---------------------------Macros-------------------------------------*/
/*---------------------------Enums--------------------------------------*/
/*---------------------------Typedefs-----------------------------------*/
/*---------------------------Globals------------------------------------*/
/*---------------------------Statics------------------------------------*/
/*---------------------------Prototypes---------------------------------*/

/**
 * Free the list of message queues
 */
public void ndrx_string_list_free(string_list_t* list)
{
    string_list_t *elt, *tmp;
    
    if (NULL!=list)
    {
        LL_FOREACH_SAFE(list,elt,tmp) 
        {
            LL_DELETE(list,elt);
            if (NULL!=elt->qname)
            {
                free(elt->qname);
            }
            
            free((char *)elt);
        }
    }
}


/**
 * List processes by filters
 * @param filter1
 * @param filter2
 * @param filter3
 * @return 
 */
public string_list_t * ndrx_sys_ps_list(char *filter1, char *filter2, char *filter3, char *filter4)
{
    FILE *fp=NULL;
    char cmd[128];
    char path[PATH_MAX];
    int ok;
    int i;
    string_list_t* tmp;
    string_list_t* ret = NULL;
    char *filter[4] = {filter1, filter2, filter3, filter4};
    
    sprintf(cmd, "ps -ef");
    
    NDRX_LOG(log_debug, "Listing processes [%s] f1=[%s] f2=[%s] f3=[%s] f4=[%s]", 
            cmd, filter1, filter2, filter3, filter4);
    
    /* Open the command for reading. */
    fp = popen(cmd, "r");
    if (fp == NULL)
    {
        NDRX_LOG(log_warn, "failed to run command [%s]: %s", cmd, strerror(errno));
        goto out;
    }
    
    /* Check the process name in output... */
    while (fgets(path, sizeof(path)-1, fp) != NULL)
    {
     /*   NDRX_LOG(log_debug, "Got line [%s]", path); - causes locks... */
        ok = 0;
        
        for (i = 0; i<4; i++)
        {
            if (EOS!=filter[i][0] && strstr(path, filter[i]))
            {
                /* NDRX_LOG(log_debug, "filter%d [%s] - ok", i, filter[i]); */
                ok++;
            }
            else if (EOS==filter[i][0])
            {
                /* NDRX_LOG(log_debug, "filter%d [%s] - ok", i, filter[i]); */
                ok++;
            }
            else
            {
                /* NDRX_LOG(log_debug, "filter%d [%s] - fail", i, filter[i]); */
            }
        }
        
        if (4==ok)
        {
            if (NULL==(tmp = calloc(1, sizeof(string_list_t))))
            {
                
                /* close */
                if (fp!=NULL)
                {
                    fp = NULL;
                    pclose(fp);
                }

                NDRX_LOG(log_always, "alloc of string_list_t (%d) failed: %s", 
                        sizeof(string_list_t), strerror(errno));
                ndrx_string_list_free(ret);
                ret = NULL;
                goto out;
            }
            
            if (NULL==(tmp->qname = malloc(strlen(path)+1)))
            {
                /* close */
                if (fp!=NULL)
                {
                    fp = NULL;
                    pclose(fp);
                }
                
                NDRX_LOG(log_always, "alloc of %d bytes failed: %s", 
                        strlen(path)+1, strerror(errno));
                free(tmp);
                ndrx_string_list_free(ret);
                ret =  NULL;
                goto out;
            }
            
            strcpy(tmp->qname, path);
            
            LL_APPEND(ret, tmp);
        }
    }

out:
    /* close */
    if (fp!=NULL)
    {
        pclose(fp);
    }

    return ret;
    
}

/**
 * Get current system username
 */
public char *ndrx_sys_get_cur_username(void)
{
    uid_t uid = geteuid();
    struct passwd *pw = getpwuid(uid);
    if (pw)
    {
        return pw->pw_name;
    }

    return "";
}

/**
 * List the contents of the folder
 */
public string_list_t* ndrx_sys_folder_list(char *path, int *return_status)
{
    string_list_t* ret = NULL;
    struct dirent **namelist;
    int n;
    string_list_t* tmp;
    int len;
    
    *return_status = SUCCEED;
    
    n = scandir(path, &namelist, 0, alphasort);
    if (n < 0)
    {
        NDRX_LOG(log_error, "Failed to open queue directory [%s]: %s", 
                path, strerror(errno));
        goto exit_fail;
    }
    else 
    {
        while (n--)
        {
            if (0==strcmp(namelist[n]->d_name, ".") || 
                        0==strcmp(namelist[n]->d_name, ".."))
            {
                free(namelist[n]);
                continue;
            }
            
            len = 1 /* / */ + strlen(namelist[n]->d_name) + 1 /* EOS */;
            
            if (NULL==(tmp = calloc(1, sizeof(string_list_t))))
            {
                NDRX_LOG(log_always, "alloc of mq_list_t (%d) failed: %s", 
                        sizeof(string_list_t), strerror(errno));
                
                
                goto exit_fail;
            }
            
            if (NULL==(tmp->qname = malloc(len)))
            {
                NDRX_LOG(log_always, "alloc of %d bytes failed: %s", 
                        len, strerror(errno));
                free(tmp);
                goto exit_fail;
            }
            
            
            strcpy(tmp->qname, "/");
            strcat(tmp->qname, namelist[n]->d_name);
            
            /* Add to LL */
            LL_APPEND(ret, tmp);
            
            free(namelist[n]);
        }
        free(namelist);
    }
    
    return ret;
    
exit_fail:

    *return_status = FAIL;

    if (NULL!=ret)
    {
        ndrx_string_list_free(ret);
        ret = NULL;
    }

    return ret;   
}



