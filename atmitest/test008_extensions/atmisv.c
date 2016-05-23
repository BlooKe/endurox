/* 
**
** @file atmisv.c
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

/* Stuff for sockets testing: */
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

#include <ndrstandard.h>

#ifdef EX_OS_LINUX

#include <sys/epoll.h>

#else

#include <poll.h>

#endif

#include <stdio.h>
#include <stdlib.h>
#include <ndebug.h>
#include <atmi.h>
#include <ubf.h>
#include <test.fd.h>


#ifdef EX_OS_LINUX

#define POLL_FLAGS (EPOLLET | EPOLLIN | EPOLLHUP)

#else

#define POLL_FLAGS (POLLIN)

#endif

void TESTSVFN (TPSVCINFO *p_svc);

char *test_ptr = "THIS IS TEST";

/*
 * This is test service!
 */
void TESTSVFN (TPSVCINFO *p_svc)
{
    int ret=SUCCEED;

    char *str = "THIS IS TEST - OK!";

    UBFH *p_ub = (UBFH *)p_svc->data;

    NDRX_LOG(log_debug, "TESTSVFN got call");

    /* Just print the buffer */
    Bprint(p_ub);
    if (NULL==(p_ub = (UBFH *)tprealloc((char *)p_ub, 4096))) /* allocate some stuff for more data to put in  */
    {
        ret=FAIL;
        goto out;
    }

    if (FAIL==Bchg(p_ub, T_STRING_FLD, 0, (char *)str, 0))
    {
        ret=FAIL;
        goto out;
    }

out:
    tpreturn(  ret==SUCCEED?TPSUCCESS:TPFAIL,
                0L,
                (char *)p_ub,
                0L,
                0L);
}

/**
 * Socket extension
 * @param fd
 * @param events
 * @return 
 */
int poll_connect_22(int fd, uint32_t events, void *ptr1)
{
    int ret=SUCCEED;    
    int so_error=0;
    int sz;
    char buf[2048];
    socklen_t len = sizeof so_error;

    getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_error, &len);
    
    NDRX_LOG(log_warn, "POLL1EXT_OK, events: %d", events);
    
    if (so_error == 0) 
    { 
        NDRX_LOG(log_debug,"%s:%s is open\n", "127.0.0.1", "22");
        /* Read some stuff!! */
        if ((sz=recv(fd, buf, 2048, 0))>0)
        {
            NDRX_DUMP(log_error, "Got stuff from socket: ", buf, sz);
        }
        else if (sz==0)
        {
            NDRX_LOG(log_debug,"peer disconnected");
        }
        else
        {
            NDRX_LOG(log_debug,"error: %s", strerror(errno));
        }
    }
    else
    {
        NDRX_LOG(log_debug,"so_error=%d - %s\n", so_error, strerror(so_error));
    }
    
    if (NULL==ptr1 || 0!=strcmp((char *)ptr1, "THIS IS TEST"))
    {
        NDRX_LOG(log_debug,"TESTERROR: ptr1 does not work in poll ext!");
    }
    
    
out:
    return ret;
}

/**
 * Before poll function
 * @return 
 */
int b4poll_cb(void)
{
    NDRX_LOG(log_warn, "B4POLL_CALLED");
    return SUCCEED;
}
/**
 * Periodical callback function
 * @param argc
 * @param argv
 * @return 
 */
int periodical_cb(void)
{
    static int first=TRUE;
    NDRX_LOG(log_debug, "PERIODCB_OK");
    
    if (first)
    {
        /* Example from: http://stackoverflow.com/questions/2597608/c-socket-connection-timeout */
        u_short port;                /* user specified port number */
        char *addr;                  /* will be a pointer to the address */
        struct sockaddr_in address;  /* the libc network address data structure */
        short int sock = -1;         /* file descriptor for the network socket */
        fd_set fdset;
        struct timeval tv;

        NDRX_LOG(log_debug, "Try to connect somewhere, and "
                        "check will we get Event!");

        port = 22;
        addr = "127.0.0.1";

        address.sin_family = AF_INET;
        address.sin_addr.s_addr = inet_addr(addr); /* assign the address */
        address.sin_port = htons(port);            /* translate int2port num */

        sock = socket(AF_INET, SOCK_STREAM, 0);
        fcntl(sock, F_SETFL, O_NONBLOCK);

        connect(sock, (struct sockaddr *)&address, sizeof(address));
        
        if (SUCCEED!=tpext_addpollerfd(sock, 
                POLL_FLAGS,
                (void *)test_ptr, poll_connect_22))
        {
            NDRX_LOG(log_error, "TESTERROR: tpext_addpollerfd failed!");
        }
        
        first=FALSE;
        
        /*
         * Lets have multiple connections!!
        first=FALSE;
         * 
         * 
         *   // Set to blocking mode again... 
  arg = fcntl(soc, F_GETFL, NULL); 
  arg &= (~O_NONBLOCK); 
  fcntl(soc, F_SETFL, arg); 
         * 
         */
    }
    
    
    return SUCCEED;
}

/*
 * Do initialization
 */
int tpsvrinit(int argc, char **argv)
{
    NDRX_LOG(log_debug, "tpsvrinit called");


    /* Step 1: add periodical call */
    if (SUCCEED!=tpext_addperiodcb(1, periodical_cb))
    {
        NDRX_LOG(log_error, "TESTERROR: tpext_addperiodcb failed!");
    }
    
    if (SUCCEED!=tpext_addb4pollcb(b4poll_cb))
    {
        NDRX_LOG(log_error, "TESTERROR: tpext_addb4pollcb failed!");
    }
    
    return SUCCEED;
}

/**
 * Do de-initialization
 */
void tpsvrdone(void)
{
    NDRX_LOG(log_debug, "tpsvrdone called");
}
