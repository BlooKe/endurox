/* 
** View unit tests.
**
** @file viewunit1.c
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

#include "t40.h"

/**
 * Basic preparation before the test
 */
void basic_setup(void)
{
    
}

void basic_teardown(void)
{
    
}


/**
 * Test NULL column
 */
Ensure(test_Bvnull)
{
    struct MYVIEW1 v;
    int i;
    char *spec_symbols = "\n\t\f\\\'\"\vHELLOWORLD\0";
    memset(&v, 0, sizeof(v));
    
    /***************************** SHORT TESTS *******************************/
    /* These fields shall not be NULL as NULL defined */
    assert_equal(Bvnull((char *)&v, "tshort2", 0, "MYVIEW1"), EXFALSE);
    assert_equal(Bvnull((char *)&v, "tshort2", 1, "MYVIEW1"), EXFALSE);
    
    /* Set to NULL */
    v.tshort2[1]=2001;
    assert_equal(Bvnull((char *)&v, "tshort2", 0, "MYVIEW1"), EXFALSE);
    assert_equal(Bvnull((char *)&v, "tshort2", 1, "MYVIEW1"), EXTRUE);
    
    /*  Set all to NULL */
    assert_equal(Bvselinit((char *)&v,"tshort2", "MYVIEW1"), EXSUCCEED);
    assert_equal(Bvnull((char *)&v, "tshort2", 0, "MYVIEW1"), EXTRUE);
    assert_equal(Bvnull((char *)&v, "tshort2", 1, "MYVIEW1"), EXTRUE);
    
    
    assert_equal(Bvnull((char *)&v, "tshort3", 0, "MYVIEW1"), EXTRUE);
    assert_equal(Bvnull((char *)&v, "tshort3", 1, "MYVIEW1"), EXTRUE);
    assert_equal(Bvnull((char *)&v, "tshort3", 2, "MYVIEW1"), EXTRUE);
    
    v.tshort3[0] = 1;
    v.tshort3[2] = 2;
    
    assert_equal(Bvnull((char *)&v, "tshort3", 0, "MYVIEW1"), EXFALSE);
    assert_equal(Bvnull((char *)&v, "tshort3", 1, "MYVIEW1"), EXTRUE);
    assert_equal(Bvnull((char *)&v, "tshort3", 2, "MYVIEW1"), EXFALSE);
    
    /* this is NONE, field set to 0, but as NONE is there it is not NULL */
    assert_equal(Bvnull((char *)&v, "tshort4", 0, "MYVIEW1"), EXFALSE);
    
    /***************************** LONG TESTS *******************************/
    
    assert_equal(Bvnull((char *)&v, "tlong1", 0, "MYVIEW1"), EXTRUE);
    
    /***************************** INT TESTS ********************************/
    
    assert_equal(Bvnull((char *)&v, "tint2", 0, "MYVIEW1"), EXTRUE);
    assert_equal(Bvnull((char *)&v, "tint2", 1, "MYVIEW1"), EXTRUE);
    assert_equal(Bvnull((char *)&v, "tint3", 0, "MYVIEW1"), EXFALSE);
    assert_equal(Bvselinit((char *)&v,"tint3", "MYVIEW1"), EXSUCCEED);
    assert_equal(Bvnull((char *)&v, "tint3", 0, "MYVIEW1"), EXTRUE);
    
    assert_equal(v.tint3, -1);
    
    assert_equal(Bvnull((char *)&v, "tint4", 0, "MYVIEW1"), EXFALSE);
    assert_equal(Bvnull((char *)&v, "tint4", 1, "MYVIEW1"), EXFALSE);
    
    assert_equal(v.tint4[0], 0);
    assert_equal(v.tint4[1], 0);
    
    assert_equal(Bvselinit((char *)&v,"tint4", "MYVIEW1"), EXSUCCEED);
    
    assert_equal(Bvnull((char *)&v, "tint4", 0, "MYVIEW1"), EXTRUE);
    assert_equal(Bvnull((char *)&v, "tint4", 1, "MYVIEW1"), EXTRUE);
    
    /***************************** CHAR TESTS ********************************/
    
    UBF_LOG(log_debug, "tchar1=%x", v.tchar1);
    assert_equal(Bvnull((char *)&v, "tchar1", 0, "MYVIEW1"), EXFALSE);
    
    v.tchar1 = '\n';
    assert_equal(Bvnull((char *)&v, "tchar1", 0, "MYVIEW1"), EXTRUE);
    v.tchar1 = 0;
    assert_equal(Bvnull((char *)&v, "tchar1", 0, "MYVIEW1"), EXFALSE);
    assert_equal(Bvselinit((char *)&v,"tchar1", "MYVIEW1"), EXSUCCEED);
    
    UBF_LOG(log_debug, "tchar1=%x", v.tchar1);
    assert_equal(Bvnull((char *)&v, "tchar1", 0, "MYVIEW1"), EXTRUE);
    
    
    /* Here default is 'A' */
    for (i=0;i<5;i++)
    {
        assert_equal(Bvnull((char *)&v, "tchar2", i, "MYVIEW1"), EXFALSE);
    }
    
    assert_equal(Bvselinit((char *)&v,"tchar2", "MYVIEW1"), EXSUCCEED);
    
    for (i=0;i<5;i++)
    {
        assert_equal(Bvnull((char *)&v, "tchar2", i, "MYVIEW1"), EXTRUE);
    }
    
    for (i=0;i<5;i++)
    {
        assert_equal(v.tchar2[i], 'A');
    }
    
    /* Here default is zero byte */
    
    for (i=0;i<2;i++)
    {
        assert_equal(v.tchar3[i], 0);
    }
    
    /***************************** FLOAT TESTS *******************************/
    for (i=0;i<4;i++)
    {
        assert_equal(Bvnull((char *)&v, "tfloat1", 0, "MYVIEW1"), EXFALSE);
    }
    
    assert_equal(Bvselinit((char *)&v,"tfloat1", "MYVIEW1"), EXSUCCEED);
    
    for (i=0;i<4;i++)
    {
        assert_equal(Bvnull((char *)&v, "tfloat1", i, "MYVIEW1"), EXTRUE);
    }
    
    for (i=0;i<4;i++)
    {
        assert_equal(v.tfloat1[i], 1.1);
    }
    
    /* occ: 0..3*/
    assert_equal(Bvnull((char *)&v, "tfloat1", 4, "MYVIEW1"), EXFAIL);
    assert_equal(Berror, BEINVAL);
    
    /* Default set by memset... */
    for (i=0;i<2;i++)
    {
        assert_equal(Bvnull((char *)&v, "tfloat2", i, "MYVIEW1"), EXTRUE);
        assert_equal(Berror, 0);
    }
    
    assert_equal(Bvnull((char *)&v, "tfloat3", 0, "MYVIEW1"), EXFALSE);
    assert_equal(v.tfloat3, 0);
    
    assert_equal(Bvselinit((char *)&v,"tfloat3", "MYVIEW1"), EXSUCCEED);
    assert_equal(v.tfloat3, 9999.99);
    assert_equal(Bvnull((char *)&v, "tfloat3", 0, "MYVIEW1"), EXTRUE);
    
    
    /***************************** DOUBLE TESTS *******************************/
    for (i=0;i<2;i++)
    {
        assert_equal(Bvnull((char *)&v, "tdouble1", i, "MYVIEW1"), EXFALSE);
    }
    
    assert_equal(Bvselinit((char *)&v,"tdouble1", "MYVIEW1"), EXSUCCEED);
    
    for (i=0;i<2;i++)
    {
        assert_equal(Bvnull((char *)&v, "tdouble1", i, "MYVIEW1"), EXTRUE);
        assert_equal(v.tdouble1[i], 55555.99);
    }
    
    assert_equal(Bvnull((char *)&v, "tdouble2", 0, "MYVIEW1"), EXFALSE);
    assert_equal(Bvselinit((char *)&v,"tdouble2", "MYVIEW1"), EXSUCCEED);
    assert_equal(Bvnull((char *)&v, "tdouble2", 0, "MYVIEW1"), EXTRUE);
    assert_equal(v.tdouble2, -999.123);
    
    /***************************** STRING TESTS *******************************/
    /* Fill the string 1 in advance.. */
    assert_equal(Bvselinit((char *)&v,"tstring1", "MYVIEW1"), EXSUCCEED);
    
    /* Test filler: */
    for (i=0;i<3;i++)
    {
        UBF_LOG(log_debug, "tstring1=[%s]", v.tstring1[i]);
        
        assert_equal(Bvnull((char *)&v, "tstring1", i, "MYVIEW1"), EXTRUE);
    }
    
    /* Test special symbols... */
    for (i=0;i<3;i++)
    {
        assert_equal(Bvnull((char *)&v, "tstring0", i, "MYVIEW1"), EXFALSE);
    }
    
    assert_equal(Bvselinit((char *)&v,"tstring0", "MYVIEW1"), EXSUCCEED);
    
    UBF_DUMP(log_debug, "Special symbols test...", spec_symbols, strlen(spec_symbols));
    
    for (i=0;i<3;i++)
    {
        UBF_LOG(log_debug, "tstring0=[%s]", v.tstring0[i]);
        
        UBF_DUMP(log_debug, "testing0", v.tstring0[i], strlen(v.tstring0[i]));
        
        UBF_DUMP_DIFF(log_debug, "diff", spec_symbols, v.tstring0[i], strlen(spec_symbols));
        
        assert_equal(Bvnull((char *)&v, "tstring0", i, "MYVIEW1"), EXTRUE);
    }
    
    assert_string_equal(v.tstring0[0], spec_symbols);
    
    /* Continue with filler... */
    
    for (i=0;i<3;i++)
    {
        assert_string_equal(v.tstring1[i], "HELLO WORLDBBBBBBBB");
    }
    
    for (i=0;i<3;i++)
    {
        assert_equal(Bvnull((char *)&v, "tstring2", i, "MYVIEW1"), EXTRUE);
    }
    
    for (i=0;i<4;i++)
    {
        assert_equal(Bvnull((char *)&v, "tstring3", i, "MYVIEW1"), EXFALSE);
    }
    
    assert_equal(Bvselinit((char *)&v,"tstring3", "MYVIEW1"), EXSUCCEED);
    
    for (i=0;i<4;i++)
    {
        assert_equal(Bvnull((char *)&v, "tstring3", i, "MYVIEW1"), EXTRUE);
    }
    
    for (i=0;i<4;i++)
    {
        assert_string_equal(v.tstring3[i], "TESTEST");
    }
    
    assert_equal(Bvnull((char *)&v, "tstring4", 0, "MYVIEW1"), EXFALSE);
    assert_equal(Bvselinit((char *)&v,"tstring4", "MYVIEW1"), EXSUCCEED);
    assert_equal(Bvnull((char *)&v, "tstring4", 0, "MYVIEW1"), EXTRUE);
    assert_string_equal(v.tstring4, "HELLO TESTTTTT");
    
    
    /***************************** CARRAY TESTS *******************************/
    
    assert_equal(Bvnull((char *)&v, "tcarray1", 0, "MYVIEW1"), EXFALSE);
    assert_equal(Bvselinit((char *)&v,"tcarray1", "MYVIEW1"), EXSUCCEED);
    assert_equal(Bvnull((char *)&v, "tcarray1", 0, "MYVIEW1"), EXTRUE);
    assert_equal(memcmp(v.tcarray1, "\0\n\t\f\\\'\"\vHELLOWORLD", 18), 0);
    
    assert_equal(Bvnull((char *)&v, "tcarray2", 0, "MYVIEW1"), EXFALSE);
    assert_equal(Bvselinit((char *)&v,"tcarray2", "MYVIEW1"), EXSUCCEED);
    assert_equal(Bvnull((char *)&v, "tcarray2", 0, "MYVIEW1"), EXTRUE);
    assert_equal(memcmp(v.tcarray2, "\0\n\t\f\\\'\"\vHELLOWORL\n\n\n\n\n\n\n\n'", 25), 0);
    
    for (i=0; i<10; i++)
    {
        assert_equal(v.tcarray3[i][0], 0);
    }
    
    assert_equal(Bvselinit((char *)&v, "tcarray3", "MYVIEW1"), EXSUCCEED);
    
    for (i=0; i<10; i++)
    {
        assert_equal(memcmp(v.tcarray3[i], "\0\\\nABC\t\f\'\vHELLOOOOOOOOOOOOOOOOOOO", 30), 0);
    }
    
    assert_equal(Bvselinit((char *)&v, "tcarray4", "MYVIEW1"), EXSUCCEED);
    assert_equal(Bvnull((char *)&v, "tcarray4", 0, "MYVIEW1"), EXTRUE);
    assert_equal(memcmp(v.tcarray4, "ABC", 3), 0);
    
    
    assert_equal(Bvselinit((char *)&v, "tcarray5", "MYVIEW1"), EXSUCCEED);
    assert_equal(Bvnull((char *)&v, "tcarray5", 0, "MYVIEW1"), EXTRUE);
    
    assert_equal(memcmp(v.tcarray5, "\0\0\0\0\0", 5), 0);
    
}

