
/*
 * Copyright (c) 1985 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to the University of California at Berkeley. The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission. This software
 * is provided ``as is'' without express or implied warranty.
 */

/*
** Modified for and distributed with 'dig' version 1.0 from 
** University of Southern California Information Sciences Institute
** (USC-ISI). 3/27/89
*/


#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)res_debug.c	5.22 (Berkeley) 3/7/88";
#endif /* LIBC_SCCS and not lint */

#if defined(lint) && !defined(DEBUG)
#define DEBUG
#endif

#ifndef RES_DEBUG
#define RES_DEBUG2      0x80000000
#endif

#include "hfiles.h"

#include <sys/types.h>
#include <netinet/in.h>
#include <stdio.h>
#include NAMESERH
#ifndef T_TXT
#define T_TXT 16
#endif T_TXT
#include RESOLVH
#include "pflag.h"


extern char *p_cdname(), *p_rr(), *p_type(), *p_class();
extern char *inet_ntoa();

char *_res_opcodes[] = {
	"QUERY",
	"IQUERY",
	"CQUERYM",
	"CQUERYU",
	"4",
	"5",
	"6",
	"7",
	"8",
	"UPDATEA",
	"UPDATED",

	"UPDATEDA",
	"UPDATEM",
	"UPDATEMA",
	"ZONEINIT",
	"ZONEREF",
};

char *_res_resultcodes[] = {
	"NOERROR",
	"FORMERR",
	"SERVFAIL",
	"NXDOMAIN",
	"NOTIMP",
	"REFUSED",
	"6",
	"7",
	"8",
	"9",
	"10",
	"11",
	"12",
	"13",
	"14",
	"NOCHANGE",
};

p_query(msg)
	char *msg;
{
#ifdef DEBUG
	fp_query(msg,stdout);
#endif
}

char *
do_rrset(msg,cp,cnt,pflag,file,hs)
     int cnt, pflag;
     char *cp,*msg, *hs;
     FILE *file;
{
  int n;
  char *pp;
  char *t1, *t2, *list[100],**tt;
	/*
	 * Print  answer records
	 */
  if (n = ntohs(cnt)) {
    *list=NULL;
    if (pfcode & pflag)
      fprintf(file,hs);
    while (--n >= 0) {
      pp = (char *) malloc(512);
      *pp=0;
      cp = p_rr(cp, msg, pp);
      if (cp == NULL)
	return;

      if (pfcode & PRF_SORT) {
	for (tt=list, t1=pp, t2=NULL;
	     ((*tt != NULL) && (strcmp(*tt,t1) < 1));) {
	  tt++;
	}
	while (t1 != NULL) {
	  t2 = *tt;
	  *tt++ = t1;
	  t1 = t2;
	}
	*tt = t1;
      }
      else {
    if (pfcode & pflag) 
	fprintf(file,"%s",pp);
	free(pp);
      }
    }

    if (pfcode & pflag) {
      if (pfcode & PRF_SORT) {
	tt=list;
	while (*tt != NULL) {
	  fprintf(file,"%s",*tt);
	  free(*tt++);
	}
      }
      fprintf(file,"\n");
    }
  }
  return(cp);
}


/*
 * Print the contents of a query.
 * This is intended to be primarily a debugging routine.
 */
fp_query(msg,file)
	char *msg;
	FILE *file;
{
	register char *cp;
	register HEADER *hp;
	register int n;
	char *pp;
	char zfile[256], tfile[100];

	/*
	 * Print header fields.
	 */
	hp = (HEADER *)msg;
	cp = msg + sizeof(HEADER);
	if (pfcode & PRF_HEADX) {
	  fprintf(file,";; ->>HEADER<<- ");
	  fprintf(file,"opcode: %s ", _res_opcodes[hp->opcode]);
	  fprintf(file,", status: %s", _res_resultcodes[hp->rcode]);
	}
	if (pfcode & PRF_TTLID)
	  fprintf(file,", id: %d", ntohs(hp->id));
	if (pfcode & PRF_HEADX)
	  putc('\n',file);
	if (pfcode & PRF_HEAD2) {
	  fprintf(file,";; flags:");
	  if (hp->qr)
	    fprintf(file," qr");
	  if (hp->aa)
	    fprintf(file," aa");
	  if (hp->tc)
	    fprintf(file," tc");
	  if (hp->rd)
	    fprintf(file," rd");
	  if (hp->ra)
	    fprintf(file," ra");
	  if (hp->pr)
	    fprintf(file," pr");
	  if (_res.options & RES_DEBUG2)
	    fprintf(file," res_opts: %x", _res.options);
	  fprintf(file,"  Ques: %d, ", ntohs(hp->qdcount));
	  fprintf(file,"Ans: %d, ", ntohs(hp->ancount));
	  fprintf(file,"Auth: %d, ", ntohs(hp->nscount));
	  fprintf(file,"Addit: %d\n", ntohs(hp->arcount));
	}
	putc('\n',file);
	/*
	 * Print question records.
	 */
	if (n = ntohs(hp->qdcount)) {
	  if (pfcode & PRF_QUES)
	    fprintf(file,";; QUESTIONS: \n");
	  while (--n >= 0) {
	    sprintf(zfile,";;\t");
	    cp = p_cdname(cp, msg, zfile);
	    if (cp == NULL)
	      return;
	    sprintf(tfile,", type = %s", p_type(_getshort(cp)));
	    strcat(zfile,tfile);
	    cp += sizeof(u_short);
	    sprintf(tfile,", class = %s\n", p_class(_getshort(cp)));
	    strcat(zfile,tfile);
	    cp += sizeof(u_short);
	    if (pfcode & PRF_QUES)
	      fprintf(file,"%s",zfile);
	  }
	  if (pfcode & PRF_QUES)
	      fprintf(file,"\n");
	}

	/*
	 * Print authoritative answer records
	 */

cp = do_rrset(msg,cp,hp->ancount,PRF_ANS,file,";; ANSWERS:\n");

	/*
	 * print name server records
	 */

cp = do_rrset(msg,cp,hp->nscount,PRF_AUTH,file,";; AUTHORITY RECORDS:\n");

	/*
	 * print additional records
	 */

cp = do_rrset(msg,cp,hp->arcount,PRF_ADD,file,";; ADDITIONAL RECORDS:\n");

} /* end function */

      

