/*
 * nsmaint - allow privileged souls to diddle the name server
 *
 * by Dennis Ferguson.
 */

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/dir.h>
#include <pwd.h>
#include <grp.h>
#include <sys/signal.h>
#include <syslog.h>
#include <errno.h>
#include <pathnames.h>

#define	STREQ(a, b)	(*(a) == *(b) && strcmp((a), (b)) == 0)

/*
 * People who are allowed to play name server god should be
 * in this group.
 */
#define	NSGROUP		"nsmaint"

/*
 * Program name we must be run with.  Want to make sure run attempts
 * end up in the accounting file in recognizable form.
 */
#define	THISPROGNAME	"nsmaint"

/*
 * When cleaning the data directory, we make sure these mode bits
 * are set on files and directories.
 */
#define	FILEMODETOSET	0664

/*
 * Path names to files we need to know about
 */

/* the nameserver pid file: /etc/named.pid */
#ifndef	PIDFILE
#define	PIDFILE	_PATH_PIDFILE
#endif

/* the nameserver program: /etc/named */
#ifndef	NAMED
#define	NAMED	_PATH_NAMED
#endif

/* the nameserver data directory: /etc/nsdata/. */
#ifndef	DATADIR
#define	DATADIR	_PATH_DATADIR
#endif

/*
 * Umask to run named with
 */
#define	NSUMASK	002

/*
 * Sanity.  We don't believe named will ever run with a pid less than this.
 */
#define	MINNSPID	3

/*
 * Phony environment so named is run relatively clean
 */
char *namedenv[] = {
	"HOME=/",
	"SHELL=/bin/sh",
	"TERM=dumb",
	"USER=root",
	NULL
};

/*
 * Hack to detect 'start' command.  This must be dealt with differently
 * than the others.
 */

typedef enum {	Error,
		Hup = SIGHUP,		/* sorry... */
		Term = SIGTERM,
		Int = SIGINT,
		Stats = SIGIOT,
		Usr1 = SIGUSR1,
		Usr2 = SIGUSR2,
		Start, Clean, ForceStart } Action;

/*
 * Command table.
 */
struct nscommands {
	char *cmdname;
	Action cmdaction;
} nscmds[] = {
	{ "sync",	Hup	},
	{ "kill",	Term	},
	{ "start",	Start	},
	{ "stop",	Term	},
	{ "dump",	Int	},
	{ "debuginc",	Usr1	},
	{ "debugoff",	Usr2	},
	{ "clean",	Clean	},
	{ "START",	ForceStart },
	{ "stats",	Stats	},
	{ NULL,		Error	}
};

char *thisprogname = THISPROGNAME;
char *nsprog = NAMED;
int userokay = 0;
char *username;		/* name of user running program */
int nsmaintgrp;		/* gid of the nsmaint group */

char *progname;
int debug = 0;

extern int errno;

/*
 * main - parse arguments and handle options
 */
main(argc, argv)
int argc;
char *argv[];
{
	int c;
	int errflg = 0;
	Action docmd;
	int namedpid;
	extern int optind, errno;
	extern char *optarg;
	extern Action getcmd();

	progname = argv[0];
	while ((c = getopt(argc, argv, "d")) != EOF)
		switch (c) {
		case 'd':
			++debug;
			break;
		default:
			errflg++;
			break;
		}
	if (!errflg &&
	    (optind != argc-1 || (docmd = getcmd(argv[optind])) == Error))
		errflg++;
	if (errflg) {
		setlinebuf(stderr);
		(void) fprintf(stderr,
		    "Usage: %s [-d] {%s", thisprogname, nscmds[0].cmdname);
		for (c = 1; nscmds[c].cmdname != NULL; ++c)
			(void) fprintf(stderr, "|%s", nscmds[c].cmdname);
		(void) fprintf(stderr, "}\n");
		exit(2);
	}

#ifdef	LOG_AUTH
	openlog(thisprogname, LOG_NOWAIT, LOG_AUTH);
#else
#ifdef	LOG_NOWAIT
	openlog(thisprogname, LOG_NOWAIT);
#else
	openlog(thisprogname, 0);
#endif
#endif
	userokay = checkoutuser();
	if (!userokay)
		exit(0);	/* ignore plebes */
	checkprogname(progname);
	namedpid = getnamedpid();

	switch (docmd) {
	case Start:
	case ForceStart:
		/*
		 * If the command is not forcestart, make sure
		 * named not running.
		 */
		if (docmd == Start && namedpid >= MINNSPID
		    && isrunning(namedpid)) {
			(void) fprintf(stderr,
			    "%s: %s (pid %d) appears to be running already\n",
			    thisprogname, nsprog, namedpid);
			exit(1);
		}
		if (debug)
			(void) printf("exec'ing %s\n", nsprog);
		else {
			(void) umask(NSUMASK);
			execle(nsprog, nsprog, (char *)NULL, namedenv);
			/* whoops! */
			Perror("can't exec %s", nsprog, "");
			exit(1);
		}
		break;
	case Clean:
		(void) umask(NSUMASK);
		if (chfile(DATADIR, 0, nsmaintgrp, FILEMODETOSET))
			chdirectory(DATADIR, 0, nsmaintgrp, FILEMODETOSET);
		break;
	case Term:
	case Hup:
	case Int:
	case Stats:
	case Usr1:
	case Usr2:
		/*
		 * If we're here, we have a signal to deliver.
		 */
		if (namedpid < MINNSPID) {
			(void) fprintf(stderr,
				       "%s: %s is either missing or contains an unlikely value\n",
				       thisprogname, PIDFILE);
			exit(1);
		}
		if (!isrunning(namedpid)) {
			(void) fprintf(stderr,
				       "%s: %s (pid %d) isn't running\n",
				       thisprogname, nsprog, namedpid);
			exit(1);
		}
		if (debug)
			(void) printf("sending signal %d to pid %d\n",
					       (int)docmd, namedpid);
		else if (kill(namedpid, (int)docmd) != 0) {
			Perror("failed to deliver signal to %s (pid %d)",
			    nsprog, (char *)namedpid);
			exit(1);
		}
		break;
	}
	exit(0);
}


