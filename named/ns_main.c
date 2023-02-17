/*
 * Copyright (c) 1986, 1989 Regents of the University of California.
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
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1986, 1989 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)ns_main.c	4.42 (Berkeley) 10/4/89";
#endif /* not lint */

/*
 * Internet Name server (see rfc883 & others).
 */

#include <sys/param.h>
#if defined(SYSV)
#include <fcntl.h>
#endif SYSV
#include <sys/file.h>
#include <sys/time.h>
#if !defined(SYSV)
#include <sys/wait.h>
#endif !SYSV
#include <sys/resource.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <stdio.h>
#include <syslog.h>
#include <errno.h>
#include <signal.h>
#include <netdb.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <resolv.h>
#undef nsaddr				/* XXX */
#include "ns.h"
#include "db.h"
#include "pathnames.h"

#ifndef	BOOTFILE
#define	BOOTFILE	_PATH_BOOT
#endif
#ifndef	DEBUGFILE
#define	DEBUGFILE	_PATH_DEBUG
#endif
#ifndef	PIDFILE
#define	PIDFILE		_PATH_PIDFILE
#endif
#ifndef	DUMPFILE
#define	DUMPFILE	_PATH_DUMPFILE
#endif
#ifndef	STATSFILE
#define	STATSFILE	_PATH_STATS
#endif
#ifndef	XFERPROG
#define	XFERPROG	_PATH_XFER
#endif

char *bootfile, *debugfile, *PidFile, *dumpfile, *statsfile, *xferprog;

struct cfileinfo {
	char	**varp;
	char	*dfltvalue;
} files[] = {
/* default boot file */
{	&bootfile,	BOOTFILE	},	/* used before init */
/* default debug output file */
{	&debugfile,	DEBUGFILE	},	/* used before init */
/* file to store current named PID */
{	&PidFile,	PIDFILE		},	/* used before init */
/* file to dump the cached data in */
{	&dumpfile,	DUMPFILE	},	/* used after init */
/* file to use for appending statistics */
{	&statsfile,	STATSFILE	},	/* used after init */
/* the named-xfer program */
{	&xferprog,	XFERPROG	},	/* used after init */
};

#ifndef FD_SET
#define	NFDBITS		32
#define	FD_SETSIZE	32
#define	FD_SET(n, p)	((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#define	FD_CLR(n, p)	((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#define	FD_ISSET(n, p)	((p)->fds_bits[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))
#define FD_ZERO(p)	bzero((char *)(p), sizeof(*(p)))
#endif

FILE	*fp;  				/* file descriptor for pid file */

#ifdef DEBUG
FILE	*ddt;
#endif

int	debug = 0;			/* debugging flag */
int	ds;				/* datagram socket */
int	read_interrupted = 0;		/* flag for read timer */
int	needreload = 0;			/* received SIGHUP, need to reload db */
int	needmaint = 0;			/* need to call ns_maint()*/
int	needzoneload = 0;		/* need to reload secondary zone(s) */
int     needToDoadump = 0;              /* need to dump database */
int     needToChkpt = 0;	        /* need to checkpoint cache */
int	needStatsDump = 0;		/* need to dump statistics */
#ifdef ALLOW_UPDATES
int     needToExit = 0;                 /* need to exit (may need to doadump
					 * first, if database has changed since
					 * it was last dumped/booted). Gets
					 * set by shutdown signal handler
					 *  (onintr)
					 */
#endif ALLOW_UPDATES

int	priming = 0;			/* is cache being primed */

#ifdef SO_RCVBUF
int	rbufsize = 8 * 1024;		/* UDP recive buffer size */
#endif

struct	qstream *streamq = QSTREAM_NULL; /* list of open streams */
struct	qdatagram *datagramq = QDATAGRAM_NULL; /* list of datagram interfaces */
struct	qdatagram *dqp, *sqp;
struct	sockaddr_in nsaddr;
struct	timeval tt;
struct	netinfo *nettab = NULL;
struct	netinfo *fnettab = NULL;
struct	netinfo *tcplist = NULL;
struct	netinfo *onettab = NULL;
struct	netinfo netloop;
u_short	ns_port;
struct	sockaddr_in from_addr;		/* Source addr of last packet */
int	from_len;			/* Source addr size of last packet */
time_t	boottime, resettime;		/* Used by ns_stats */
static fd_set	mask;			/* select mask of open descriptors */

#ifdef NET_ERRS
struct	errinfo *errlist = NULL;	/* list of open error sockets */
int	nerrsockets = 0;		/* number of open error sockets */
struct	in_addr err_interface;		/* interface to bind to XXX */
struct	errinfo *esp;

short	err_initport = 13272;		/* somewhat random port number */
#endif /* NET_ERRS */

char		**Argv = NULL;		/* pointer to argument vector */
char		*LastArg = NULL;	/* end of argv */

extern int errno;

#if defined(SYSV)
getdtablesize()
{
	return(FD_SETSIZE);
}
#endif SYSV