char *
p_cdname(cp, msg, p)
	char *cp, *msg;
        char *p;
{
#ifdef DEBUG
	char name[MAXDNAME];
	int n;

	if ((n = dn_expand(msg, msg + 512, cp, name, sizeof(name))) < 0)
		return (NULL);
	if (name[0] == '\0') {
		name[0] = '.';
		name[1] = '\0';
	}
	strcat(p,name);
	return (cp + n);
#endif
}



/********************************************************
 * Print resource record fields in human readable form.
 ********************************************************/
char *
p_rr(cp, msg,p)
	char *cp, *msg, *p;
{
#ifdef DEBUG
	int type, class, dlen, n, c, xx;
	struct in_addr inaddr;
	char *cp1;
	unsigned long tmpttl;
	char file[100];

	if ((cp = p_cdname(cp, msg, p)) == NULL)
		return (NULL);			/* compression error */
	type = _getshort(cp);
	cp += sizeof(u_short);
	sprintf(file,"\t%s",p_class(class = _getshort(cp)));
	strcat(p, file);
	cp += sizeof(u_short);
	sprintf(file,"\t%s", p_type(type));
	strcat(p, file);
	tmpttl = _getlong(cp);
	cp += sizeof(u_long);
	dlen = _getshort(cp);
	cp += sizeof(u_short);
	cp1 = cp;
	/*
	 * Print type specific data, if appropriate
	 */
	switch (type) {
	case T_A:
		switch (class) {
		case C_IN:
			bcopy(cp, (char *)&inaddr, sizeof(inaddr));
			if (dlen == 4) {
			        sprintf(file,"\t%s",inet_ntoa(inaddr));
				strcat(p,file);
				cp += dlen;
			} else if (dlen == 7) {
				strcat(p,inet_ntoa(inaddr));
				sprintf(file,";; proto: %d", cp[4]);
				strcat(p,file);
				sprintf(file,", port: %d",
					(cp[5] << 8) + cp[6]);
				strcat(p,file);
				cp += dlen;
			}
			break;
		default:
			cp += dlen;
		}
		break;
	case T_CNAME:
	case T_MB:
#ifdef OLDRR
	case T_MD:
	case T_MF:
#endif /* OLDRR */
	case T_MG:
	case T_MR:
	case T_NS:
	case T_PTR:
		strcat(p,"\t");
		cp = p_cdname(cp, msg, p);
		break;

	case T_HINFO:
		if (n = *cp++) {
			sprintf(file,"\t%.*s ", n, cp);
			strcat(p,file);
			cp += n;
		}
		if (n = *cp++) {
			sprintf(file,"%.*s", n, cp);
			strcat(p,file);
			cp += n;
		}
		break;

	case T_UINFO:
		sprintf(file,"\t%s", cp);
		strcat(p,file);
		cp += dlen;
		break;

	case T_TXT:
		xx=0;
		while (xx++<dlen) {
		  if (n = *cp++) {
		    sprintf(file,"\t%.*s",n,cp);
		    strcat(p,file);
		    cp += (n-1);
		    xx += n;
		    if (*cp++ != '\n')
		      strcat(p,"\n");
		  }
		}
		strcat(p,"\n");
		break;

	case T_SOA:
		strcat(p,"\t");
		cp = p_cdname(cp, msg, p);
		strcat(p,"  ");
		cp = p_cdname(cp, msg, p);
		sprintf(file," (\n\t\t\t%ld\t;serial\n", _getlong(cp));
		strcat(p,file);
		cp += sizeof(u_long);
		sprintf(file,"\t\t\t%ld\t;refresh\n", _getlong(cp));
		strcat(p,file);
		cp += sizeof(u_long);
		sprintf(file,"\t\t\t%ld\t;retry\n", _getlong(cp));
		strcat(p,file);
		cp += sizeof(u_long);
		sprintf(file,"\t\t\t%ld\t;expire\n", _getlong(cp));
		strcat(p,file);
		cp += sizeof(u_long);
		sprintf(file,"\t\t\t%ld\t;minim )\n", _getlong(cp));
		strcat(p,file);
		cp += sizeof(u_long);
		break;

	case T_MX:
		sprintf(file,"\t%ld ",_getshort(cp));
		strcat(p,file);
		cp += sizeof(u_short);
		cp = p_cdname(cp, msg, p);
		break;

	case T_MINFO:
		strcat(p,"\t");
		cp = p_cdname(cp, msg, p);
		strcat(p," ");
		cp = p_cdname(cp, msg, p);
		break;


	case T_UID:
	case T_GID:
		if (dlen == 4) {
			sprintf(file,"\t%ld", _getlong(cp));
			strcat(p,file);
			cp += sizeof(int);
		}
		break;

	case T_WKS:
		if (dlen < sizeof(u_long) + 1)
			break;
		bcopy(cp, (char *)&inaddr, sizeof(inaddr));
		cp += sizeof(u_long);
		sprintf(file,"\t%s %d (\n",
			inet_ntoa(inaddr), *cp++);
		strcat(p,file);
		n = 0;
		strcat(p,"\t\t\t");
		while (cp < cp1 + dlen) {
			c = *cp++;
			do {
 				if (c & 0200) {
				  sprintf(file," %d", n);
				  strcat(p,file);
				      }
 				c <<= 1;
			} while (++n & 07);
		      }
		strcat(p," )\n\t\t");
		break;

#ifdef ALLOW_T_UNSPEC
	case T_UNSPEC:
		{
			int NumBytes = 8;
			char *DataPtr;
			int i;

			if (dlen < NumBytes) NumBytes = dlen;
			sprintf(file, "\tFirst %d bytes of hex data:",
				NumBytes);
			strcat(p,file);
		for (i = 0, DataPtr = cp; i < NumBytes; i++, DataPtr++) {
		  sprintf(file, " %x", *DataPtr);
		  strcat(p,file);
		}
			strcat(p,"\n");
			cp += dlen;
		}
		break;
#endif /* ALLOW_T_UNSPEC */

	default:
		strcat(p,"\t???\n");
		cp += dlen;
	}

	if (pfcode & PRF_TTLID) {
	  sprintf(file,"\t; %d",tmpttl);
	  strcat(p,file);
	}
	strcat(p,"\n");

	if (cp != cp1 + dlen)
	   fprintf(stdout,";;packet size error (%#x != %#x)\n", cp, cp1+dlen);
	return (cp);
#endif
}

