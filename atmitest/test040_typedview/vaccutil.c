/* 
** View access unit test, utility funcs
**
** @file vaccutil.c
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <cgreen/cgreen.h>
#include <ubf.h>
#include <ndrstandard.h>
#include <string.h>
#include "test.fd.h"
#include "ubfunit1.h"
#include "ndebug.h"
#include <fdatatype.h>

#include "test040.h"

/**
 * Basic preparation before the test
 */
exprivate void basic_setup(void)
{
    
}

exprivate void basic_teardown(void)
{
    
}

/**
 * Test Bvget as int
 */
Ensure(test_Bvsizeof)
{
    struct MYVIEW1 v;
    struct MYVIEW3 v3;
    
    assert_equal(Bvsizeof("MYVIEW1"), sizeof(v));
    assert_equal(Bvsizeof("MYVIEW3"), sizeof(v3));
}


/**
 * Test Bvoccur
 */
Ensure(test_Bvoccur)
{
    struct MYVIEW1 v;
    BFLDOCC maxocc;
    BFLDOCC realocc;
    
    init_MYVIEW1(&v);
    
    assert_equal(Bvoccur((char *)&v, "MYVIEW1", "tshort1", &maxocc, &realocc), 1);
    assert_equal(maxocc, 1);
    assert_equal(realocc, 1);
    
    assert_equal(Bvoccur((char *)&v, "MYVIEW1", "tshort2", &maxocc, &realocc), 2);
    assert_equal(maxocc, 2);
    assert_equal(realocc, 2);
    
    assert_equal(Bvoccur((char *)&v, "MYVIEW1", "tshort3", &maxocc, &realocc), 2);
    assert_equal(maxocc, 3);
    assert_equal(realocc, 3);
    
    
    assert_equal(Bvoccur((char *)&v, "MYVIEW1", "tshort4", &maxocc, &realocc), 1);
    assert_equal(maxocc, 1);
    assert_equal(realocc, 1);
    
    v.tint2[1] = 0;
    assert_equal(Bvoccur((char *)&v, "MYVIEW1", "tint2", &maxocc, &realocc), 2);
    assert_equal(maxocc, 2);
    assert_equal(realocc, 1); /* due to last element is NULL */
    
    assert_equal(Bvoccur((char *)&v, "MYVIEW1", "tchar2", &maxocc, &realocc), 5);
    assert_equal(maxocc, 5);
    assert_equal(realocc, 5);
    
    v.tchar2[4]='A';
    v.tchar2[3]='A';
    
    assert_equal(Bvoccur((char *)&v, "MYVIEW1", "tchar2", &maxocc, &realocc), 5);
    assert_equal(maxocc, 5);
    assert_equal(realocc, 3);
    
    
    assert_equal(Bvoccur((char *)&v, "MYVIEW1", "tchar3", &maxocc, &realocc), 0);
    assert_equal(maxocc, 2);
    assert_equal(realocc, 2);
    
    
    assert_equal(Bvoccur((char *)&v, "MYVIEW1", "tfloat1", &maxocc, &realocc), 4);
    assert_equal(maxocc, 4);
    assert_equal(realocc, 4);
    
    
    assert_equal(Bvoccur((char *)&v, "MYVIEW1", "tstring0", &maxocc, &realocc), 3);
    assert_equal(maxocc, 3);
    assert_equal(realocc, 3);
    
    
    assert_equal(Bvoccur((char *)&v, "MYVIEW1", "tstring2", &maxocc, &realocc), 2);
    assert_equal(maxocc, 3);
    assert_equal(realocc, 3);
    
}


/**
 * Test Bvsetoccur
 */
Ensure(test_Bvsetoccur)
{
    struct MYVIEW1 v;
    BFLDOCC maxocc;
    BFLDOCC realocc;
    
    memset(&v, 0, sizeof(v));
    
    v.tshort2[0] =2001;
    v.tshort2[1] =2001;
    
    assert_equal(Bvoccur((char *)&v, "MYVIEW1", "tshort2", &maxocc, &realocc), 0);
    assert_equal(maxocc, 2);
    assert_equal(realocc, 0);
    
    v.C_tshort2 = 1;
    
    assert_equal(Bvoccur((char *)&v, "MYVIEW1", "tshort2", &maxocc, &realocc), 1);
    assert_equal(maxocc, 2);
    assert_equal(realocc, 0);
    
    v.C_tshort2 = 2;
    
    assert_equal(Bvoccur((char *)&v, "MYVIEW1", "tshort2", &maxocc, &realocc), 2);
    assert_equal(maxocc, 2);
    assert_equal(realocc, 0);
    
}

/**
 * Very basic tests of the framework
 * @return
 */
TestSuite *vacc_util_tests(void)
{
    TestSuite *suite = create_test_suite();
    
    set_setup(suite, basic_setup);
    set_teardown(suite, basic_teardown);

    /* init view test */
    add_test(suite, test_Bvsizeof);
    add_test(suite, test_Bvoccur);
    add_test(suite, test_Bvsetoccur);
    
    return suite;
}