main(argc, argv, envp)
	int argc;
	char *argv[], *envp[];
{
	register int n, udpcnt;
	register char *arg;
	register struct qstream *sp;
	struct qstream *nextsp;
	int vs;
	int nfds;
	int on = 1;
	int rfd, size;
	u_long lasttime, maxctime;
	u_long nnn, nm;
	char buf[BUFSIZ];
#ifndef SYSV
	struct sigvec vec;
#endif

	fd_set tmpmask;

	struct timeval t, *tp;
	struct qstream *candidate = QSTREAM_NULL;
	struct ifconf ifc;
	struct ifreq ifreq, *ifr;
	struct netinfo *ntp;
	struct netinfo *ntip;
	struct netinfo *ontp;
	struct netinfo *ontip;
	extern int loadxfer();
	extern sigfunc_t onintr(), maint_alarm(), endxfer();
	extern sigfunc_t setdumpflg(), onhup();
	extern sigfunc_t setIncrDbgFlg(), setNoDbgFlg(), sigprof();
	extern sigfunc_t setchkptflg(), setstatsflg();
	extern struct qstream *sqadd();
	extern struct qinfo *qhead; 
	extern char Version[];

	ns_port = htons(NAMESERVER_PORT);

	/*
	**  Save start and extent of argv for setproctitle.
	*/

	Argv = argv;
#ifndef	ultrix
	if (envp == NULL || *envp == NULL)
#endif
		envp = argv;
	while (*envp)
		envp++;
	LastArg = envp[-1] + strlen(envp[-1]);

	for (n = 0; n < (sizeof files / sizeof files[0]); ++n)
		*files[n].varp = files[n].dfltvalue;
	(void) umask(022);
	while (--argc > 0) {	/* should use getopt() */
		arg = *++argv;
		if (*arg == '-') {
			while (*++arg)
				switch (*arg) {
				case 'b':
					if (--argc <= 0)
						usage();
					bootfile = savestr(*++argv);
					break;

  				case 'd':
 					++argv;

 					if (*argv != 0) {
 					    if (**argv == '-') {
 						argv--;
 						break;
 					    }
 					    debug = atoi(*argv);
 					    --argc;
 					}
					if (debug <= 0)
						debug = 1;
					setdebug(1);
					break;

				case 'p':
					if (--argc <= 0)
						usage();
					ns_port = htons((u_short)atoi(*++argv));
					break;

				default:
					usage();
				}
		} else
			bootfile = savestr(*argv);
	}

	if (!debug)
		for (n = getdtablesize() - 1; n > 2; n--)
			(void) close(n);
#ifdef DEBUG
	else {
		fprintf(ddt,"Debug turned ON, Level %d\n",debug);
		fprintf(ddt,"Version = %s\t",Version);
		fprintf(ddt,"bootfile = %s\n",bootfile);
	}		
#endif

#ifdef LOG_DAEMON
#ifdef LOGLEVEL
	openlog("named", LOG_PID|LOG_CONS|LOG_NDELAY, LOGLEVEL);
#else
	openlog("named", LOG_PID|LOG_CONS|LOG_NDELAY, LOG_DAEMON);
#endif /* LOGLEVEL */
#else
	openlog("named", LOG_PID);
#endif

	/* tuck my process id away */
	fp = fopen(PidFile, "w");
	if (fp != NULL) {
		fprintf(fp, "%d\n", getpid());
		(void) fclose(fp);
	}
	syslog(LOG_NOTICE, "restarted\n");

	_res.options &= ~(RES_DEFNAMES | RES_DNSRCH | RES_RECURSE);

	nsaddr.sin_family = AF_INET;
	nsaddr.sin_addr.s_addr = INADDR_ANY;
	nsaddr.sin_port = ns_port;
	/*
	** Initialize and load database.
	*/
	gettime(&tt);
	buildservicelist();
	buildprotolist();
	setproctitle("Reading bootfile");
	ns_init(bootfile);

	time(&boottime);
	resettime = boottime;

	(void) signal(SIGHUP, onhup);
#if defined(SYSV)
	(void) signal(SIGCLD, endxfer);
	(void) signal(SIGALRM, maint_alarm);
#else
	bzero((char *)&vec, sizeof(vec));
	vec.sv_handler = maint_alarm;
	vec.sv_mask = sigmask(SIGCHLD);
	(void) sigvec(SIGALRM, &vec, (struct sigvec *)NULL);

	vec.sv_handler = endxfer;
	vec.sv_mask = sigmask(SIGALRM);
	(void) sigvec(SIGCHLD, &vec, (struct sigvec *)NULL);
#endif SYSV
	(void) signal(SIGPIPE, SIG_IGN);
	(void) signal(SIGSYS, sigprof);
	(void) signal(SIGINT, setdumpflg);
	(void) signal(SIGQUIT, setchkptflg);
	(void) signal(SIGIOT, setstatsflg);

#ifdef ALLOW_UPDATES
        /* Catch SIGTERM so we can dump the database upon shutdown if it
           has changed since it was last dumped/booted */
        (void) signal(SIGTERM, onintr);
#endif ALLOW_UPDATES

#if defined(SIGUSR1) && defined(SIGUSR2)
	(void) signal(SIGUSR1, setIncrDbgFlg);
	(void) signal(SIGUSR2, setNoDbgFlg);
#else	SIGUSR1&&SIGUSR2
	(void) signal(SIGEMT, setIncrDbgFlg);
	(void) signal(SIGFPE, setNoDbgFlg);
#endif SIGUSR1&&SIGUSR2

#ifdef DEBUG
	if (debug) {
		fprintf(ddt,"database initialized\n");
	}
#endif
	setproctitle("Initializing structures");
	/*
	** Open stream port.
	*/
	if ((vs = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		syslog(LOG_ERR, "socket(SOCK_STREAM): %m");
		exit(1);
	}	
	(void)setsockopt(vs, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));
	if (bind(vs, (struct sockaddr *)&nsaddr, sizeof(nsaddr))) {
		syslog(LOG_ERR, "bind(vs, %s[%d]): %m",
			inet_ntoa(nsaddr.sin_addr), ntohs(nsaddr.sin_port));
		exit(1);
	}
	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;
	if (ioctl(vs, SIOCGIFCONF, (char *)&ifc) < 0) {
		syslog(LOG_ERR, "get interface configuration: %m");
		exit(1);
	}
	n = ifc.ifc_len/sizeof(struct ifreq);
	ntp = NULL;
	for (ifr = ifc.ifc_req; n > 0; n--, ifr++) {
		if (ifr->ifr_addr.sa_family != AF_INET)
			continue;
		ifreq = *ifr;
		/*
		 * Don't test IFF_UP, packets may still be received at this
		 * address if any other interface is up.
		 */
		if (ioctl(vs, SIOCGIFADDR, (char *)&ifreq) < 0) {
			syslog(LOG_ERR, "get interface addr: %m");
			continue;
		}
		/* build datagram queue */
		/* 
		 * look for an already existing source interface address.  If
		 * the machine has multiple point to point interfaces, then 
		 * the local address may appear more than once.
		 */		   
		for (sqp=datagramq; sqp != NULL; sqp = sqp->dq_next)
		    if (((struct sockaddr_in *)&ifreq.ifr_addr)->sin_addr.s_addr
		        == sqp->dq_addr.s_addr) {
#ifdef DEBUG
			  if (debug)
			      fprintf(ddt, "dup interface address %s on %s\n",
				    inet_ntoa(dqp->dq_addr), ifreq.ifr_name);
#endif		      
			  break;
		    }
		if (sqp != NULL)
			continue;

		if ((dqp = (struct qdatagram *)calloc(1,
		    sizeof(struct qdatagram))) == NULL) {
#ifdef DEBUG
			if (debug >= 5)
				fprintf(ddt,"main: malloc error\n");
#endif
			syslog(LOG_ERR, "main: Out Of Memory");
			exit(12);
		}
		dqp->dq_next = datagramq;
		datagramq = dqp;
		dqp->dq_addr = ((struct sockaddr_in *)&ifreq.ifr_addr)->sin_addr;

		if (ntp == NULL)
			ntp = (struct netinfo *)malloc(sizeof(struct netinfo));
		ntp->my_addr = 
		    ((struct sockaddr_in *)&ifreq.ifr_addr)->sin_addr;
#ifdef SIOCGIFNETMASK
		if (ioctl(vs, SIOCGIFNETMASK, (char *)&ifreq) < 0) {
			syslog(LOG_ERR, "get netmask: %m (using default)");
			ntp->mask = net_mask(ntp->my_addr);
		} else
			ntp->mask = ((struct sockaddr_in *)
			    &ifreq.ifr_addr)->sin_addr.s_addr;
#else
		/* 4.2 does not support subnets */
		ntp->mask = net_mask(ntp->my_addr);
#endif
		if (ioctl(vs, SIOCGIFFLAGS, (char *)&ifreq) < 0) {
			syslog(LOG_ERR, "get interface flags: %m");
			continue;
		}
#ifdef IFF_LOOPBACK
		if ((ifreq.ifr_flags & IFF_LOOPBACK))
#else
		/* test against 127.0.0.1 (yuck!!) */
		if (ntp->my_addr.s_addr == htonl(0x7F000001))
#endif
		{
		    netloop.my_addr = ntp->my_addr;
		    netloop.mask = 0xffffffff;
		    netloop.net = ntp->my_addr.s_addr;
		    netloop.next = NULL;
#ifdef DEBUG
		    if (debug) 
			fprintf(ddt,"loopback address: x%lx\n",
				netloop.my_addr.s_addr);
#endif DEBUG
		    continue;
		} else if ((ifreq.ifr_flags & IFF_POINTOPOINT)) {
			if (ioctl(vs, SIOCGIFDSTADDR, (char *)&ifreq) < 0) {
		    	    syslog(LOG_ERR, "get dst addr: %m");
		            continue;
			}
			ntp->mask = 0xffffffff;
			ntp->net = ((struct sockaddr_in *)
		    	    &ifreq.ifr_addr)->sin_addr.s_addr;
		} else {
			ntp->net = ntp->mask & ntp->my_addr.s_addr;
		}
		ntp->next = NULL;
		if (nettab == NULL)
			nettab = ntip = ntp;
		else {
			ntip->next = ntp;
			ntip = ntp;
		}
		ntp = NULL;
	}
	if (ntp)
		(void) free((char *)ntp);

	/*
	 * Create separate qdatagram structure for socket
	 * wildcard address.
	 */
	if ((dqp = (struct qdatagram *)calloc(1, sizeof(struct qdatagram)))
	     == NULL) {
#ifdef DEBUG
		if (debug >= 5)
			fprintf(ddt,"main: malloc error\n");
#endif
		syslog(LOG_ERR, "main: Out Of Memory");
		exit(12);
	}
	dqp->dq_next = datagramq;
	datagramq = dqp;
	dqp->dq_addr.s_addr = INADDR_ANY;

#ifdef NET_ERRS
	/*
	 * Create structures which will hold error socket info
	 */
	for (n = nerrsockets; n > 0; n--) {
		if ((esp = (struct errinfo *)calloc(1, sizeof(struct errinfo)))
		    == NULL) {
#ifdef DEBUG
			if (debug >= 5)
				fprintf(ddt,"main: malloc error\n");
#endif
			syslog(LOG_ERR, "main: Out Of Memory");
			exit(12);
		}

		/*
		 * All of these sockets must be connected to something
		 * when they are created to avoid address-in-use errors.
		 * We connect them to random ports on the local interface
		 *
		 */
		esp->e_dest.sin_family = AF_INET;
		esp->e_dest.sin_addr = err_interface;
		esp->e_dest.sin_port = err_initport;
		err_initport++;

		esp->e_next = errlist;
		errlist = esp;
	}
#endif /* NET_ERRS */

	/*
	 * Compute other networks for sorting based on network configuration
	 */
	for (ntp = nettab; ntp != NULL; ntp = ntp->next) {
		nm = net_mask(ntp->my_addr);
		if (nm != ntp->mask) {
			nnn = ntp->my_addr.s_addr & nm;
			for (ontp = onettab; ontp != NULL; ontp = ontp->next) {
				if ((ontp->net == nnn) &&
				    (ontp->mask == nm))
					goto outerloop;
				ontip = ontp;
			}
			ontp = (struct netinfo *)malloc(sizeof(struct netinfo));
			ontp->net = nnn;
			ontp->mask = nm;
			ontp->my_addr = ntp->my_addr;
			ontp->next = NULL;
			if (onettab == NULL) {
				onettab = ontp;
				ntip->next = ontp;
			} else
				ontip->next = ontp;
		}
outerloop:
		;
	}

	/* 
	 * Merge sort list from boot file with list from 
	 * network configuration
	 */
	for (ntp = nettab; ntp != NULL; ntp = ntp->next) {
	    for (ontp = fnettab; ontp != NULL; ontp = ontp->next) {
		if ((ontp->mask == ntp->mask) && (ontp->net == ntp->net)) {
		    if (fnettab == ontp) {
			fnettab = ontp->next;
			free((char *)ontp);
			break;
		    } else {
		   	for (ontip = fnettab; ontip != NULL;ontip = ontip->next)
			   if (ontip->next == ontp) break;
			ontip->next = ontp->next;
			free((char *)ontp);
			ontp = ontip;
			break;
		    }
		}
	    }
	    if (ntp->next == NULL)
		break;
	}
	if (ntp)
		ntp->next = fnettab;
	else
		nettab = fnettab;
#ifdef DEBUG
	if (debug)
		for (ntp = nettab; ntp != NULL; ntp = ntp->next) {
			fprintf(ddt,"net x%lx mask x%lx", ntp->net, ntp->mask);
			fprintf(ddt," my_addr x%lx", ntp->my_addr.s_addr);
			fprintf(ddt," %s\n", inet_ntoa(ntp->my_addr));
		}
#endif DEBUG
	(void) listen(vs, 5);

#ifdef NET_ERRS
	/*
	 * Open connectable datagram sockets to receive error indications.
	 * 
	 */
	for (esp = errlist; esp != NULL; esp = esp->e_next) {
		esp->e_fd = socket(AF_INET, SOCK_DGRAM, 0);
		if (esp->e_fd < 0) {
			syslog(LOG_ERR, "socket(SOCK_DGRAM): %m");
			exit(1);
		}
#ifdef DEBUG
		if (debug > 4)
			fprintf(ddt, "opening error socket: fd %d, idle %s\n",
			    esp->e_fd, inet_ntoa(esp->e_dest.sin_addr));
#endif
		(void)setsockopt(esp->e_fd, SOL_SOCKET, SO_REUSEADDR,
		    (char *)&on, sizeof(on));
#ifdef SO_RCVBUF
		(void)setsockopt(esp->e_fd, SOL_SOCKET, SO_RCVBUF,
		    (char *)&rbufsize, sizeof(rbufsize));
#endif SO_RCVBUF
		(void) fcntl(esp->e_fd, F_SETFL, FNDELAY);

		/*
		 * Note that here nsaddr is set to the proper
		 * port number but with the wildcard address.
		 * This suits us fine.  See comments below about
		 * Sun's screwing up on the bind.
		 */
		if (bind(esp->e_fd, (struct sockaddr *)&nsaddr,
		    sizeof(nsaddr))) {
			syslog(LOG_ERR, "bind(e_fd %d, %s[%d]): %m",
			    esp->e_fd, inet_ntoa(nsaddr.sin_addr),
			    ntohs(nsaddr.sin_port));
#if !defined(sun)
			exit(1);
#endif
		}
		/*
		 * Connect it to the initial address.
		 */
		if (connect(esp->e_fd, (struct sockaddr *)&esp->e_dest,
		    sizeof (struct sockaddr_in))) {
			syslog(LOG_ERR, "connect(e_fd %d, %s[%d]): %m",
			    esp->e_fd, inet_ntoa(esp->e_dest.sin_addr),
			    ntohs(esp->e_dest.sin_port));
			exit(1);
		}
	}
#endif /* NET_ERRS */

	/*
	 * Open datagram ports for each interface
	 * (the first is un-addressed datagram port).
	 */
	for (dqp = datagramq; dqp != QDATAGRAM_NULL; dqp = dqp->dq_next) {
		nsaddr.sin_addr = dqp->dq_addr;
		if ((dqp->dq_dfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			syslog(LOG_ERR, "socket(SOCK_DGRAM): %m");
			exit(1);
		}	
#ifdef DEBUG
		if (debug)
			fprintf(ddt,"dqp->dq_addr %s d_dfd %d\n",
			    inet_ntoa(dqp->dq_addr), dqp->dq_dfd);
#endif DEBUG
		(void)setsockopt(dqp->dq_dfd, SOL_SOCKET, SO_REUSEADDR,
		    (char *)&on, sizeof(on));
#ifdef SO_RCVBUF
		(void)setsockopt(dqp->dq_dfd, SOL_SOCKET, SO_RCVBUF,
		    (char *)&rbufsize, sizeof(rbufsize));
#endif SO_RCVBUF
		(void) fcntl(dqp->dq_dfd, F_SETFL, FNDELAY);
/*
 *   NOTE: Some versions of SunOS have problems with the following
 *   call to bind.  Bind still seems to function on these systems
 *   if you comment out the exit inside the if.  This may cause
 *   Suns with multiple interfaces to reply strangely.
 */
		if (bind(dqp->dq_dfd, (struct sockaddr *)&nsaddr, sizeof(nsaddr))) {
			syslog(LOG_ERR, "bind(dfd %d, %s[%d]): %m",
				dqp->dq_dfd, inet_ntoa(nsaddr.sin_addr),
				ntohs(nsaddr.sin_port));
#if !defined(sun)
			exit(1);
#endif
		}
	}
	ds = datagramq->dq_dfd;

#ifdef NET_ERRS
	/*
	 * For each of the error sockets, find the datagram port which
	 * is bound to the same local address and record its file descriptor.
	 * This allows us to receive data on the error sockets and get
	 * the return address pointed at the right spot.
	 */
	for (esp = errlist; esp != NULL; esp = esp->e_next) {
		from_len = sizeof(from_addr);
		if (getsockname(esp->e_fd, (struct sockaddr *)&from_addr,
		    &from_len) != 0) {
			syslog(LOG_ERR, "getsockname(e_fd %d): %m", esp->e_fd);
			exit(1);
		}
		for (dqp = datagramq; dqp != QDATAGRAM_NULL;
		    dqp = dqp->dq_next)
			if (dqp->dq_addr.s_addr == from_addr.sin_addr.s_addr)
				break;
		if (dqp == NULL) {
			syslog(LOG_ERR,
			    "error address %s not found in datagram queue",
			    inet_ntoa(from_addr.sin_addr));
			exit(1);
		}
		esp->e_dfd = dqp->dq_dfd;
	}
#endif /* NET_ERRS */

	t.tv_usec = 0;

	/*
	 * Fork and go into background now that
	 * we've done any slow initialization
	 * and are ready to answer queries.
	 */
	if (!debug) {
		if (fork() > 0)
			exit(0);
		n = open(_PATH_DEVNULL, O_RDONLY);
		(void) dup2(n, 0);
		(void) dup2(n, 1);
		(void) dup2(n, 2);
		if (n > 2)
			(void) close(n);
		n = open(_PATH_TTY, O_RDWR);
		if (n > 0) {
#if defined(BSD) && BSD >= 198810	/* POSIX */
			setsid();
#else
#ifndef SYSV
			(void) ioctl(n, TIOCNOTTY, (char *)NULL);
#else
			setpgrp();
#endif
#endif
			(void) close(n);
		}
		/* tuck my process id away again */
		fp = fopen(PidFile, "w");
		if (fp != NULL) {
			fprintf(fp, "%d\n", getpid());
			(void) fclose(fp);
		}
	}

#ifdef DEBUG
	if (debug)
		fprintf(ddt,"Ready to answer queries.\n");
#endif
	setproctitle("Ready for queries");
	prime_cache();
	nfds = getdtablesize();       /* get the number of file descriptors */
	if (nfds > FD_SETSIZE) {
		nfds = FD_SETSIZE;	/* Bulletproofing */
		syslog(LOG_ERR, "Return from getdtablesize() > FD_SETSIZE");
#ifdef DEBUG
		if (debug)
		      fprintf(ddt,"Return from getdtablesize() > FD_SETSIZE\n");
#endif
	}
	FD_ZERO(&mask);
	FD_SET(vs, &mask);
	FD_SET(ds, &mask);
	for (dqp = datagramq; dqp != QDATAGRAM_NULL; dqp = dqp->dq_next)
		FD_SET(dqp->dq_dfd, &mask);
#ifdef NET_ERRS
	for (esp = errlist; esp != NULL; esp = esp->e_next)
		FD_SET(esp->e_fd, &mask);
#endif /* NET_ERRS */
	for (;;) {
#ifdef DEBUG
		if (ddt && debug == 0) {
			fprintf(ddt,"Debug turned OFF\n");
			(void) fclose(ddt);
			ddt = 0;
		}
#endif
#ifdef ALLOW_UPDATES
                if (needToExit) {
			struct zoneinfo *zp;
			sigblock(~0);   /*
					 * Block all blockable signals
					 * to ensure a consistant
					 * state during final dump
					 */
#ifdef DEBUG
			if (debug)
				fprintf(ddt, "Received shutdown signal\n");                     
#endif DEBUG
			setproctitle("Final dump");
			for (zp = zones; zp < &zones[nzones]; zp++) {
				if (zp->hasChanged)
					zonedump(zp);
                        }
                        exit(0);
                }
#endif ALLOW_UPDATES
		if (needreload) {
			needreload = 0;
			setproctitle("Reloading nameserver");
			db_reload();
		}
#ifdef STATS
		if (needStatsDump) {
			needStatsDump = 0;
			setproctitle("Printing statistics");
			ns_stats();
		}
#endif STATS
		if (needzoneload) {
			needzoneload = 0;
			loadxfer();
		}
		if (needmaint) {
                        needmaint = 0;
			setproctitle("Doing maintenance");
                        ns_maint();
                }
	        if (needToChkpt) {
                        needToChkpt = 0;
			setproctitle("Checkpointing nameserver");
                        doachkpt();
	        }
                if (needToDoadump) {
                        needToDoadump = 0;
			setproctitle("Dumping nameserver");
                        doadump();
                }
		/*
		** Wait until a query arrives
		*/
		setproctitle("Waiting for query");
		if (retryqp != NULL) {
			gettime(&tt);
			t.tv_sec = (long) retryqp->q_time - tt.tv_sec;
			if (t.tv_sec <= 0) {
				retry(retryqp);
				continue;
			}
			tp = &t;
		} else
			tp = NULL;
		tmpmask = mask;
		n = select(nfds, &tmpmask, (fd_set *)NULL, (fd_set *)NULL, tp);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			syslog(LOG_ERR, "select: %m");
#ifdef DEBUG
			if (debug)
				fprintf(ddt,"select error\n");
#endif
			;
		}
		if (n == 0)
			continue;

#ifdef NET_ERRS
		/*
		 * Process errors and the like first, if any.  This will free
		 * up error sockets to be reused by simultaneously
		 * incoming requests.  Data received on these sockets is
		 * treated as if it had arrived on the datagram socket.
		 */
		for (esp = errlist; esp != NULL; esp = esp->e_next) {
			if (!FD_ISSET(esp->e_fd, &tmpmask))
				continue;
		        for (udpcnt = 0; udpcnt < 25; udpcnt++) {
			    from_len = sizeof(from_addr);
			    if ((n = recvfrom(esp->e_fd, buf, sizeof(buf), 0,
				(struct sockaddr *)&from_addr, &from_len)) < 0)
			    {
				if (errno == EWOULDBLOCK)
					break;
				if (ISNETERR(errno)) {
					neterror(esp);
					continue;
				}
#ifdef DEBUG
				if (debug) {
					fprintf(ddt,
	"Got unknown error %d on error socket: fd %d, addr %s, links %d\n",
					    errno, esp->e_fd,
					    inet_ntoa(esp->e_dest.sin_addr),
					    esp->e_count);
				}
#endif
				syslog(LOG_ERR,
				    "unexpected error on error socket: %m");
				break;
			    }
#ifdef STATS
			    stats[S_INPKTS].cnt++;
#endif
#ifdef DEBUG
			    if (debug)
				fprintf(ddt,
				"\ndatagram from %s port %d, e_fd %d, len %d\n",
				    inet_ntoa(from_addr.sin_addr),
				    ntohs(from_addr.sin_port), esp->e_fd, n);
			    if (debug >= 10)
				fp_query(buf, n, ddt);
#endif
			    esp->e_traffic++;
			    /*
			     * Consult database to get the answer.
			     */
			    gettime(&tt);
			    ns_req(buf, n, PACKETSZ, QSTREAM_NULL, &from_addr,
				    esp->e_dfd, (fd_set *)NULL);
		        }
		}
#endif /* NET_ERRS */

		for (dqp = datagramq; dqp != QDATAGRAM_NULL;
		    dqp = dqp->dq_next) {
		    if (FD_ISSET(dqp->dq_dfd, &tmpmask))
		        for(udpcnt = 0; udpcnt < 25; udpcnt++) {
			    from_len = sizeof(from_addr);
			    if ((n = recvfrom(dqp->dq_dfd, buf, sizeof(buf), 0,
				(struct sockaddr *)&from_addr, &from_len)) < 0)
			    {
				if ((n == -1) && (errno == EWOULDBLOCK))
					break;
				syslog(LOG_WARNING, "recvfrom: %m");
				break;
			    }
#ifdef STATS
			    stats[S_INPKTS].cnt++;
#endif
#ifdef DEBUG
			    if (debug)
				fprintf(ddt,
				  "\ndatagram from %s port %d, fd %d, len %d\n",
				    inet_ntoa(from_addr.sin_addr),
				    ntohs(from_addr.sin_port), ds, n);
			    if (debug >= 10)
				fp_query(buf, n, ddt);
#endif
			    /*
			     * Consult database to get the answer.
			     */
			    gettime(&tt);
			    ns_req(buf, n, PACKETSZ, QSTREAM_NULL, &from_addr,
				    dqp->dq_dfd, (fd_set *)NULL);
		        }
		}
		/*
		** Process stream connection
		*/
		if (FD_ISSET(vs, &tmpmask)) {
			from_len = sizeof(from_addr);
			rfd = accept(vs, (struct sockaddr *)&from_addr, &from_len);
			gettime(&tt);
			if (rfd < 0 && errno == EMFILE && streamq != NULL) {
				maxctime = 0;
				candidate = QSTREAM_NULL;
				for (sp = streamq; sp != QSTREAM_NULL;
				   sp = nextsp) {
					nextsp = sp->s_next;
					if (sp->s_refcnt != 0)
					    continue;
					lasttime = tt.tv_sec - sp->s_time;
					if (lasttime >= 900)
					    sqrm(sp);
					else if (lasttime > maxctime) {
					    candidate = sp;
					    maxctime = lasttime;
					}
				}
				rfd = accept(vs, (struct sockaddr *)&from_addr, &from_len);
				if ((rfd < 0) && (errno == EMFILE) &&
				    candidate != QSTREAM_NULL) {
					if (candidate->s_refcnt == 0)
						sqrm(candidate);
					rfd = accept(vs, (struct sockaddr *)&from_addr, &from_len);
				}
			}
			if (rfd < 0) {
				syslog(LOG_WARNING, "accept: %m");
				continue;
			}
			if (tcplist) {
				/* if tcplist was specified, peer address
				 * must be on it.  should probably allow
				 * for negation some day. (vixie@decwrl)
				 */
				struct sockaddr saddr;
				int slen = (sizeof saddr);
				struct sockaddr_in *iaddr =
					(struct sockaddr_in *) &saddr;

				if (getpeername(rfd, &saddr, &slen) == -1) {
					syslog(LOG_WARNING, "getpeername: %m");
					close(rfd);
					continue;
				}
				if (saddr.sa_family != AF_INET) {
					syslog(LOG_WARNING, "af%d???", saddr.sa_family);
					close(rfd);
					continue;
				}
				if (!net_on_tcp_list(iaddr->sin_addr)) {
					syslog(LOG_INFO,
						"dangerous TCP addr: %s %u",
						inet_ntoa(iaddr->sin_addr),
						ntohs(iaddr->sin_port));
						close(rfd);
						continue;
				}
			}
			(void) fcntl(rfd, F_SETFL, FNDELAY);
			(void) setsockopt(rfd, SOL_SOCKET, SO_KEEPALIVE,
				(char *)&on, sizeof(on));
			if ((sp = sqadd()) == QSTREAM_NULL) {
				(void) close(rfd);
				continue;
			}
			sp->s_rfd = rfd;	/* stream file descriptor */
			sp->s_size = -1;	/* amount of data to receive */
			gettime(&tt);
			sp->s_time = tt.tv_sec;	/* last transaction time */
			sp->s_from = from_addr;	/* address to respond to */
			sp->s_bufp = (char *)&sp->s_tempsize;
			FD_SET(rfd, &mask);
			FD_SET(rfd, &tmpmask);
#ifdef DEBUG
			if (debug) {
				fprintf(ddt,
				   "\nTCP connection from %s port %d (fd %d)\n",
					inet_ntoa(sp->s_from.sin_addr),
					ntohs(sp->s_from.sin_port), rfd);
			}
#endif
		}
#ifdef DEBUG
		if (debug > 2 && streamq)
			fprintf(ddt,"streamq  = x%x\n",streamq);
#endif
		if (streamq != NULL) {
			for (sp = streamq; sp != QSTREAM_NULL; sp = nextsp) {
			    nextsp = sp->s_next;
			    if (FD_ISSET(sp->s_rfd, &tmpmask)) {
#ifdef DEBUG
				if (debug > 5) {
				    fprintf(ddt,
					"sp x%x rfd %d size %d time %d ",
					sp, sp->s_rfd, sp->s_size,
					sp->s_time );
				    fprintf(ddt," next x%x \n", sp->s_next );
				    fprintf(ddt,"\tbufsize %d",sp->s_bufsize);
				    fprintf(ddt," buf x%x ",sp->s_buf);
				    fprintf(ddt," bufp x%x\n",sp->s_bufp);
				}
#endif DEBUG
			    if (sp->s_size < 0) {
			        size = sizeof(u_short) -
				   (sp->s_bufp - (char *)&sp->s_tempsize);
			        while (size > 0 &&
			           (n = read(sp->s_rfd, sp->s_bufp, size)) > 0){
					    sp->s_bufp += n;
					    size -= n;
			        }
			        if ((n == -1) && (errno == EWOULDBLOCK))
					    continue;
			        if (n <= 0) {
					    sqrm(sp);
					    continue;
			        }
			        if ((sp->s_bufp - (char *)&sp->s_tempsize) ==
					sizeof(u_short)) {
					sp->s_size = htons(sp->s_tempsize);
					if (sp->s_bufsize == 0) {
					    if ( (sp->s_buf = malloc(BUFSIZ))
						== NULL) {
						    sp->s_buf = buf;
						    sp->s_size  = sizeof(buf);
					    } else {
						    sp->s_bufsize = BUFSIZ;
					    }
					}
					if (sp->s_size > sp->s_bufsize &&
					  sp->s_bufsize != 0) {
					    if ((sp->s_buf = realloc(
						(char *)sp->s_buf,
						(unsigned)sp->s_size)) == NULL){
						    sp->s_buf = buf;
						    sp->s_bufsize = 0;
						    sp->s_size  = sizeof(buf);
					   } else {
						    sp->s_bufsize = sp->s_size;
					   }
					}
					sp->s_bufp = sp->s_buf;	
				    }
			    }
			    gettime(&tt);
			    sp->s_time = tt.tv_sec;
			    while (sp->s_size > 0 &&
			      (n = read(sp->s_rfd, sp->s_bufp, sp->s_size)) > 0)
			    {
				    sp->s_bufp += n;
				    sp->s_size -= n;
			    }
			    /*
			     * we don't have enough memory for the query.
			     * if we have a query id, then we will send an
			     * error back to the user.
			     */
			    if (sp->s_bufsize == 0 &&
				(sp->s_bufp - sp->s_buf > sizeof(u_short))) {
				    HEADER *hp;

				    hp = (HEADER *)sp->s_buf;
				    hp->qr = 1;
				    hp->ra = 1;
				    hp->ancount = 0;
				    hp->qdcount = 0;
				    hp->nscount = 0;
				    hp->arcount = 0;
				    hp->rcode = SERVFAIL;
				    (void) writemsg(sp->s_rfd, sp->s_buf,
					sizeof(HEADER));
				    continue;
			    }
			    if ((n == -1) && (errno == EWOULDBLOCK))
				    continue;
			    if (n <= 0) {
				    sqrm(sp);
				    continue;
			    }
			    /*
			     * Consult database to get the answer.
			     */
			    if (sp->s_size == 0) {
				    sq_query(sp);
				    ns_req(sp->s_buf,
					sp->s_bufp - sp->s_buf,
					sp->s_bufsize, sp,
					&sp->s_from, -1, &mask);
				    sp->s_bufp = (char *)&sp->s_tempsize;
				    sp->s_size = -1;
				    continue;
			    }
			}
		    }
		}
	}
	/* NOTREACHED */
}

