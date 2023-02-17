#
# Copyright (c) 1987 Regents of the University of California.
# All rights reserved.
#
# Redistribution and use in source and binary forms are permitted
# provided that the above copyright notice and this paragraph are
# duplicated in all such forms and that any documentation,
# advertising materials, and other materials related to such
# distribution and use acknowledge that the software was developed
# by the University of California, Berkeley.  The name of the
# University may not be used to endorse or promote products derived
# from this software without specific prior written permission.
# THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
# WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
#
#	@(#)Makefile	4.9 (Berkeley) 10/10/88
#
CFLAGS=	-g -I../include
RES=	../res/libresolv.a
LIBC=	/lib/libc.a
SRCS=	nsquery.c nstest.c nsmaint.c host.c
OBJS=

all: nsquery nstest nsmaint nslookup host

nsquery: ${LIBC}
	${CC} ${CFLAGS} -o $@ nsquery.c ${RES}

# test programs and utilities
nstest: ${LIBC}
	@echo "The resolver routines in libc must be compiled with the -DDEBUG"
	@echo "option or nstest will not have any output."
	${CC} ${CFLAGS} -o $@ nstest.c ${RES}

nsmaint: ${LIBC}
	${CC} ${CFLAGS} -o $@ nsmaint.c ${RES}

nslookup: FRC
	cd nslookup; make ${MFLAGS} RES=${RES}

host: FRC
	${CC} ${CFLAGS} -o $@ host.c ${RES}

clean: FRC
	cd nslookup; make ${MFLAGS} clean
	rm -f ${OBJS} core nsquery nstest nsmaint host

cleandir: clean
	cd nslookup; make ${MFLAGS} cleandir
	rm -f ${MAN} tags .depend

depend: ${SRCS}
	cd nslookup; make ${MFLAGS} depend
	mkdep -p ${CFLAGS} ${SRCS}

install: ${MAN}
	cd nslookup; make ${MFLAGS} DESTDIR=${DESTDIR} install

lint: ${SRCS}
	cd nslookup; make ${MFLAGS} lint
	lint ${CFLAGS} ${SRCS}

tags: ${SRCS}
	cd nslookup; make ${MFLAGS} tags
	ctags ${SRCS}

FRC:
# DO NOT DELETE THIS LINE -- mkdep uses it.
# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.

nsquery: nsquery.c /usr/include/sys/param.h /usr/include/machine/param.h
nsquery: /usr/include/sys/signal.h /usr/include/vm/faultcode.h
nsquery: /usr/include/sys/types.h /usr/include/sys/sysmacros.h
nsquery: ../include/arpa/nameser.h ../include/netdb.h /usr/include/sys/socket.h
nsquery: /usr/include/netinet/in.h ../include/resolv.h /usr/include/stdio.h
nstest: nstest.c /usr/include/sys/types.h /usr/include/sys/sysmacros.h
nstest: /usr/include/sys/socket.h /usr/include/netinet/in.h
nstest: /usr/include/stdio.h ../include/arpa/nameser.h ../include/resolv.h
nsmaint: nsmaint.c /usr/include/stdio.h /usr/include/ctype.h
nsmaint: /usr/include/sys/types.h /usr/include/sys/sysmacros.h
nsmaint: /usr/include/sys/stat.h /usr/include/sys/param.h
nsmaint: /usr/include/machine/param.h /usr/include/sys/signal.h
nsmaint: /usr/include/vm/faultcode.h /usr/include/sys/types.h
nsmaint: /usr/include/sys/dir.h /usr/include/pwd.h /usr/include/grp.h
nsmaint: /usr/include/sys/signal.h /usr/include/syslog.h /usr/include/errno.h
nsmaint: /usr/include/sys/errno.h ../include/pathnames.h

# IF YOU PUT ANYTHING HERE IT WILL GO AWAY
