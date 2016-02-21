/* 
** NDR 'standard' common utilites
** Enduro Execution system platform library
**
** @file nstdutil.c
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
#include <ndrstandard.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "nstdutil.h"
/*---------------------------Externs------------------------------------*/
/*---------------------------Macros-------------------------------------*/
#define _MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
/*---------------------------Enums--------------------------------------*/
/*---------------------------Typedefs-----------------------------------*/
/*---------------------------Globals------------------------------------*/
/*---------------------------Statics------------------------------------*/
/*---------------------------Prototypes---------------------------------*/


/**
 * Return timstamp UTC, milliseconds since epoch date.
 */
public unsigned long long nstdutil_utc_tstamp(void)
{
    struct timeval tv;
    unsigned long long ret;

    gettimeofday(&tv, NULL);

    ret =
        (unsigned long long)(tv.tv_sec) * 1000 +
        (unsigned long long)(tv.tv_usec) / 1000;
    
    return ret;
}

/**
 * Return date/time local 
 * @param p_date - ptr to store long date, format YYYYMMDD
 * @param p_time - ptr to store long time, format HHMISS
 */
public void nstdutil_get_dt_local(long *p_date, long *p_time)
{
        struct tm       *p_tm;
        long            lret;
        struct timeval  timeval;
        struct timezone timezone_val;

        gettimeofday (&timeval, &timezone_val);
        p_tm = localtime(&timeval.tv_sec);
        *p_time = 10000L*p_tm->tm_hour+100*p_tm->tm_min+1*p_tm->tm_sec;
        *p_date = 10000L*(1900 + p_tm->tm_year)+100*(1+p_tm->tm_mon)+1*(p_tm->tm_mday);

        return;
}

/**
 * Substitute environment 
 * @param str
 * @return 
 */
public char * nstdutil_str_env_subs(char * str)
{
    char *p;
    char *next = str;
    char envnm[1024];
    char *env;
    char *out;
    char *empty="";
    while (NULL!=(p=strstr(next, "${")))
    {
        char *close =strchr(next, '}');
        if (NULL!=close)
        {
            int cpylen = close-p-2;
            int envlen;
            /* do substitution */
            strncpy(envnm, p+2, cpylen);
            envnm[cpylen] = EOS;
            env = getenv(envnm);
            if (NULL!=env)
                out = env;
            else
                out = empty;

            envlen = strlen(out);
            
            /* fix up the buffer!!! */
            if (cpylen+3==envlen)
            {
                memcpy(p, out, envlen);
            }
            else if (cpylen+3 > envlen)
            {
                /*int overleft = cpylen+2 - envlen; */
                /* copy there, and reduce total len */
                memcpy(p, out, envlen);
                /* copy left overs after } at the end of the env, including eos */
                memmove(p+envlen, close+1, strlen(close+1)+1);
            }
            else if (cpylen+3 < envlen)
            {
                int missing = envlen - (cpylen+2);
                
                /* we have to stretch that stuff and then copy in, including eos */
                memmove(close+missing, close+1, strlen(close+1)+1);
                memcpy(p, out, envlen);
            }
            next = p+envlen;
        }
        else
        {
            /* just step forward... */
            next+=2;
        }
    }

    return str;
}

/**
 * Decode number
 * @param t
 * @param slot
 * @return 
 */
char *nstdutil_decode_num(long tt, int slot, int level, int levels)
{
    static char text[20][128];
    char tmp[128];
    long next_t=0;
    long t = tt;
#define DEC_K  ((long)1000)
#define DEC_M  ((long)1000*1000)
#define DEC_B  ((long)1000*1000*1000)
#define DEC_T  ((long long)1000*1000*1000*1000)

    
    level++;

    if ((double)t/DEC_K < 1.0) /* Less that thousand */
    {
        sprintf(tmp, "%ld", t);
    }
    else if ((double)t/DEC_M < 1.0) /* less than milliion */
    {
        sprintf(tmp, "%ldK", t/DEC_K);
        
        if (level<levels)
            next_t = t%DEC_K;
    }
    else if ((double)t/DEC_B < 1.0) /* less that billion */
    {
        sprintf(tmp, "%ldM", t/DEC_M);
        
        if (level<levels)
            next_t = t%DEC_M;
    }
    else if ((double)t/DEC_T < 1.0) /* less than trillion */
    {
        sprintf(tmp, "%ldB", t/DEC_B);
        
        if (level<levels)
            next_t = t%DEC_B;
    }
    
    if (level==1)
        strcpy(text[slot], tmp);
    else
        strcat(text[slot], tmp);
    
    if (next_t)
        nstdutil_decode_num(next_t, slot, level, levels);
    
    return text[slot];
}

/**
 * Strip specified char from string
 * @param haystack - string to strip
 * @param needle - chars to strip
 * @return 
 */
public char *nstdutil_str_strip(char *haystack, char *needle)
{
    char *dest;
    char *src;
    int len = strlen(needle);
    int i;
    int have_found;
    dest = src = haystack;

    for (; EOS!=*src; src++)
    {
        have_found = FALSE;
        for (i=0; i<len; i++)
        {
            if (*src == needle[i])
            {
                have_found = TRUE;
                continue;
            }
        }
        /* Copy only if have found... */
        if (!have_found)
        {
            *dest = *src;
            dest++;
        }
    }
    
    *dest = EOS;
    
    return haystack;
}


/**
 * Returns the string mapped to long value
 * @param map - mapping table
 * @param val - value to map
 * @param endval - List end/default value
 * @return ptr to maping str
 */
public char *dolongstrgmap(longstrmap_t *map, long val, long endval)
{
    do 
    {
        if (map->from == val)
        {
            return map->to;
        }
        map++;
    } while (endval!=map->from);
    
    return map->to;
}


/**
 * Returns the string mapped to long value
 * @param map - mapping table
 * @param val - value to map
 * @param endval - List end/default value
 * @return ptr to maping str
 */
public char *docharstrgmap(longstrmap_t *map, char val, char endval)
{
    do 
    {
        if (map->from == val)
        {
            return map->to;
        }
        map++;
    } while (endval!=map->from);
    
    return map->to;
}


/**
 * Get thread id (not the very portable way...)
 * @return 
 */
public uint64_t ndrx_gettid(void) 
{
    pthread_t ptid = pthread_self();
    uint64_t threadId = 0;
    memcpy(&threadId, &ptid, _MIN(sizeof(threadId), sizeof(ptid)));
    return threadId;
}