usage()
{
	fprintf(stderr, "Usage: named [-d #] [-p port] [{-b} bootfile]\n");
	exit(1);
}


/*
** Set flag saying to reload database upon receiving SIGHUP.
** Must make sure that someone isn't walking through a data
** structure at the time.
*/

sigfunc_t
onhup()
{
#if defined(SYSV)
	(void)signal(SIGHUP, onhup);
#endif SYSV
	needreload = 1;
}

/*
** Set flag saying to call ns_maint()
** Must make sure that someone isn't walking through a data
** structure at the time.
*/

sigfunc_t
maint_alarm()
{
#if defined(SYSV)
	(void)signal(SIGALRM, maint_alarm);
#endif SYSV
	needmaint = 1;
 }


#ifdef ALLOW_UPDATES
/*
 * Signal handler to schedule shutdown.  Just set flag, to ensure a consistent
 * state during dump.
 */
sigfunc_t
onintr()
{
        needToExit = 1;
}
#endif ALLOW_UPDATES

/*
 * Signal handler to schedule a data base dump.  Do this instead of dumping the
 * data base immediately, to avoid seeing it in a possibly inconsistent state
 * (due to updates), and to avoid long disk I/O delays at signal-handler
 * level
 */
sigfunc_t
setdumpflg()
{
#if defined(SYSV)
	(void)signal(SIGINT, setdumpflg);
#endif SYSV
        needToDoadump = 1;
}

