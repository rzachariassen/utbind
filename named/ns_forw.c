/*
 * Copyright (c) 1986 Regents of the University of California.
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
static char sccsid[] = "@(#)ns_forw.c	4.28 (Berkeley) 10/4/89";
#endif /* not lint */

#include <stdio.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <syslog.h>
#include <errno.h>
#include <arpa/nameser.h>
#include "ns.h"
#include "db.h"

struct	qinfo *qhead = QINFO_NULL;	/* head of allocated queries */
struct	qinfo *retryqp = QINFO_NULL;	/* list of queries to retry */
struct	fwdinfo *fwdtab;		/* list of forwarding hosts */

int	nsid;				/* next forwarded query id */
extern int forward_only;		/* you are only a slave */
extern int errno;
extern u_short ns_port;

time_t	retrytime();

/*
 * Forward the query to get the answer since its not in the database.
 * Returns FW_OK if a request struct is allocated and the query sent.
 * Returns FW_DUP if this is a duplicate of a pending request. 
 * Returns FW_NOSERVER if there were no addresses for the nameservers.
 * Returns FW_SERVFAIL on malloc error.
 * (no action is taken on errors and qpp is not filled in.)
 */
ns_forw(nsp, msg, msglen, fp, qsp, dfd, qpp)
	struct databuf *nsp[];
	char *msg;
	int msglen;
	struct sockaddr_in *fp;
	struct qstream *qsp;
	int dfd;
	struct qinfo **qpp;
{
	register struct qinfo *qp;
	HEADER *hp;
	u_short id;
	extern char *calloc();

#ifdef DEBUG
	if (debug >= 3)
		fprintf(ddt,"ns_forw()\n");
#endif

	/* Don't forward if we're already working on it. */
	hp = (HEADER *) msg;
	id = hp->id;
	/* Look at them all */
	for (qp = qhead; qp!=QINFO_NULL; qp = qp->q_link) {
		if (qp->q_id == id &&
		    bcmp((char *)&qp->q_from, fp, sizeof(qp->q_from)) == 0 &&
		    (qp->q_cmsglen == 0 && qp->q_msglen == msglen &&
		     bcmp((char *)qp->q_msg+2, msg+2, msglen-2) == 0) ||
		    (qp->q_cmsglen == msglen &&
		     bcmp((char *)qp->q_cmsg+2, msg+2, msglen-2) == 0)) {
#ifdef DEBUG
			if (debug >= 3)
				fprintf(ddt,"forw: dropped DUP id=%d\n", ntohs(id));
#endif
#ifdef STATS
			stats[S_DUPQUERIES].cnt++;
#endif
			return (FW_DUP);
		}
	}

	qp = qnew();
	if (nslookup(nsp, qp) == 0) {
#ifdef DEBUG
		if (debug >= 2)
			fprintf(ddt,"forw: no nameservers found\n");
#endif
		qfree(qp);
		return (FW_NOSERVER);
	}
	qp->q_stream = qsp;
#ifdef DEBUG
	if (debug >= 5)
		fprintf(ddt, "forw: q_stream = %X\n", qsp);
#endif
	qp->q_curaddr = 0;
	qp->q_fwd = fwdtab;
	qp->q_dfd = dfd;
	qp->q_id = id;
	hp->id = qp->q_nsid = htons((u_short)++nsid);
	hp->ancount = 0;
	hp->nscount = 0;
	hp->arcount = 0;

	qp->q_from = *fp;
	if ((qp->q_msg = malloc((unsigned)msglen)) == NULL) {
		syslog(LOG_ERR, "forw: %m");
		qfree(qp);
		return (FW_SERVFAIL);
	}
	bcopy(msg, qp->q_msg, qp->q_msglen = msglen);
	if (!qp->q_fwd) {
		hp->rd = 0;
		qp->q_addr[0].stime = tt;
	}

	if (!sendquery(qp))
		return (FW_FAILED);

	if (qpp)
		*qpp = qp;
	return (0);
}

/*
 * Lookup the address for each nameserver in `nsp' and add it to
 * the list saved in the qinfo structure.
 */