Ensure(test_Bvsinit)
{
    struct MYVIEW2 v;
   
    memset(&v, 0, sizeof(v));
    assert_equal(Bvsinit((char *)&v, "MYVIEW2"), EXSUCCEED);
    
    assert_equal(Bvnull((char *)&v, "tshort1", 0, "MYVIEW2"), EXTRUE);
    assert_equal(Bvnull((char *)&v, "tlong1", 0, "MYVIEW2"), EXTRUE);
    assert_equal(Bvnull((char *)&v, "tchar1", 0, "MYVIEW2"), EXTRUE);
    assert_equal(Bvnull((char *)&v, "tfloat1", 0, "MYVIEW2"), EXTRUE);
    assert_equal(Bvnull((char *)&v, "tdouble1", 0, "MYVIEW2"), EXTRUE);
    assert_equal(Bvnull((char *)&v, "tstring1", 0, "MYVIEW2"), EXTRUE);
    assert_equal(Bvnull((char *)&v, "tcarray1", 0, "MYVIEW2"), EXTRUE); 
}

Ensure(test_Bvrefresh)
{
    Bvrefresh();
}

/**
 * Install structure to UBF
 */
Ensure(test_Bvstof)
{
    struct MYVIEW1 v;
    char buf[2048];
    BFLDID bfldid;
    BFLDOCC occ;
    int flds_got;
    UBFH *p_ub = (UBFH *)buf;
    
    assert_equal(Binit(p_ub, sizeof(buf)), EXSUCCEED);
    
    /* Set to NULL */
    assert_equal(Bvsinit((char *)&v, "MYVIEW1"), EXSUCCEED);
    
    /* transfer to UBF.. */
    assert_equal(Bvstof(p_ub, (char *)&v, BUPDATE, "MYVIEW1"), EXSUCCEED);
    
    /* not fields must be transfered. */
    bfldid = BFIRSTFLDID;
    flds_got = 0;
    while(1==Bnext(p_ub, &bfldid, &occ, NULL, NULL))
    {
        flds_got++;
    }

    assert_equal(flds_got, 0);
    
    
}

/* TODO: Unit test for default values... */

/**
 * Very basic tests of the framework
 * @return
 */
TestSuite *view_tests() {
    TestSuite *suite = create_test_suite();
    
    set_setup(suite, basic_setup);
    set_teardown(suite, basic_teardown);

    /* init view test */
    add_test(suite, test_Bvnull);
    add_test(suite, test_Bvsinit);
    add_test(suite, test_Bvrefresh);
    add_test(suite, test_Bvstof);
    
    return suite;
}

/*
 * Main test entry.
 */
int main(int argc, char** argv)
{    
    TestSuite *suite = create_test_suite();

    add_suite(suite, view_tests());
    

    if (argc > 1) {
        return run_single_test(suite,argv[1],create_text_reporter());
    }

    return run_test_suite(suite, create_text_reporter());
    
}