/*
** Set flag saying to read was interrupted
** used for a read timer
*/
sigfunc_t
read_alarm()
{
	extern int read_interrupted;
	read_interrupted = 1;
}


/*
** Turn on or off debuging by open or closeing the debug file
*/

setdebug(code)
int code;
{
#if defined(lint) && !defined(DEBUG)
	code = code;
#endif
#ifdef DEBUG

	if (code) {
		ddt = freopen(debugfile, "w+", stderr);
		if (ddt == NULL) {
			syslog(LOG_WARNING, "can't open debug file %s: %m",
			    debugfile);
			debug = 0;
		} else {
#if defined(SYSV)
			setvbuf(ddt, NULL, _IOLBF, BUFSIZ);
#else
			setlinebuf(ddt);
#endif
			(void) fcntl(fileno(ddt), F_SETFL, FAPPEND);
		}
	} else
		debug = 0;
		/* delay closing ddt, we might interrupt someone */
#endif
}

/*
** Catch a special signal and set debug level.
**
**  If debuging is off then turn on debuging else increment the level.
**
** Handy for looking in on long running name servers.
*/
sigfunc_t
setIncrDbgFlg()
{
#if defined(SYSV)
	(void)signal(SIGUSR1, setIncrDbgFlg);
#endif SYSV
#ifdef DEBUG
	if (debug == 0) {
		debug++;
		setdebug(1);
	}
	else {
		debug++;
	}
	fprintf(ddt,"Debug turned ON, Level %d\n",debug);
#endif
}

