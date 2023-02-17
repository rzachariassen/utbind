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
#	@(#)Makefile	5.20 (Berkeley) 10/2/89
#
CFLAGS=	-O
RES=
LIBC=	/lib/libc.a
LIBS=	-ll ../../res/libresolv.a	# ${RES}
CSRCS=	main.c getinfo.c debug.c send.c skip.c list.c subr.c
SRCS=	${CSRCS} commands.c
OBJS=	main.o getinfo.o debug.o send.o skip.o list.o subr.o commands.o
MAN=	nslookup.0

all: nslookup

nslookup: ${OBJS} ${LIBC}
	${CC} -o $@ ${OBJS} ${LIBS}

clean:
	rm -f ${OBJS} core nslookup commands.c nslookup.0 lex.yy.c lex.yy.o

cleandir: clean
	rm -f ${MAN} tags .depend

depend: ${SRCS}
	mkdep ${CFLAGS} ${SRCS}

install: ${MAN}
	install -s -o bin -g bin -m 755 nslookup ${DESTDIR}/usr/local/nslookup
	install -c -o bin -g bin -m 444 nslookup.help ${DESTDIR}/usr/local/nslookup.help
	install -c -o bin -g bin -m 444 nslookup.0 ${DESTDIR}/usr/local/man/cat1/nslookup.0

lint: ${SRCS}
	lint ${SRCS}

tags: ${CSRCS}
	ctags ${CSRCS}
# DO NOT DELETE THIS LINE -- mkdep uses it.
# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.

main.o: main.c /usr/include/stdio.h /usr/include/strings.h
main.o: /usr/include/sys/param.h /usr/include/machine/param.h
main.o: /usr/include/sys/signal.h /usr/include/vm/faultcode.h
main.o: /usr/include/sys/types.h /usr/include/sys/sysmacros.h
main.o: /usr/include/netdb.h /usr/include/sys/socket.h
main.o: /usr/include/netinet/in.h /usr/include/arpa/nameser.h
main.o: /usr/include/resolv.h /usr/include/signal.h /usr/include/setjmp.h
main.o: /usr/include/machine/setjmp.h /usr/include/ctype.h res.h
getinfo.o: getinfo.c /usr/include/sys/types.h /usr/include/sys/sysmacros.h
getinfo.o: /usr/include/sys/socket.h /usr/include/netinet/in.h
getinfo.o: /usr/include/stdio.h /usr/include/ctype.h
getinfo.o: /usr/include/arpa/nameser.h /usr/include/resolv.h res.h
debug.o: debug.c /usr/include/sys/types.h /usr/include/sys/sysmacros.h
debug.o: /usr/include/netinet/in.h /usr/include/stdio.h
debug.o: /usr/include/arpa/nameser.h /usr/include/resolv.h /usr/include/netdb.h
debug.o: res.h
send.o: send.c /usr/include/sys/types.h /usr/include/sys/sysmacros.h
send.o: /usr/include/sys/time.h /usr/include/time.h /usr/include/sys/socket.h
send.o: /usr/include/netinet/in.h /usr/include/stdio.h
send.o: /usr/include/arpa/nameser.h /usr/include/resolv.h res.h
skip.o: skip.c /usr/include/sys/types.h /usr/include/sys/sysmacros.h
skip.o: /usr/include/netinet/in.h /usr/include/stdio.h
skip.o: /usr/include/arpa/nameser.h
list.o: list.c /usr/include/sys/types.h /usr/include/sys/sysmacros.h
list.o: /usr/include/sys/socket.h /usr/include/netinet/in.h
list.o: /usr/include/netdb.h /usr/include/stdio.h /usr/include/strings.h
list.o: /usr/include/ctype.h /usr/include/arpa/nameser.h /usr/include/resolv.h
list.o: res.h
subr.o: subr.c /usr/include/stdio.h /usr/include/strings.h
subr.o: /usr/include/sys/types.h /usr/include/sys/sysmacros.h
subr.o: /usr/include/netdb.h /usr/include/sys/socket.h
subr.o: /usr/include/netinet/in.h /usr/include/arpa/nameser.h
subr.o: /usr/include/signal.h /usr/include/vm/faultcode.h /usr/include/setjmp.h
subr.o: /usr/include/machine/setjmp.h res.h
commands.o: commands.c /usr/include/stdio.h res.h

# IF YOU PUT ANYTHING HERE IT WILL GO AWAY