nslookup(nsp, qp)
	struct databuf *nsp[];
	register struct qinfo *qp;
{
	register struct namebuf *np;
	register struct databuf *dp, *nsdp;
	register struct qserv *qs;
	register int n, i;
	struct hashbuf *tmphtp;
	char *dname, *fname;
	int oldn, naddr, class, found_arr;
	time_t curtime;
	int qcomp();

#ifdef DEBUG
	if (debug >= 3)
		fprintf(ddt,"nslookup(nsp=x%x,qp=x%x)\n",nsp,qp);
#endif

	naddr = n = qp->q_naddr;
	curtime = (u_long) tt.tv_sec;
	while ((nsdp = *nsp++) != NULL) {
		class = nsdp->d_class;
		dname = nsdp->d_data;
#ifdef DEBUG
		if (debug >= 3)
			fprintf(ddt,"nslookup: NS %s c%d t%d (x%x)\n",
				dname, class, nsdp->d_type, nsdp->d_flags);
#endif
		/* don't put in people we have tried */
		for (i = 0; i < qp->q_nusedns; i++)
			if (qp->q_usedns[i] == nsdp) {
#ifdef DEBUG
				if (debug >= 2)
fprintf(ddt, "skipping used NS w/name %s\n", nsdp->d_data);
#endif DEBUG
				goto skipserver;
			}

		tmphtp = ((nsdp->d_flags & DB_F_HINT) ? fcachetab : hashtab);
		np = nlookup(dname, &tmphtp, &fname, 1);
		if (np == NULL || fname != dname) {
#ifdef DEBUG
			if (debug >= 3)
			    fprintf(ddt,"%s: not found %s %x\n",dname,fname,np);
#endif
			continue;
		}
		found_arr = 0;
		oldn = n;
		/* look for name server addresses */
		for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
			if (dp->d_type != T_A || dp->d_class != class
			    || (dp->d_flags & DB_F_NXRR))
				continue;
			/*
			 * Don't use records that may become invalid to
			 * reference later when we do the rtt computation.
			 * Never delete our safety-belt information!
			 */
			if ((dp->d_zone == 0) &&
			    (dp->d_ttl < (curtime+900)) &&
			    !(dp->d_flags & DB_F_HINT) )
		        {
#ifdef DEBUG
				if (debug >= 3)
					fprintf(ddt,"nslookup: stale entry '%s'\n",
					    np->n_dname);
#endif
				/* Cache invalidate the NS RR's */
				if (dp->d_ttl < curtime)
					delete_all(np, class, T_A);
				n = oldn;
				break;
			}

			found_arr++;
			/* don't put in duplicates */
			qs = qp->q_addr;
			for (i = 0; i < n; i++, qs++)
				if (bcmp((char *)&qs->ns_addr.sin_addr,
				    dp->d_data, sizeof(struct in_addr)) == 0)
					goto skipaddr;
			qs->ns_addr.sin_family = AF_INET;
			qs->ns_addr.sin_port = ns_port;
			qs->ns_addr.sin_addr = 
				    *(struct in_addr *)dp->d_data;
			qs->ns = nsdp;
			qs->nsdata = dp;
			qp->q_addr[n].nretry = 0;
			n++;
			if (n >= NSMAX)
				goto out;
	skipaddr:	;
		}
#ifdef DEBUG
		if (debug >= 3)
			fprintf(ddt,"nslookup: %d ns addrs\n", n);
#endif
		if (found_arr == 0 && qp->q_system == 0)
			(void) sysquery(dname, class, T_A);
skipserver:	;
	}
out:
#ifdef DEBUG
	if (debug >= 3)
		fprintf(ddt,"nslookup: %d ns addrs total\n", n);
#endif
	qp->q_naddr = n;
	if (n > 1)
		qsort((char *)qp->q_addr, n, sizeof(struct qserv), qcomp);
	return (n - naddr);
}

qcomp(qs1, qs2)
	struct qserv *qs1, *qs2;
{

	return (qs1->nsdata->d_nstime - qs2->nsdata->d_nstime);
}

/*
 * Arrange that forwarded query (qp) is retried after t seconds.
 */
schedretry(qp, t)
	struct qinfo *qp;
	time_t t;
{
	register struct qinfo *qp1, *qp2;

#ifdef DEBUG
	if (debug > 3) {
		fprintf(ddt,"schedretry(%#x, %dsec)\n", qp, t);
		if (qp->q_time)
		   fprintf(ddt,"WARNING: schedretry(%x,%d) q_time already %d\n", qp->q_time);
	}
#endif
	t += (u_long) tt.tv_sec;
	qp->q_time = t;

	if ((qp1 = retryqp) == NULL) {
		retryqp = qp;
		qp->q_next = NULL;
		return;
	}
	while ((qp2 = qp1->q_next) != NULL && qp2->q_time < t)
		qp1 = qp2;
	qp1->q_next = qp;
	qp->q_next = qp2;
}