/*
** Catch a special signal to turn off debugging
*/
sigfunc_t
setNoDbgFlg()
{
#if defined(SYSV)
	(void)signal(SIGUSR2, setNoDbgFlg);
#endif SYSV
	setdebug(0);
}

/*
** Set flag for statistics dump
*/
sigfunc_t
setstatsflg()
{
#if defined(SYSV)
	(void)signal(SIGIOT, setstatsflg);
#endif SYSV
	needStatsDump = 1;
}

sigfunc_t
setchkptflg()
{
#if defined(SYSV)
	(void)signal(SIGQUIT, setchkptflg);
#endif SYSV
	needToChkpt = 1;
}

/*
** Catch a special signal SIGSYS
**
**  this is setup to fork and exit to drop to /usr/tmp/gmon.out
**   and keep the server running
*/
sigfunc_t
sigprof()
{
#if defined(SYSV)
	(void)signal(SIGSYS, sigprof);
#endif SYSV
#ifdef DEBUG
	if (debug)
		fprintf(ddt,"sigprof()\n");
#endif
	if ( fork() == 0)
	{
		(void) chdir(_PATH_TMPDIR);
		exit(1);
	}
}

/*
** Routines for managing stream queue
*/

struct qstream *
sqadd()
{
	register struct qstream *sqp;

	if ((sqp = (struct qstream *)calloc(1, sizeof(struct qstream)))
	    == NULL ) {
#ifdef DEBUG
		if (debug >= 5)
			fprintf(ddt,"sqadd: malloc error\n");
#endif
		syslog(LOG_ERR, "sqadd: Out Of Memory");
		return(QSTREAM_NULL);
	}
#ifdef DEBUG
	if (debug > 3)
		fprintf(ddt,"sqadd(x%x)\n", sqp);
#endif

	sqp->s_next = streamq;
	streamq = sqp;
	return(sqp);
}