static	char nbuf[20];

/*
 * Return a string for the type
 */
char *
p_type(type)
	int type;
{
	switch (type) {
	case T_A:
		return("A");
	case T_NS:		/* authoritative server */
		return("NS");
#ifdef OLDRR
	case T_MD:		/* mail destination */
		return("MD");
	case T_MF:		/* mail forwarder */
		return("MF");
#endif /* OLDRR */
	case T_CNAME:		/* connonical name */
		return("CNAME");
	case T_SOA:		/* start of authority zone */
		return("SOA");
	case T_MB:		/* mailbox domain name */
		return("MB");
	case T_MG:		/* mail group member */
		return("MG");
	case T_MX:		/* mail routing info */
		return("MX");
	case T_MR:		/* mail rename name */
		return("MR");
	case T_NULL:		/* null resource record */
		return("NULL");
	case T_WKS:		/* well known service */
		return("WKS");
	case T_PTR:		/* domain name pointer */
		return("PTR");
	case T_HINFO:		/* host information */
		return("HINFO");
	case T_MINFO:		/* mailbox information */
		return("MINFO");
	case T_AXFR:		/* zone transfer */
		return("AXFR");
	case T_MAILB:		/* mail box */
		return("MAILB");
	case T_MAILA:		/* mail address */
		return("MAILA");
	case T_ANY:		/* matches any type */
		return("ANY");
        case T_TXT:
		return("TXT");
	case T_UINFO:
		return("UINFO");
	case T_UID:
		return("UID");
	case T_GID:
		return("GID");
#ifdef ALLOW_T_UNSPEC
	case T_UNSPEC:
		return("UNSPEC");
#endif /* ALLOW_T_UNSPEC */
	default:
		(void)sprintf(nbuf, "%d", type);
		return(nbuf);
	}
}

/*
 * Return a mnemonic for class
 */
char *
p_class(class)
	int class;
{

	switch (class) {
	case C_IN:		/* internet class */
		return("IN");
	case C_ANY:		/* matches any class */
		return("ANY");
	default:
		(void)sprintf(nbuf, "%d", class);
		return(nbuf);
	}
}