/*
 * Unsched is called to remove a forwarded query entry.
 */
unsched(qp)
	struct qinfo *qp;
{
	register struct qinfo *np;

#ifdef DEBUG
	if (debug > 3) {
		fprintf(ddt,"unsched(%#x, %d )\n", qp, ntohs(qp->q_id));
	}
#endif
	if( retryqp == qp )  {
		retryqp = qp->q_next;
	} else if (retryqp != QINFO_NULL) {
		for( np=retryqp; np->q_next != QINFO_NULL; np = np->q_next ) {
			if( np->q_next != qp)
				continue;
			np->q_next = qp->q_next;	/* dequeue */
			break;
		}
	}
	qp->q_next = QINFO_NULL;		/* sanity check */
	qp->q_time = 0;
}


/*
 * nextserver - determine the next server to send a request to
 */
nextserver(qp)
	register struct qinfo *qp;
{
	register int n, nstart;

#ifdef DEBUG
	if (debug > 3)
		fprintf(ddt, "nextserver(0x%x) id=%d\n", (int)qp,
		    ntohs(qp->q_id));
#endif
	/*
	 * Try next forwarder, if any
	 */
	n = qp->q_curaddr;
	if (qp->q_fwd) {
		qp->q_fwd = qp->q_fwd->next;
		if (qp->q_fwd)
			return 1;
	} else {
		/*
		 * Last try was to server.  Bump his retry count.
		 */
		if (qp->q_addr[n].nretry < MAXRETRY)
			qp->q_addr[n].nretry++;
		if (++n >= qp->q_naddr)
			n = 0;
	}

	/*
	 * No forwarders (left).  If forwarding only, the jig's up.
	 */
	if (forward_only)
		return 0;
	
	/*
	 * Look for a server which hasn't timed out yet, starting
	 * at `n'.
	 */
	nstart = n;
	do {
		if (qp->q_addr[n].nretry < MAXRETRY) {
			qp->q_curaddr = n;
			return 1;
		}
		if (++n >= qp->q_naddr)
			n = 0;
	} while (n != nstart);

	/*
	 * No one left, return failure.
	 */
	return 0;
}


/*
 * giveup - we can't complete this query, so give it up.
 */
giveup(qp)
	register struct qinfo *qp;
{
	register int n;
	register HEADER *hp;

#ifdef DEBUG
	if (debug > 3)
		fprintf(ddt, "giveup(0x%x) id=%d\n", (int)qp, ntohs(qp->q_id));
#endif
	hp = (HEADER *)(qp->q_cmsg ? qp->q_cmsg : qp->q_msg);
	if (qp->q_system == PRIMING_CACHE) {
		/* Can't give up priming */
#ifdef NET_ERRS
		if (qp->q_errinfo)
			(void) getsocket(qp, (struct sockaddr_in *)0);
#endif /* NET_ERRS */
		unsched(qp);
		schedretry(qp, (time_t)60*60);	/* 1 hour */
		hp->rcode = NOERROR;	/* Lets be safe, reset the query */
		hp->qr = hp->aa = 0;
		qp->q_fwd = fwdtab;
		for (n = 0; n < qp->q_naddr; n++)
			qp->q_addr[n].nretry = 0;
		return;
	}

	n = ((HEADER *)qp->q_cmsg ? qp->q_cmsglen : qp->q_msglen);
	hp->id = qp->q_id;
	hp->qr = 1;
	hp->ra = 1;
	hp->rd = 1;
	hp->rcode = SERVFAIL;
#ifdef DEBUG
	if (debug >= 10)
		fp_query(qp->q_msg, qp->q_msglen, ddt);
#endif
	if (send_msg((char *)hp, n, qp)) {
#ifdef DEBUG
		if (debug)
			fprintf(ddt,"giveup(0x%x) nsid=%d id=%d\n",
				qp, ntohs(qp->q_nsid), ntohs(qp->q_id));
#endif
	}
	qremove(qp);
}


/*
 * sendquery - send a query to a server, retrying until we find one
 *	       we can actually send to.
 */
