/* 
** Enduro/X internal UBF field table
**
** @file Exfields.h
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
#ifndef __EXFIELDS_H
#define __EXFIELDS_H
/*	fname	bfldid            */
/*	-----	-----            */
#define	SRVCNM	((BFLDID32)167772261)	/* number: 101	 type: string */
#define	EV_FLAGS	((BFLDID32)33554536)	/* number: 104	 type: long */
#define	EV_MASK	((BFLDID32)167772265)	/* number: 105	 type: string */
#define	EV_FILTER	((BFLDID32)167772266)	/* number: 106	 type: string */
#define	EV_SRVCNM	((BFLDID32)167772267)	/* number: 107	 type: string */
#define	EV_SUBSNR	((BFLDID32)33554540)	/* number: 108	 type: long */
#define	EXDM_RESTARTS	((BFLDID32)33554547)	/* number: 115	 type: long */
#define	TMCMD	((BFLDID32)67108984)	/* number: 120	 type: char */
#define	TMXID	((BFLDID32)167772281)	/* number: 121	 type: string */
#define	TMRMID	((BFLDID32)123)	/* number: 123	 type: short */
#define	TMNODEID	((BFLDID32)122)	/* number: 122	 type: short */
#define	TMSRVID	((BFLDID32)124)	/* number: 124	 type: short */
#define	TMKNOWNRMS	((BFLDID32)167772285)	/* number: 125	 type: string */
#define	TMERR_CODE	((BFLDID32)130)	/* number: 130	 type: short */
#define	TMERR_REASON	((BFLDID32)131)	/* number: 131	 type: short */
#define	TMERR_MSG	((BFLDID32)167772292)	/* number: 132	 type: string */
#define	TMPROCESSID	((BFLDID32)167772310)	/* number: 150	 type: string */
#define	TMCALLERRM	((BFLDID32)151)	/* number: 151	 type: short */
#define	TMTXTOUT	((BFLDID32)33554584)	/* number: 152	 type: long */
#define	TMTXTOUT_LEFT	((BFLDID32)33554585)	/* number: 153	 type: long */
#define	TMTXSTAGE	((BFLDID32)154)	/* number: 154	 type: short */
#define	TMTXRMID	((BFLDID32)155)	/* number: 155	 type: short */
#define	TMTXRMSTATUS	((BFLDID32)67109020)	/* number: 156	 type: char */
#define	TMTXRMERRCODE	((BFLDID32)33554589)	/* number: 157	 type: long */
#define	TMTXRMREASON	((BFLDID32)158)	/* number: 158	 type: short */
#define	TMTXTRYCNT	((BFLDID32)33554591)	/* number: 159	 type: long */
#define	TMTXTRYMAXCNT	((BFLDID32)33554592)	/* number: 160	 type: long */
#define	TMTXFLAGS	((BFLDID32)33554593)	/* number: 161	 type: long */
#define	EX_CPMCOMMAND	((BFLDID32)167772360)	/* number: 200	 type: string */
#define	EX_CPMOUTPUT	((BFLDID32)167772361)	/* number: 201	 type: string */
#define	EX_CPMTAG	((BFLDID32)167772362)	/* number: 202	 type: string */
#define	EX_CPMSUBSECT	((BFLDID32)167772363)	/* number: 203	 type: string */
#define	EX_CLTID	((BFLDID32)167772410)	/* number: 250	 type: string */
#define	EX_DATA	((BFLDID32)201326843)	/* number: 251	 type: carray */
#define	EX_DATA_BUFTYP	((BFLDID32)252)	/* number: 252	 type: short */
#define	EX_TSTAMP1_STR	((BFLDID32)167772413)	/* number: 253	 type: string */
#define	EX_TSTAMP2_STR	((BFLDID32)167772414)	/* number: 254	 type: string */
#define	EX_QSPACE	((BFLDID32)167772460)	/* number: 300	 type: string */
#define	EX_QNAME	((BFLDID32)167772461)	/* number: 301	 type: string */
#define	EX_QFLAGS	((BFLDID32)33554734)	/* number: 302	 type: long */
#define	EX_QDEQ_TIME	((BFLDID32)33554735)	/* number: 303	 type: long */
#define	EX_QPRIORITY	((BFLDID32)33554736)	/* number: 304	 type: long */
#define	EX_QDIAGNOSTIC	((BFLDID32)33554737)	/* number: 305	 type: long */
#define	EX_QMSGID	((BFLDID32)201326898)	/* number: 306	 type: carray */
#define	EX_QCORRID	((BFLDID32)201326899)	/* number: 307	 type: carray */
#define	EX_QREPLYQUEUE	((BFLDID32)167772468)	/* number: 308	 type: string */
#define	EX_QFAILUREQUEUE	((BFLDID32)167772469)	/* number: 309	 type: string */
#define	EX_QURCODE	((BFLDID32)33554742)	/* number: 310	 type: long */
#define	EX_QAPPKEY	((BFLDID32)33554743)	/* number: 311	 type: long */
#define	EX_QDELIVERY_QOS	((BFLDID32)33554744)	/* number: 312	 type: long */
#define	EX_QREPLY_QOS	((BFLDID32)33554745)	/* number: 313	 type: long */
#define	EX_QEXP_TIME	((BFLDID32)33554746)	/* number: 314	 type: long */
#define	EX_QCMD	((BFLDID32)67109179)	/* number: 315	 type: char */
#define	EX_QDIAGMSG	((BFLDID32)167772476)	/* number: 316	 type: string */
#define	EX_QNUMMSG	((BFLDID32)33554749)	/* number: 317	 type: long */
#define	EX_QNUMLOCKED	((BFLDID32)33554750)	/* number: 318	 type: long */
#define	EX_QNUMSUCCEED	((BFLDID32)33554751)	/* number: 319	 type: long */
#define	EX_QNUMFAIL	((BFLDID32)33554752)	/* number: 320	 type: long */
#define	EX_QSTRFLAGS	((BFLDID32)167772481)	/* number: 321	 type: string */
#define	EX_QMSGTRIES	((BFLDID32)33554754)	/* number: 322	 type: long */
#define	EX_QMSGLOCKED	((BFLDID32)323)	/* number: 323	 type: short */
#define	EX_QMSGIDSTR	((BFLDID32)167772484)	/* number: 324	 type: string */
#define	EX_QNUMENQ	((BFLDID32)33554757)	/* number: 325	 type: long */
#define	EX_QNUMDEQ	((BFLDID32)33554758)	/* number: 326	 type: long */
#endif
