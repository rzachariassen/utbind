#
# Copyright (c) 1987, 1988 Regents of the University of California.
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
#	@(#)Makefile	4.19 (Berkeley) 2/8/89
#
DEFINES= -DDEBUG -DSTATS -DSUNOS4 -DNET_ERRS
PATHS=	-DBOOTFILE=\"/etc/nsdata/named.boot\" -DPIDFILE=\"/etc/named.pid\"
### -DALLOW_T_UNSPEC -Dmalloc=rt_malloc -Dfree=rt_free
### ALLOC=storage.o
COPTS=	-g
CFLAGS=	${COPTS} -I../include ${DEFINES}

USR_SBIN=/local/etc
USR_LIBEXEC=/local/etc
USR_MAN=/local/man

RES=	../res/libresolv.a
LIBS=	${RES}
LIBC=	/lib/libc.a

SRCS=	db_dump.c db_load.c db_lookup.c db_reload.c db_save.c db_update.c \
	ns_forw.c ns_init.c ns_main.c ns_maint.c ns_req.c ns_resp.c \
	ns_sort.c ns_stats.c ns_path.c db_glue.c xfer.c
HDRS=	db.h ns.h
OBJS=	db_dump.o db_load.o db_lookup.o db_reload.o db_save.o db_update.o \
	ns_forw.o ns_init.o ns_main.o ns_maint.o ns_req.o ns_resp.o \
	ns_sort.o ns_stats.o ns_path.o db_glue.o $(ALLOC)
XFEROBJ=   xfer.o db_glue.o

SUBDIR=	tools
#MAN=	named.0

all: named xfer
#	for i in ${SUBDIR}; do \
#		(cd $$i; make ${MFLAGS} $@); done

named: ${OBJS} ${LIBC}
	sh newvers.sh
	${CC} ${COPTS} -o $@ version.c ${OBJS} ${LIBS}

ns_main.o:	ns_main.c
	${CC} ${CFLAGS} ${PATHS} -c $<

xfer:	${XFEROBJ} ${LIBC}
	${CC} ${COPTS} -o $@ ${XFEROBJ} ${LIBS}

clean:
	rm -f ${OBJS} ${XFEROBJ} core named xfer version.o version.c
#	for i in ${SUBDIR}; do \
#		(cd $$i; make ${MFLAGS} $@); done

cleandir: clean
	rm -f ${MAN} tags .depend
#	for i in ${SUBDIR}; do \
#		(cd $$i; make ${MFLAGS} $@); done

depend .depend: ${SRCS}
	mkdep ${CFLAGS} ${SRCS}
#	for i in ${SUBDIR}; do \
#		(cd $$i; make ${MFLAGS} $@); done

install: ${MAN}
	install -s -o bin -g bin -m 755 named ${DESTDIR}${USR_SBIN}
	install -s -o bin -g bin -m 755 xfer ${DESTDIR}${USR_LIBEXEC}/named-xfer
#	install -c -o bin -g bin -m 444 ${MAN} ${DESTDIR}${USR_MAN}/cat8
	install -c -o bin -g bin -m 755 named.restart ${DESTDIR}${USR_SBIN}
	install -c -o bin -g bin -m 755 named.reload ${DESTDIR}${USR_SBIN}
#	for i in ${SUBDIR}; do \
#		(cd $$i; make ${MFLAGS} $@); done

lint: ${SRCS} ${HDRS}
	lint -Dlint ${CFLAGS} ${SRCS}
#	for i in ${SUBDIR}; do \
#		(cd $$i; make ${MFLAGS} $@); done

tags: ${SRCS}
	ctags -t ${SRCS} ${HDRS}
#	for i in ${SUBDIR}; do \
#		(cd $$i; make ${MFLAGS} $@); done
# DO NOT DELETE THIS LINE -- mkdep uses it.
# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.