int
sendquery(qp)
	register struct qinfo *qp;
{
	register int n;
	register HEADER *hp;
	int sent = 0;
	int s;
	int res;

	hp = (HEADER *)qp->q_msg;

	/*
	 * Loop until we either send this successfully or give up
	 * on it.
	 */
	while (!sent) {
		n = qp->q_curaddr;
		if (qp->q_fwd == 0 && qp->q_addr[n].nretry == 0)
			qp->q_addr[n].stime = tt;
		hp->rd = (qp->q_fwd ? 1 : 0);

#ifdef DEBUG
		if (debug)
			fprintf(ddt,
			  "%s(addr=%d n=%d) -> %s %d (%d) nsid=%d id=%d %dms\n",
				(qp->q_fwd ? "forw" : "send"),
				n, qp->q_addr[n].nretry,
				inet_ntoa(Q_NEXTADDR(qp,n)->sin_addr),
				ds, ntohs(Q_NEXTADDR(qp,n)->sin_port),
				ntohs(qp->q_nsid), ntohs(qp->q_id),
				qp->q_addr[n].nsdata->d_nstime);
		if ( debug >= 10)
			fp_query(qp->q_msg, qp->q_msglen, ddt);
#endif

		/*
		 * Get a socket for the query.  If it isn't the
		 * default, use write(), else sendto().
		 */
		s = getsocket(qp, Q_NEXTADDR(qp,n));
		if (s == ds)
			res = sendto(ds, qp->q_msg, qp->q_msglen, 0,
			    (struct sockaddr *)Q_NEXTADDR(qp,n),
			    sizeof(struct sockaddr_in));
		else
			res = write(s, qp->q_msg, qp->q_msglen);

		if (res < 0) {
#ifdef	NET_ERRS
			if (ISNETERR(errno)) {
				/*
				 * Network error.  Mark this guy
				 * hopeless and go on to the next.
				 */
				if (qp->q_fwd == NULL)
					qp->q_addr[qp->q_curaddr].nretry
					    = MAXRETRY;
				if (!nextserver(qp)) {
					giveup(qp);
					return 0;
				}
			} else
#endif	/* NET_ERRS */
			{
#ifdef DEBUG
				if (debug > 3)
					fprintf(ddt,
					    "error sending msg errno=%d\n",
					    errno);
#endif
				/*
				 * Shouldn't happen.  Pretend it was sent
				 * anyway.
				 */
				sent = 1;
			}
		} else {
			sent = 1;
		}
	}
	hp->rd = 1;	/* leave set to 1 for dup detection */
#ifdef STATS
	stats[S_OUTPKTS].cnt++;
#endif
	unsched(qp);
	schedretry(qp, qp->q_fwd ? (2*RETRYBASE) : retrytime(qp));
	return 1;
}


/*
 * Retry is called to retransmit query 'qp'.
 */
retry(qp)
	register struct qinfo *qp;
{

#ifdef DEBUG
	if (debug > 3)
		fprintf(ddt,"retry(x%x) id=%d\n", qp, ntohs(qp->q_id));
#endif
	if((HEADER *)qp->q_msg == NULL) {		/*** XXX ***/
		qremove(qp);
		return;
	}						/*** XXX ***/

	/*
	 * Try the next server.  If none to try, give up.  Otherwise
	 * send it again.
	 */
	if (nextserver(qp))
		(void)sendquery(qp);
	else
		giveup(qp);
}

/*
 * Compute retry time for the next server for a query.
 * Use a minimum time of RETRYBASE (4 sec.) or twice the estimated
 * service time; * back off exponentially on retries, but place a 45-sec.
 * ceiling on retry times for now.  (This is because we don't hold a reference
 * on servers or their addresses, and we have to finish before they time out.)
 */
time_t
retrytime(qp)
register struct qinfo *qp;
{
	time_t t;
	struct qserv *ns = &qp->q_addr[qp->q_curaddr];

#ifdef DEBUG
	if (debug > 3)
		fprintf(ddt,"retrytime: nstime %dms.\n",
		    ns->nsdata->d_nstime / 1000);
#endif
	t = (time_t) MAX(RETRYBASE, 2 * ns->nsdata->d_nstime / 1000);
	t <<= ns->nretry;
	t = MIN(t, 45);			/* max. retry timeout for now */
#ifdef notdef
	if (qp->q_system)
		return ((2 * t) + 5);	/* system queries can wait. */
#endif
	return (t);
}

qflush()
{
	while (qhead)
		qremove(qhead);
	qhead = QINFO_NULL;
}

qremove(qp)
register struct qinfo *qp;
{
#ifdef DEBUG
	if(debug > 3)
		fprintf(ddt,"qremove(x%x)\n", qp);
#endif
#ifdef NET_ERRS
	if (qp->q_errinfo)
		(void) getsocket(qp, (struct sockaddr_in *)0);
#endif /* NET_ERRS */
	unsched(qp);			/* get off queue first */
	qfree(qp);
}

