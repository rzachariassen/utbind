

 /*******************************************************************
 **      DiG -- Domain Information Groper                          **
 **                                                                **
 **        dig.c - Version 1.0                                     **
 **        Developed by: Steve Hotz & Paul Mockapetris             **
 **        USC Information Sciences Institute (USC-ISI)            **
 **        Marina del Rey, California                              **
 **        1989                                                    **
 **                                                                **
 **     DiG is Public Domain, and may be used for any purpose as   **
 **     long as this notice is not removed.                        **
 *******************************************************************/

#include "hfiles.h"

#include <sys/types.h>
#include <netinet/in.h>
#include <stdio.h>
#include RESOLVH
#include <sys/file.h>
#include <sys/stat.h>
#include <ctype.h> 
#include "subr.c"
#include "strcasecmp.c"
#include "pflag.h"
#include "options.c"

#ifdef RESLOCAL
#include "qtime.c"
#else
#include "qtime.h"
#endif

int pfcode = PRF_DEF;
extern char *inet_ntoa();
extern struct state _res;

FILE  *qfp;

#define MAXDATA		256   
#define SAVEENV "DiG.env"

char defsrv[]="default";


 /*
 ** Take arguments appearing in simple string (from file)
 ** place in char**.
 */
stackarg(y,l)
     char *l;
     char **y;
{
  int done=0;
  while (!done) {
    switch (*l) {
    case '\t':
    case ' ':  l++;    break;
    case NULL:
    case '\n': done++; *y = NULL; break;
    default:   *y++=l;  while (!isspace(*l)) l++;
      if (*l == '\n') done++; *l++ = NULL; *y = NULL;
}}}


