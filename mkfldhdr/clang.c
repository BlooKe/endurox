/* 
** 'C' lang support
**
** @file clang.c
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

#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <atmi.h>

#include <ubf.h>
#include <ferror.h>
#include <fieldtable.h>
#include <fdatatype.h>

#include <ndrstandard.h>
#include <ndebug.h>
#include "mkfldhdr.h"
/*---------------------------Externs------------------------------------*/
/*---------------------------Macros-------------------------------------*/
/*---------------------------Enums--------------------------------------*/
/*---------------------------Typedefs-----------------------------------*/
/*---------------------------Globals------------------------------------*/
/*---------------------------Statics------------------------------------*/
/*---------------------------Prototypes---------------------------------*/

/**
 * Get the c lang output file name
 * @param data
 */
public void c_get_fullname(char *data)
{
    sprintf(data, "%s/%s.h", G_output_dir, G_active_file);
}

/**
 * Write text line to output file
 * @param text
 * @return
 */
public int c_put_text_line (char *text)
{
    int ret=SUCCEED;
    
    fprintf(G_outf, "%s", text);
    
    /* Check errors */
    if (ferror(G_outf))
    {
        _Fset_error_fmt(BFTOPEN, "Failed to write to output file: [%s]", strerror(errno));
        ret=FAIL;
    }

    return ret;
}

/**
 * Process the baseline
 * @param base
 * @return
 */
public int c_put_got_base_line(char *base)
{

    int ret=SUCCEED;

    fprintf(G_outf, "/*\tfname\tbfldid            */\n"
                    "/*\t-----\t-----            */\n");

    /* Check errors */
    if (ferror(G_outf))
    {
        _Fset_error_fmt(BFTOPEN, "Failed to write to output file: [%s]", strerror(errno));
        ret=FAIL;
    }

    return ret;
}

/**
 * Write definition to output file
 * @param def
 * @return
 */
public int c_put_def_line (UBF_field_def_t *def)
{
    int ret=SUCCEED;
    int type = def->bfldid>>EFFECTIVE_BITS;
    BFLDID number = def->bfldid & EFFECTIVE_BITS_MASK;

    fprintf(G_outf, "#define\t%s\t((BFLDID32)%d)\t/* number: %d\t type: %s */\n",
            def->fldname, def->bfldid, number,
            G_dtype_str_map[type].fldname);
    
    /* Check errors */
    if (ferror(G_outf))
    {
        _Fset_error_fmt(BFTOPEN, "Failed to write to output file: [%s]", strerror(errno));
        ret=FAIL;
    }

    return ret;
}

/**
 * Output file have been open
 * @param fname
 * @return 
 */
public int c_file_open (char *fname)
{
    return SUCCEED;
}

/**
 * Output file have been closed
 * @param fname
 * @return 
 */
public int c_file_close (char *fname)
{
    return SUCCEED;
}