struct qinfo *
qfindid(id)
register u_short id;
{
	register struct qinfo *qp;

#ifdef DEBUG
	if(debug > 3)
		fprintf(ddt,"qfindid(%d)\n", ntohs(id));
#endif
	for (qp = qhead; qp!=QINFO_NULL; qp = qp->q_link) {
		if (qp->q_nsid == id)
			return(qp);
	}
#ifdef DEBUG
	if (debug >= 5)
		fprintf(ddt,"qp not found\n");
#endif
	return(NULL);
}

struct qinfo *
qnew()
{
	register struct qinfo *qp;

	if ((qp = (struct qinfo *)calloc(1, sizeof(struct qinfo))) == NULL) {
#ifdef DEBUG
		if (debug >= 5)
			fprintf(ddt,"qnew: calloc error\n");
#endif
		syslog(LOG_ERR, "forw: %m");
		exit(12);
	}
#ifdef DEBUG
	if (debug >= 5)
		fprintf(ddt,"qnew(x%x)\n", qp);
#endif
	qp->q_link = qhead;
	qhead = qp;
	return( qp );
}

qfree(qp)
struct qinfo *qp;
{
	register struct qinfo *np;

#ifdef DEBUG
	if(debug > 3)
		fprintf(ddt,"qfree( x%x )\n", qp);
	if(debug && qp->q_next)
		fprintf(ddt,"WARNING:  qfree of linked ptr x%x\n", qp);
#endif
	if (qp->q_msg)
	 	free(qp->q_msg);
 	if (qp->q_cmsg)
 		free(qp->q_cmsg);
	if( qhead == qp )  {
		qhead = qp->q_link;
	} else {
		for( np=qhead; np->q_link != QINFO_NULL; np = np->q_link )  {
			if( np->q_link != qp )  continue;
			np->q_link = qp->q_link;	/* dequeue */
			break;
		}
	}
	(void)free((char *)qp);
}

/*
 * isForwarder - is this guy one of our forwarders?
 */
int
isForwarder(addr)
	register struct sockaddr_in *addr;
{
	register struct fwdinfo *fwd;

	for (fwd = fwdtab; fwd != NULL; fwd = fwd->next)
		if (addr->sin_addr.s_addr == fwd->fwdaddr.sin_addr.s_addr)
			break;
	return (fwd != NULL);
}


/*
 * getsocket - return a socket to send a query on
 */
int
getsocket(qp, addr)
	register struct qinfo *qp;
	struct sockaddr_in *addr;
{
#ifndef NET_ERRS
	/*
	 * Trival.  Return the datagram socket.
	 */
#ifdef DEBUG
	if (debug >= 5)
		fprintf(ddt, "getsocket(x%x, %s)\n",
		    qp, (addr == 0) ? "noaddr" : inet_ntoa(addr->sin_addr));
#endif
	return ds;
#else
	register struct errinfo *ep;
	struct errinfo *epunused;
	extern struct errinfo *errlist;

#ifdef DEBUG
	if (debug >= 5)
		fprintf(ddt, "getsocket(x%x, %s)\n",
		    qp, (addr == 0) ? "noaddr" : inet_ntoa(addr->sin_addr));
#endif

	if (qp->q_errinfo != NULL) {
		/*
		 * Decrement the link count on the old socket.
		 */
		ep = qp->q_errinfo;
		if (ep->e_count == 0) {
#ifdef DEBUG
			if (debug)
				fprintf(ddt,
			   "WARNING: disconnecting unconnected error socket\n");
#endif
		} else {
			ep->e_count--;
		}
		qp->q_errinfo = NULL;
	}

	/*
	 * If no address, just return.  This was likely a call
	 * to release an existing socket.
	 */
	if (addr == (struct sockaddr_in *)0)
		return ds;

	/*
	 * Look for a socket which is connected to this guy already or,
	 * if not, at least a socket which is not currently linked.
	 */
	epunused = NULL;
	for (ep = errlist; ep != NULL; ep = ep->e_next) {
		if (addr->sin_addr.s_addr == ep->e_dest.sin_addr.s_addr)
			break;
		if (epunused == NULL) {
			epunused = ep;
		} else if (ep->e_traffic < epunused->e_traffic) {
			epunused = ep;
		}
	}

	if (ep == NULL && epunused == NULL) {
		/*
		 * Everything in use.  Return default
		 * socket.
		 */
#ifdef STATS
		stats[S_ALLCONNECTED].cnt++;
#endif
		return ds;
	}