/*
 * getsig - return the signal corresponding to this command
 */
Action
getcmd(cmd)
	char *cmd;
{
	register struct nscommands *nsc;

	for (nsc = nscmds; nsc->cmdname != NULL; nsc++)
		if (nsc->cmdname == NULL || STREQ(cmd, nsc->cmdname))
			return nsc->cmdaction;
	return Error;
}


/*
 * checkprogname - check the program name we were run with
 */
checkprogname(name)
	char *name;
{
	register char *cp;
	int nlen, thislen;
	int err = 1;

	/*
	 * The last element in the path in argv[0] is compared against
	 * the required program name.  This check is grossly inadequate,
	 * but prevents the easiest method of running the program invisibly
	 * to the accounting (with the shell).
	 */
	nlen = strlen(name);
	thislen = strlen(thisprogname);

	if (nlen == thislen) {
		cp = name;
		err = 0;
	} else if (nlen > thislen) {
		cp = &name[nlen-thislen];
		if (*(cp-1) == '/')
			err = 0;
	}

	if (!err && STREQ(cp, thisprogname))
		return;
	
	(void) fprintf(stderr, "%s: must run progam with name of %s\n",
	    thisprogname, thisprogname);
	/* Complain loudly, at least until holes are fixed */
	syslog(LOG_ALERT, "user %s attempted to run %s with name %s",
	    username, thisprogname, name);
	exit(1);
}


/*
 * checkoutuser - see if this user is one of the privileged few
 */
int
checkoutuser(docmd)
	Action docmd;
{
	register int i;
	struct passwd *pw;
	struct group *gr;
	int uid;
	int ngroups, gidset[NGROUPS];

	uid = getuid();
	if ((gr = getgrnam(NSGROUP)) == NULL) {
		/*
		 * Super gross hack.  A side effect of this routine
		 * is to set nsmaintgrp to the proper gid.  If root
		 * is running this, however, we don't necessarily
		 * want to prevent him from doing things even if the
		 * group doesn't exist, except that he can't do a clean
		 * command if we don't know the group.  Sigh.
		 */
		if (uid != 0 || docmd == Clean) {
			(void) fprintf(stderr, "%s: group %s doesn't exist\n",
			    thisprogname, NSGROUP);
			exit(1);
		}
		nsmaintgrp = -1;
	} else
		nsmaintgrp = gr->gr_gid;

	if (uid == 0) {
		username = "root";
		/*
		 * Let root do what he wants.
		 */
		return 1;
	}

	if ((pw = getpwuid(uid)) == NULL) {
		(void) fprintf(stderr, "%s: username for your uid (%d) unknown",
		    thisprogname, uid);
		syslog(LOG_ALERT, "unknown user with uid %d tried to run %s",
		    uid, thisprogname);
		exit(1);
	}
	username = pw->pw_name;

	if (nsmaintgrp == getgid())
		goto okay;	/* okay */
	
	if ((ngroups = getgroups(NGROUPS, gidset)) == -1) {
		(void) fprintf(stderr, "%s: getgroups() failed: ",
		    thisprogname);
		perror("");
	}

	for (i = 0; i < ngroups; i++)
		if (nsmaintgrp == gidset[i])
			goto okay;
	(void)setuid(uid);
	syslog(LOG_ALERT, "user %s attempted to run %s", username,
	    thisprogname);
	return 0;

okay:
	(void)setreuid(0, 0);
	return 1;
}


/*
 * getnamedpid - get named's pid from disk file
 */
