/* 
** Enduro/X standard utilities 
**
** @file psstdexutil.cpp
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
#include <pscript.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <psstdexutil.h>
#include <ndebug.h>
#include <nstdutil.h>
#include <userlog.h>
#include <string.h>
#include <ndrx_config.h>


 #define C_TEXT( text ) ((char*)std::string( text ).c_str())

//Read the line from terminal
//@return line read string
static PSInteger _exutil_getline(HPSCRIPTVM v)
{
    char *ret = ndrx_getline();
    
    ps_pushstring(v,ret,-1);
    
    if (NULL!=ret)
    {
        NDRX_FREE(ret);
    }

    return 1;
}

//Return the compiled operating system name
static PSInteger _exutil_getosname(HPSCRIPTVM v)
{
    ps_pushstring(v,NDRX_BUILD_OS_NAME,-1);
    
    return 1;
}


//Get the current working dir
//@return   current working dir
static PSInteger _exutil_getcwd(HPSCRIPTVM v)
{
    char* ret;
    char buff[PATH_MAX + 1];
    
    ret = getcwd( buff, sizeof(buff) );
     
    ps_pushstring(v,ret,-1);
    
    
    return 1;
}

//Write user log message
//@param msg    Message
static PSInteger _exutil_userlog(HPSCRIPTVM v)
{
    const PSChar *s;
    if(PS_SUCCEEDED(ps_getstring(v,2,&s))){
        
        userlog_const (s);
        
        return 1;
    }
    return 0;
}

//TODO: We shall add ulog() here... so that we can do basic logging from install
//scripts.

#define _DECL_FUNC(name,nparams,pmask) {_SC(#name),_exutil_##name,nparams,pmask}
static PSRegFunction exutillib_funcs[]={
	_DECL_FUNC(getline,1,_SC(".s")),
        _DECL_FUNC(getcwd,1,_SC(".s")),
        _DECL_FUNC(getosname,1,_SC(".s")),
        _DECL_FUNC(userlog,2,_SC(".s")),
	{0,0}
};
#undef _DECL_FUNC

PSInteger psstd_register_exutillib(HPSCRIPTVM v)
{
    PSInteger i=0;
    while(exutillib_funcs[i].name!=0)
    {
        ps_pushstring(v,exutillib_funcs[i].name,-1);
        ps_newclosure(v,exutillib_funcs[i].f,0);
        ps_setparamscheck(v,exutillib_funcs[i].nparamscheck,exutillib_funcs[i].typemask);
        ps_setnativeclosurename(v,-1,exutillib_funcs[i].name);
        ps_newslot(v,-3,PSFalse);
        i++;
    }
    return 1;
}