/*
 * Remove stream queue structure.
 * No current queries may refer to this stream when it is removed.
 */
sqrm(qp)
	register struct qstream *qp;
{
	register struct qstream *qsp;
	register struct qinfo *info;
	extern struct qinfo *qhead;

#ifdef DEBUG
	if (debug > 1) {
		fprintf(ddt,"sqrm(%#x, %d ) rfcnt=%d\n",
		    qp, qp->s_rfd, qp->s_refcnt);
	}
#endif
	/*
	 * Note: there may be outstanding queries on this stream, so
	 * mark the forwarding info structure to NOT send anything to
	 * the user, for example by changing the outstanding query from
	 * a USER to a SYSTEM query.
	 */
	for(info = qhead; info != QINFO_NULL; info = info->q_link)
		if (info->q_stream == qp && info->q_system == 0)
			info->q_system = USER_RESET;

	if (qp->s_bufsize != 0)
		(void) free(qp->s_buf);
	FD_CLR(qp->s_rfd, &mask);
	(void) close(qp->s_rfd);
	if (qp == streamq) {
		streamq = qp->s_next;
	} else {
		for (qsp = streamq; qsp->s_next != qp; qsp = qsp->s_next)
			;
		qsp->s_next = qp->s_next;
	}
	(void)free((char *)qp);
}