int
getnamedpid()
{
	register char *cp;
	int pid;
	char buf[10];
	FILE *fp;

	if ((fp = fopen(PIDFILE, "r")) == NULL)
		return 0;
	buf[0] = '\0';
	(void) fgets(buf, sizeof buf, fp);
	(void) fclose(fp);

	for (cp = buf; *cp != '\0' && *cp != '\n'; cp++)
		if (!isdigit(*cp))
			return 0;
	if (*cp != '\0')
		*cp = '\0';
	if (buf[0] == '\0')
		return 0;
	
	pid = atoi(buf);
	if (pid < MINNSPID || pid > 32767)
		return 0;
	return pid;
}


/*
 * isrunning - see if program with this pid is running
 */
int
isrunning(pid)
	int pid;
{
	if (kill(pid, 0) == -1) {
		/*
		 * ESRCH means process not running.  Otherwise, user
		 * was unprivileged but program is going.
		 */
		if (errno == ESRCH)
			return 0;
	}
	return 1;
}


/*
 * chdirectory - (recursively) change owner, group and perm bits on files
 *		 in a directory.
 */
chdirectory(dir, uid, gid, modebits)
	char *dir;
	int uid, gid, modebits;
{
	register DIR *dirp;
	register struct direct *dp;
	char savedir[1024];

	if (getwd(savedir) == 0) {
		(void) fprintf(stderr, "%s: can't get working directory: %s\n",
		    thisprogname, savedir);
		exit(1);
	}

	if (chdir(dir) == -1) {
		Perror("can't change directory to %s", dir, "");
		exit(1);
	}
	if ((dirp = opendir(".")) == NULL) {
		Perror("failed to open directory %s", dir, "");
		exit(1);
	}

	dp = readdir(dirp);
	dp = readdir(dirp); /* read "." and ".." */
	for (dp = readdir(dirp); dp != NULL; dp = readdir(dirp))
		if (chfile(dp->d_name, uid, gid, modebits))
			chdirectory(dp->d_name, uid, gid, modebits);
	closedir(dirp);

	if (chdir(savedir) == -1) {
		Perror("can't change directory to %s", savedir, "");
		exit(1);
	}
}


/*
 * chfile - cautiously change the mode and permissions of a file.  Return
 *	1 if file was a directory, 0 otherwise.
 */
int
chfile(name, uid, gid, modebits)
	char *name;
	int uid, gid, modebits;
{
	struct stat st;
	int isdir = 0;
	int okaytochange = 0;
	int mode;

	/*
	 * Act real suspicious.  We only change directories and regular
	 * files, the latter only if they have a single hard link.
	 * Complain about everything else, including symbolic links.
	 */
	if (lstat(name, &st) != 0) {
		Perror("can't stat %s", name, "");
		exit(1);
	}

	switch(st.st_mode & S_IFMT) {
	case S_IFDIR:
		okaytochange = 1;
		isdir = 1;
		break;
	case S_IFREG:
		if (st.st_mode & (S_ISUID|S_ISGID)) {
			syslog(LOG_ALERT,
		"user %s attempting to change mode of set[ug]id file %s",
			    username, name);
			(void) fprintf(stderr, "%s: file %s is set[ug]id\n",
			    thisprogname, name);
		} else if (st.st_nlink > 1) {
			syslog(LOG_ALERT,
		"user %s attempting to change mode of file %s, has %d links",
			    username, name, st.st_nlink);
			(void) fprintf(stderr, "%s: file %s has %d links\n",
			    thisprogname, name, st.st_nlink);
		} else if (st.st_mode & S_ISVTX) {
			if (debug)
				printf("%s: will not change file %s\n",
					    thisprogname, name);
		} else {
			okaytochange = 1;
		}
		break;
	default:
		syslog(LOG_ALERT,
		"user %s attempting to change mode of %s, current mode is 0%o",
		    username, name, st.st_mode);
		fprintf(stderr,
		    "%s: file %s is neither directory nor regular file\n",
		    thisprogname, name);
		break;
	}

	if (okaytochange) {
		/*
		 * Only do the system calls if we need to.
		 */
		if ((uid != -1 && st.st_uid != uid) ||
		    (gid != -1 && st.st_gid != gid)) {
			if (debug) {
				(void) printf("chown'ing %s to %d,%d\n",
				    name, uid, gid);
			} else if (chown(name, uid, gid) != 0) {
				Perror("can't chown %s", name, "");
			}
		}
		if ((st.st_mode & modebits) != modebits) {
			mode = (st.st_mode | modebits) & 07777;
			if (debug) {
				(void) printf("chmod'ing %s to mode 0%o\n",
				    name, mode);
			} else if (chmod(name, mode) != 0) {
				Perror("can't chmod %s", name, "");
			}
		}
	}
	return isdir;
}



/*
 * Perror - print an error message
 */
Perror(fmt, s1, s2)
	char *fmt;
	char *s1, *s2;
{
	int e = errno;

	(void) fprintf(stderr, "%s: ", thisprogname);
	(void) fprintf(stderr, fmt, s1, s2);
	(void) fputs(": ", stderr);
	errno = e;
	perror("");
}