db_dump.o: db_dump.c /usr/include/sys/param.h /usr/include/machine/param.h
db_dump.o: /usr/include/sys/signal.h /usr/include/vm/faultcode.h
db_dump.o: /usr/include/sys/types.h /usr/include/sys/sysmacros.h
db_dump.o: /usr/include/sys/time.h /usr/include/time.h /usr/include/sys/stat.h
db_dump.o: /usr/include/netinet/in.h ../include/netdb.h /usr/include/stdio.h
db_dump.o: /usr/include/syslog.h ../include/arpa/nameser.h ns.h
db_dump.o: /usr/include/strings.h /usr/include/arpa/inet.h db.h
db_dump.o: ../include/pathnames.h ../include/paths.h
db_load.o: db_load.c /usr/include/sys/param.h /usr/include/machine/param.h
db_load.o: /usr/include/sys/signal.h /usr/include/vm/faultcode.h
db_load.o: /usr/include/sys/types.h /usr/include/sys/sysmacros.h
db_load.o: /usr/include/sys/time.h /usr/include/time.h /usr/include/sys/stat.h
db_load.o: /usr/include/netinet/in.h /usr/include/stdio.h /usr/include/syslog.h
db_load.o: /usr/include/ctype.h ../include/netdb.h ../include/arpa/nameser.h
db_load.o: ns.h /usr/include/strings.h /usr/include/arpa/inet.h db.h
db_lookup.o: db_lookup.c /usr/include/sys/types.h /usr/include/sys/sysmacros.h
db_lookup.o: /usr/include/stdio.h ../include/arpa/nameser.h db.h
db_reload.o: db_reload.c /usr/include/sys/types.h /usr/include/sys/sysmacros.h
db_reload.o: /usr/include/sys/time.h /usr/include/time.h
db_reload.o: /usr/include/netinet/in.h /usr/include/stdio.h
db_reload.o: /usr/include/syslog.h ../include/arpa/nameser.h ns.h
db_reload.o: /usr/include/strings.h /usr/include/arpa/inet.h db.h
db_save.o: db_save.c /usr/include/sys/types.h /usr/include/sys/sysmacros.h
db_save.o: /usr/include/stdio.h /usr/include/syslog.h ../include/arpa/nameser.h
db_save.o: db.h
db_update.o: db_update.c /usr/include/sys/types.h /usr/include/sys/sysmacros.h
db_update.o: /usr/include/sys/socket.h /usr/include/sys/time.h
db_update.o: /usr/include/time.h /usr/include/netinet/in.h /usr/include/stdio.h
db_update.o: /usr/include/syslog.h ../include/arpa/nameser.h ns.h
db_update.o: /usr/include/strings.h /usr/include/arpa/inet.h db.h
ns_forw.o: ns_forw.c /usr/include/stdio.h /usr/include/sys/param.h
ns_forw.o: /usr/include/machine/param.h /usr/include/sys/signal.h
ns_forw.o: /usr/include/vm/faultcode.h /usr/include/sys/types.h
ns_forw.o: /usr/include/sys/sysmacros.h /usr/include/sys/time.h
ns_forw.o: /usr/include/time.h /usr/include/sys/socket.h
ns_forw.o: /usr/include/netinet/in.h /usr/include/syslog.h /usr/include/errno.h
ns_forw.o: /usr/include/sys/errno.h ../include/arpa/nameser.h ns.h
ns_forw.o: /usr/include/strings.h /usr/include/arpa/inet.h db.h
ns_init.o: ns_init.c /usr/include/sys/types.h /usr/include/sys/sysmacros.h
ns_init.o: /usr/include/sys/socket.h /usr/include/sys/time.h
ns_init.o: /usr/include/time.h /usr/include/sys/stat.h
ns_init.o: /usr/include/netinet/in.h /usr/include/stdio.h /usr/include/errno.h
ns_init.o: /usr/include/sys/errno.h /usr/include/signal.h
ns_init.o: /usr/include/vm/faultcode.h /usr/include/syslog.h
ns_init.o: /usr/include/ctype.h /usr/include/string.h ../include/arpa/nameser.h
ns_init.o: ns.h /usr/include/strings.h /usr/include/arpa/inet.h db.h
ns_main.o: ns_main.c /usr/include/sys/param.h /usr/include/machine/param.h
ns_main.o: /usr/include/sys/signal.h /usr/include/vm/faultcode.h
ns_main.o: /usr/include/sys/types.h /usr/include/sys/sysmacros.h
ns_main.o: /usr/include/sys/file.h /usr/include/sys/fcntlcom.h
ns_main.o: /usr/include/sys/time.h /usr/include/time.h /usr/include/sys/wait.h
ns_main.o: /usr/include/sys/resource.h /usr/include/sys/ioctl.h
ns_main.o: /usr/include/sys/ttychars.h /usr/include/sys/ttydev.h
ns_main.o: /usr/include/sys/ttold.h /usr/include/sys/ioccom.h
ns_main.o: /usr/include/sys/ttycom.h /usr/include/sys/filio.h
ns_main.o: /usr/include/sys/ioccom.h /usr/include/sys/sockio.h
ns_main.o: /usr/include/sys/ioccom.h /usr/include/sys/socket.h
ns_main.o: /usr/include/netinet/in.h /usr/include/net/if.h /usr/include/stdio.h
ns_main.o: /usr/include/syslog.h /usr/include/errno.h /usr/include/sys/errno.h
ns_main.o: /usr/include/signal.h ../include/netdb.h ../include/arpa/nameser.h
ns_main.o: /usr/include/arpa/inet.h ../include/resolv.h ns.h
ns_main.o: /usr/include/strings.h /usr/include/arpa/inet.h db.h
ns_main.o: ../include/pathnames.h ../include/paths.h
ns_maint.o: ns_maint.c /usr/include/sys/param.h /usr/include/machine/param.h
ns_maint.o: /usr/include/sys/signal.h /usr/include/vm/faultcode.h
ns_maint.o: /usr/include/sys/types.h /usr/include/sys/sysmacros.h
ns_maint.o: /usr/include/sys/socket.h /usr/include/sys/time.h
ns_maint.o: /usr/include/time.h /usr/include/netinet/in.h /usr/include/stdio.h
ns_maint.o: /usr/include/syslog.h /usr/include/signal.h /usr/include/errno.h
ns_maint.o: /usr/include/sys/errno.h ../include/arpa/nameser.h
ns_maint.o: /usr/include/sys/wait.h /usr/include/sys/stat.h
ns_maint.o: /usr/include/vfork.h ns.h /usr/include/strings.h
ns_maint.o: /usr/include/arpa/inet.h db.h ../include/pathnames.h
ns_maint.o: ../include/paths.h
ns_req.o: ns_req.c /usr/include/stdio.h /usr/include/sys/param.h
ns_req.o: /usr/include/machine/param.h /usr/include/sys/signal.h
ns_req.o: /usr/include/vm/faultcode.h /usr/include/sys/types.h
ns_req.o: /usr/include/sys/sysmacros.h /usr/include/sys/uio.h
ns_req.o: /usr/include/sys/time.h /usr/include/time.h /usr/include/sys/socket.h
ns_req.o: /usr/include/netinet/in.h /usr/include/syslog.h
ns_req.o: /usr/include/sys/file.h /usr/include/sys/fcntlcom.h
ns_req.o: ../include/arpa/nameser.h ns.h /usr/include/strings.h
ns_req.o: /usr/include/arpa/inet.h db.h
ns_resp.o: ns_resp.c /usr/include/stdio.h /usr/include/sys/param.h
ns_resp.o: /usr/include/machine/param.h /usr/include/sys/signal.h
ns_resp.o: /usr/include/vm/faultcode.h /usr/include/sys/types.h
ns_resp.o: /usr/include/sys/sysmacros.h /usr/include/sys/time.h
ns_resp.o: /usr/include/time.h /usr/include/sys/socket.h
ns_resp.o: /usr/include/sys/file.h /usr/include/sys/fcntlcom.h
ns_resp.o: /usr/include/netinet/in.h /usr/include/syslog.h
ns_resp.o: ../include/arpa/nameser.h ns.h /usr/include/strings.h
ns_resp.o: /usr/include/arpa/inet.h db.h
ns_sort.o: ns_sort.c /usr/include/stdio.h /usr/include/sys/types.h
ns_sort.o: /usr/include/sys/sysmacros.h /usr/include/sys/time.h
ns_sort.o: /usr/include/time.h /usr/include/sys/socket.h
ns_sort.o: /usr/include/sys/file.h /usr/include/sys/fcntlcom.h
ns_sort.o: /usr/include/netinet/in.h /usr/include/syslog.h
ns_sort.o: ../include/arpa/nameser.h ns.h /usr/include/strings.h
ns_sort.o: /usr/include/arpa/inet.h db.h
ns_stats.o: ns_stats.c /usr/include/sys/param.h /usr/include/machine/param.h
ns_stats.o: /usr/include/sys/signal.h /usr/include/vm/faultcode.h
ns_stats.o: /usr/include/sys/types.h /usr/include/sys/sysmacros.h
ns_stats.o: /usr/include/sys/time.h /usr/include/time.h
ns_stats.o: /usr/include/netinet/in.h /usr/include/stdio.h
ns_stats.o: /usr/include/syslog.h ../include/arpa/nameser.h ns.h
ns_stats.o: /usr/include/strings.h /usr/include/arpa/inet.h
ns_stats.o: ../include/pathnames.h ../include/paths.h
ns_path.o: ns_path.c /usr/include/sys/types.h /usr/include/sys/sysmacros.h
ns_path.o: ../include/arpa/nameser.h /usr/include/stdio.h /usr/include/ctype.h
ns_path.o: /usr/include/syslog.h /usr/include/sys/file.h
ns_path.o: /usr/include/sys/fcntlcom.h /usr/include/ndbm.h
db_glue.o: db_glue.c /usr/include/sys/param.h /usr/include/machine/param.h
db_glue.o: /usr/include/sys/signal.h /usr/include/vm/faultcode.h
db_glue.o: /usr/include/sys/types.h /usr/include/sys/sysmacros.h
db_glue.o: /usr/include/sys/time.h /usr/include/time.h /usr/include/sys/stat.h
db_glue.o: /usr/include/netinet/in.h /usr/include/stdio.h /usr/include/syslog.h
db_glue.o: /usr/include/ctype.h ../include/netdb.h ../include/arpa/nameser.h
db_glue.o: ns.h /usr/include/strings.h /usr/include/arpa/inet.h db.h
xfer.o: xfer.c /usr/include/sys/param.h /usr/include/machine/param.h
xfer.o: /usr/include/sys/signal.h /usr/include/vm/faultcode.h
xfer.o: /usr/include/sys/types.h /usr/include/sys/sysmacros.h
xfer.o: /usr/include/sys/file.h /usr/include/sys/fcntlcom.h
xfer.o: /usr/include/sys/time.h /usr/include/time.h /usr/include/sys/stat.h
xfer.o: /usr/include/sys/wait.h /usr/include/sys/resource.h
xfer.o: /usr/include/sys/ioctl.h /usr/include/sys/ttychars.h
xfer.o: /usr/include/sys/ttydev.h /usr/include/sys/ttold.h
xfer.o: /usr/include/sys/ioccom.h /usr/include/sys/ttycom.h
xfer.o: /usr/include/sys/filio.h /usr/include/sys/ioccom.h
xfer.o: /usr/include/sys/sockio.h /usr/include/sys/ioccom.h
xfer.o: /usr/include/sys/socket.h /usr/include/sys/uio.h
xfer.o: /usr/include/netinet/in.h /usr/include/net/if.h /usr/include/stdio.h
xfer.o: /usr/include/syslog.h /usr/include/errno.h /usr/include/sys/errno.h
xfer.o: /usr/include/signal.h ../include/netdb.h /usr/include/arpa/inet.h
xfer.o: ../include/arpa/nameser.h ../include/resolv.h ns.h
xfer.o: /usr/include/strings.h /usr/include/arpa/inet.h ../include/pathnames.h
xfer.o: ../include/paths.h

# IF YOU PUT ANYTHING HERE IT WILL GO AWAY