	if (ep != NULL) {
		/*
		 * Got one which is already connected.  Increment the
		 * link count and return it.
		 */
		ep->e_count++;
		ep->e_traffic++;
#ifdef STATS
		stats[S_ALREADYCONN].cnt++;
#endif
	} else {
		/*
		 * This one isn't being used.  Connect it to the
		 * remote host and return it.
		 */
		ep = epunused;
		ep->e_dest = *addr;
		ep->e_count++;
		ep->e_traffic = 0;
		if (connect(ep->e_fd, (struct sockaddr *)addr,
		    sizeof(struct sockaddr_in)) != 0) {
#ifdef DEBUG
			if (debug)
				fprintf(ddt,
				    "Can't connect to %s: error number %d\n",
				    inet_ntoa(addr->sin_addr), errno);
#endif
			syslog(LOG_ERR, "can't connect to %s: %m",
			    inet_ntoa(addr->sin_addr));
			ep->e_dest.sin_addr.s_addr = 0;
			ep->e_count = 0;
			return ds;
		}
#ifdef STATS
		stats[S_CONNECTS].cnt++;
#endif
	}
	qp->q_errinfo = ep;
	return (ep->e_fd);
#endif /* NET_ERRS */
}

/*
 * qresperr - take action when an error response is received to this query
 */
qresperr(qp, from, serverfail)
	register struct qinfo *qp;
	struct sockaddr_in *from;
	int serverfail;
{
	register int n;

#ifdef DEBUG
	if (debug >= 3)
		fprintf(ddt, "qresperr(x%x, %s, %d)\n", qp,
		    inet_ntoa(from->sin_addr), serverfail);
#endif
	/*
	 * If the message wasn't a server failure or was
	 * from a name server, mark this guy as dead and
	 * try someone else.  If the message was a server
	 * failure and was from a forwarder, however, we
	 * assume we aren't going to be able to do any better
	 * than the forwarder and give up on the request immediately.
	 */
	if (isForwarder(from)) {
		if (serverfail) {
			giveup(qp);
			return;
		}
		/*
		 * If this forwarder isn't our current favourite,
		 * no need to do anything since we already timed
		 * him out.  If he is our current target we need
		 * to move on, however.
		 */
		if (qp->q_fwd == NULL ||
		    qp->q_fwd->fwdaddr.sin_addr.s_addr != from->sin_addr.s_addr)
			return;
	} else {
		/*
		 * See if we can find the corresponding server.  If
		 * so, mark him silly.  Otherwise this was a martian.
		 * If the loser is the current guy we're working on
		 * we'll have to move on.
		 */
		if (qp->q_fwd)
			return;
		for (n = 0; n < qp->q_naddr; n++) {
			if (qp->q_addr[n].ns_addr.sin_addr.s_addr
			    == from->sin_addr.s_addr)
				break;
		}
		if (n == qp->q_naddr)
			return;
		qp->q_addr[n].nretry = MAXRETRY;
		if (n != qp->q_curaddr)
			return;
	}

	/*
	 * Try another target.
	 */
	if (nextserver(qp))
		(void)sendquery(qp);
	else
		giveup(qp);
}


#ifdef	NET_ERRS
/*
 * neterror - retry anyone who is affected by a network error
 */
neterror(ep)
	register struct errinfo *ep;
{
	register struct qinfo *qp;
	register struct qinfo *qnext;

#ifdef DEBUG
	if (debug >= 3) {
		/*
		 * Use the illicit knowledge that no system calls have
		 * been made between the offending call and here.
		 */
		fprintf(ddt, "neterror(addr %s, err %d)\n",
		    inet_ntoa(ep->e_dest.sin_addr), errno);
	}
#endif
	if (ep->e_count == 0) {
		/*
		 * martian
		 */
#ifdef DEBUG
		if (debug)
			fprintf(ddt,"WARNING: neterror finds ep->count == 0\n");
#endif
		return;
	}
		
	/*
	 * Search through and find the guys who are affected.
	 */
	qp = qhead;
	while (qp != QINFO_NULL) {
		qnext = qp->q_link;		/* in case qp gets deleted */
		if (qp->q_errinfo == ep) {
			if (qp->q_fwd == NULL)
				qp->q_addr[qp->q_curaddr].nretry = MAXRETRY;
			if (nextserver(qp))
				(void)sendquery(qp);
			else
				giveup(qp);
		}
		qp = qnext;
	}
}
#endif	/* NET_ERRS */