main(argc, argv)
     int argc;
     char **argv;
{
	struct hostent *hp;
	short port = htons(NAMESERVER_PORT);
	char packet[PACKETSZ];
	char answer[PACKETSZ];
	int n;
	char doping[90];
	char pingstr[50];
	char *afile;
	unsigned long tmpaddr;

	struct timeval exectime, tv1,tv2,tv3;
	char curhost[30];

	char *srv;
	int anyflag = 0;
	int sticky = 0;
	int tmp; 
	int qtype = 1, qclass = 1;
	int addrflag = 0;

	char cmd[256];
	char domain[128];
	char **vtmp;
	char *args[30];
	char **ax;
	char **ay;
	int once = 1, dofile=0; /* batch -vs- interactive control */
	char fileq[100];
	char *qptr;
	int  fp;
	int wait=0;
	int envset=0, envsave=0;
	int Xpfcode, Tpfcode, Toptions;
	char *Xres, *Tres;


	res_init();
	_res.options |= RES_DEFAULT;
	Xres = (char *) malloc(sizeof(_res)+1);
	Tres = (char *) malloc(sizeof(_res)+1);

 /*
 ** If LOCALDEF in environment, should point to file
 ** containing local favourite defaults.
 */
	if (((afile= (char *) getenv("LOCALDEF")) != (char *) NULL) &&
	    ((fp=open(afile,O_RDONLY)) > 0)) {
	  read(fp,Xres,sizeof(struct state));
	  read(fp,&Xpfcode,sizeof(int));
	  close(fp);
	  bcopy(Xres,&_res,sizeof(struct state));
	  pfcode = Xpfcode;
	}
 /*
 **   check for batch-mode DiG 
 */
	vtmp = argv; ax=args;
	while (*vtmp != NULL) {
	  if (strcmp(*vtmp,"-f",2) == 0) {
	    dofile++; once=0;
	    if ((qfp = fopen(*++vtmp,"r")) == NULL) {
	      fflush(stdout);
	      perror("file open");
	      fflush(stderr);
	      exit(1);
	    }
	  } else {
	    *ax++ = *vtmp;
	  }
	  vtmp++;
	}

	_res.id = 1;
	gettimeofday(&tv1,NULL);

 /*
 **  Main section: once if cmd-line query
 **                while !EOF if batch mode
 */
	*fileq=NULL;
	while ((dofile && (fgets(fileq,100,qfp) !=NULL)) || 
	       ((!dofile) && (once--))) 
	  {
	    if ((*fileq=='\n') || (*fileq=='#') || (*fileq==';'))
	      continue; /* ignore blank lines & comments */

/*
 * "sticky" requests that before current parsing args
 * return to current "working" environment (X******)
 */
	    if (sticky) {
	      bcopy(Xres,&_res,sizeof(struct state));
	      pfcode = Xpfcode;
	    }

/* concat cmd-line and file args */
	    ay=ax;
	    qptr=fileq;
	    stackarg(ay,qptr);

	    /* defaults */
	    qtype=qclass=1;
	    *pingstr=0;
	    srv=NULL;

	    sprintf(cmd,"\n; <<>> DiG <<>> ");
	    argv = args;
/*
 * More cmd-line options than anyone should ever have to
 * deal with ....
 */
	    while (*(++argv) != NULL) { 
	      strcat(cmd,*argv); strcat(cmd," ");
	      if (**argv == '@') {
		srv = (*argv+1);
		continue;
	      }
	      if (**argv == '%')
		continue;
	      if (**argv == '+') {
		SetOption(*argv+1);
		continue;
	      }
	 
	      if (strcmp(*argv,"-nost",5) == 0) {
		sticky=0;; continue;
	      } else if (strcmp(*argv,"-st",2) == 0) {
		sticky++; continue;
	      } else if (strcmp(*argv,"-envsa",5) == 0) {
		envsave++; continue;
	      } else if (strcmp(*argv,"-envse",5) == 0) {
		envset++; continue;
	      }

         if (**argv == '-') {
	   switch (argv[0][1]) { 
	   case 'T': wait = atoi(*++vtmp); break;
	   case 'c': 
	     if ((tmp = atoi(*++argv)) || *argv[0]=='0') {
	       qclass = tmp;
	     } else if (tmp = StringToClass(*argv,0)) {
	       qclass = tmp;
	     } else {
	       printf("; invalid class specified\n");
	     }
	     break;
	   case 't': 
	     if ((tmp = atoi(*++argv)) || *argv[0]=='0') {
	       qtype = tmp;
	     } else if (tmp = StringToClass(*argv,0)) {
	       qtype = tmp;
	     } else {
	       printf("; invalid type specified\n");
	     }
	     break;
	   case 'p': port = htons(atoi(*++argv)); break;
	   case 'P':
	     if (argv[0][2] != NULL)
	       strcpy(pingstr,&argv[0][2]);
	     else
	       strcpy(pingstr,"ping -s");
	     break;
	   } /* switch - */
	   continue;
	 } /* if '-'   */

	      if ((tmp = StringToType(*argv,-1)) != -1) { 
		if ((T_ANY == tmp) && anyflag++) { 	
		  qclass = C_ANY; 	
		  continue; 
		} 
		qtype = tmp; 
	      }
	      else if ((tmp = StringToClass(*argv,-1)) != -1) { 
		qclass = tmp; 
	      }	 else {
		bzero(domain,128);
		sprintf(domain,"%s",*argv);
	      }
	    } /* while argv remains */
if (pfcode & 0x80000)
  printf("; pflag: %x res: %x\n", pfcode, _res.options);
	  
/*
 * Current env. (after this parse) is to become the
 * new "working environmnet. Used in conj. with sticky.
 */
	    if (envset) {
	      bcopy(&_res,Xres,sizeof(struct state));
	      Xpfcode=pfcode;
	      envset=0;
	    }

/*
 * Current env. (after this parse) is to become the
 * new default saved environmnet. Save in user specified
 * file if exists else is SAVEENV (== "DiG.env").
 */
	    if (envsave) {
	      if ((((afile= (char *) getenv("LOCALDEF")) != (char *) NULL) &&
		   ((fp=open(afile,O_WRONLY|O_CREAT|O_TRUNC,
			     S_IREAD|S_IWRITE)) > 0)) ||
		  ((fp = open(afile,SAVEENV,O_WRONLY|O_CREAT|O_TRUNC,
			       S_IREAD|S_IWRITE)) > 0)) {
		write(fp,&_res,sizeof(struct state));
		write(fp,&pfcode,sizeof(int));
		close(fp);
	      }
	      envsave=0;
	    }

	    if (pfcode & PRF_CMD)
	      printf("%s\n",cmd);

      addrflag=anyflag=0;

/*
 * Find address of server to query. If not dot-notation, then
 * try to resolve domain-name (if so, save and turn off print 
 * options, this domain-query is not the one we want. Restore
 * user options when done.
 * Things get a bit wierd since we need to use resolver to be
 * able to "put the resolver to work".
 */

   if (srv != NULL) {
     tmpaddr = inet_addr(srv);
     if (tmpaddr != (unsigned)-1) {
       _res.nscount = 1;      
       _res.nsaddr.sin_addr.s_addr = tmpaddr;
     } else {
      Tpfcode=pfcode;
      pfcode=0;
      bcopy(&_res,Tres,sizeof(_res));
      Toptions = _res.options;
      _res.options = RES_DEFAULT;
      res_init();
      hp = gethostbyname(srv);
      pfcode=Tpfcode;
      if (hp == NULL) {
	fflush(stdout);
	perror("Bad server: using default nameserver and timer opts.\n");
	fflush(stderr);
	_res.options = Toptions;
      } else {
	bcopy(Tres,&_res,sizeof(_res));
	bcopy(hp->h_addr, &_res.nsaddr_list[0].sin_addr, hp->h_length);
	_res.nscount = 1;
      }
    }
     _res.id += _res.retry;
   }


       _res.id += _res.retry;
       _res.options |= RES_DEBUG;
       _res.nsaddr.sin_family = AF_INET;
       _res.nsaddr.sin_port = port;

       n = res_mkquery(QUERY, domain, qclass, qtype, (char *)0, 0,
		       NULL, packet, sizeof(packet));
       if (n < 0) {
	 fflush(stderr);
	 printf(";; res_mkquery: buffer too small\n\n");
	 exit(-1);
       }

       if ((n = res_send(packet, n, answer, sizeof(answer))) < 0) {
	 fflush(stdout);
	 perror(";; res_send");
	 fflush(stderr);

#ifndef RESLOCAL
/*
 * resolver does not currently calculate elapsed time
 * if there is an error in res_send()
 */
/*
	 if (pfcode & PRF_STATS) {
	   printf(";; Error time: "); prnttime(&hqtime); putchar('\n');
	 }
*/
#endif
       }
#ifndef RESLOCAL
       else {
	 if (pfcode & PRF_STATS) {
	   printf(";; Sent %d pkts, answer found in time: ",hqcount);
	   prnttime(&hqtime);
	   if (hxcount)
	     printf(" sent %d too many pkts",hxcount);
	   putchar('\n');
	 }
       }
#endif

       if (pfcode & PRF_STATS) {
	 gettimeofday(&exectime,NULL);
	 gethostname(curhost,30);
	 printf(";; FROM: %s to SERVER: %s\n",curhost,(srv==NULL)?defsrv:srv);
	 printf(";; WHEN: %s",ctime(&(exectime.tv_sec)));
       }
	  
       fflush(stdout);
/*
 *   Argh ... not particularly elegant.
 *   Should put in *real* ping code.
 */
       if (*pingstr) {
	 sprintf(doping,"%s %s 56 3 | tail -3",pingstr,(srv==NULL)?defsrv:srv);
	 system(doping);
       }
       putchar('\n');

/*
 * Fairly crude method and low overhead method of keeping two
 * batches started at different sites somewhat synchronized.
 */
       gettimeofday(&tv2);
       tv1.tv_sec += wait;
       difftime(&tv1,&tv2,&tv3);
       if (tv3.tv_sec > 0)
	 sleep(tv3.tv_sec);
  }
}

