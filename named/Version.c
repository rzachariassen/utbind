/*
 * Copyright (c) 1986, 1987 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	@(#)Version.c	4.8.2.1 (Berkeley) 9/18/89
 */

#ifndef lint
char sccsid[] = "@(#)named 4.8.2 (Beta) %VERSION% %WHOANDWHERE%\n";
#endif /* not lint */

char Version[] = "named 4.8.2 (Beta) %VERSION%\n\t%WHOANDWHERE%\n";

#ifdef COMMENT

SCCS/s.Version.c:

D 4.8.2   89/09/18 13:56:57	bloom  35      34      00020/00014/00087
Interim fixes release

D 4.8.1   89/02/08 17:12:15	karels  34      33      00026/00017/00075
Berkeley Networking Release

D 4.8	88/07/09 14:27:00       karels  33      28      00043/00031/00049
4.8 is here!

D 4.7	87/11/20 13:15:52	karels	25	24	00000/00000/00062
4.7.3 beta

D 4.6	87/07/21 12:15:52	karels	25	24	00000/00000/00062
4.6 declared stillborn

D 4.5	87/02/10 12:33:25	kjd	24	18	00000/00000/00062
February 1987, Network Release. Child (bind) grows up, parent (kevin) leaves home.

D 4.4	86/10/01 10:06:26	kjd	18	12	00020/00017/00042
October 1, 1986 Network Distribution

D 4.3	86/06/04 12:12:18	kjd	12	7	00015/00028/00044
Version distributed with 4.3BSD

D 4.2	86/04/30 20:57:16	kjd	7	1	00056/00000/00016
Network distribution Freeze and one more version until 4.3BSD

D 1.1	86/04/30 19:30:00	kjd	1	0	00016/00000/00000
date and time created 86/04/30 19:30:00 by kjd

code versions:

Makefile
	Makefile	4.19 (Berkeley) 2/8/89
db.h
	db.h	4.14 (Berkeley) 6/18/88
ns.h
	ns.h	4.26 (Berkeley) 8/9/89
pathnames.h
	pathnames.h	5.3 (Berkeley) 8/9/89
db_dump.c
	db_dump.c	4.26 (Berkeley) 6/29/89
db_glue.c
	db_glue.c	4.2 (Berkeley) 1/14/89
db_load.c
	db_load.c	4.33 (Berkeley) 8/9/89
db_lookup.c
	db_lookup.c	4.16 (Berkeley) 1/14/89
db_reload.c
	db_reload.c	4.19 (Berkeley) 1/14/89
db_save.c
	db_save.c	4.14 (Berkeley) 6/18/88
db_update.c
	db_update.c	4.22 (Berkeley) 8/9/89
ns_forw.c
	ns_forw.c	4.27 (Berkeley) 6/18/88
ns_init.c
	ns_init.c	4.30 (Berkeley) 8/9/89
ns_main.c
	 Copyright (c) 1986, 1989 Regents of the University of California.\n\
	ns_main.c	4.41 (Berkeley) 8/11/89
ns_maint.c
	ns_maint.c	4.34 (Berkeley) 9/18/89
ns_req.c
	ns_req.c	4.38 (Berkeley) 8/9/89
ns_resp.c
	ns_resp.c	4.58 (Berkeley) 9/15/89
ns_sort.c
	ns_sort.c	4.5 (Berkeley) 1/14/89
ns_stats.c
	ns_stats.c	4.6 (Berkeley) 8/9/89
xfer.c
	 Copyright (c) 1988 Regents of the University of California.\n\
	xfer.c	4.7 (Berkeley) 8/23/89
newvers.sh
	newvers.sh	4.6 (Berkeley) 5/11/89

#endif COMMENT