sqflush()
{
	register struct qstream *sp, *spnext;

	for (sp = streamq; sp != QSTREAM_NULL; sp = spnext) {
		spnext = sp->s_next;
		sqrm(sp);
	}
}

/*
 * Initiate query on stream;
 * mark as referenced and stop selecting for input.
 */
sq_query(sp)
	register struct qstream *sp;
{
	sp->s_refcnt++;
	FD_CLR(sp->s_rfd, &mask);
}

/*
 * Note that the current request on a stream has completed,
 * and that we should continue looking for requests on the stream.
 */
sq_done(sp)
	register struct qstream *sp;
{

	sp->s_refcnt = 0;
	sp->s_time = tt.tv_sec;
	FD_SET(sp->s_rfd, &mask);
}

setproctitle(buf)
	char *buf;
{
	register char *p;
	register int i;
	extern char **Argv;
	extern char *LastArg;

	/* make ps print "(named)" */
	p = Argv[0];
	*p++ = '-';

	i = strlen(buf);
	if (i > LastArg - p - 2)
	{
		i = LastArg - p - 2;
		buf[i] = '\0';
	}
	(void) strcpy(p, buf);
	p += i;
	while (p < LastArg)
		*p++ = ' ';
}

net_mask(in)
struct in_addr in;
{
	register u_long i = ntohl(in.s_addr);

	if (IN_CLASSA(i))
		return (htonl(IN_CLASSA_NET));
	else if (IN_CLASSB(i))
		return (htonl(IN_CLASSB_NET));
	else
		return (htonl(IN_CLASSC_NET));
}

gettime(ttp)
struct timeval *ttp;
{
	if (gettimeofday(ttp, (struct timezone *)0) < 0)
		syslog(LOG_ERR, "gettimeofday failed: %m");
	return;
}
