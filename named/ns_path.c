#include <sys/types.h>
#include <arpa/nameser.h>
#ifdef	T_MP
#ifndef	lint
static char *RCSid = "$Header: ns_path.c,v 1.8 87/09/02 23:02:29 rayan Exp $";
#endif	lint

/* ns_path.c based on the following rev levels of uupath files: */
/*	Header: uupath.c,v 2.1 86/10/24 00:25:21 path Exp	*/
/*	Header: pathToSite.c,v 2.2 86/10/24 00:24:55 path Exp	*/
/*	Header: domainPath.c,v 2.1 86/10/24 00:23:48 path Exp	*/
/*	Header: casechng.c,v 2.0 86/05/31 06:08:36 rayan Exp	*/
/*	Header: munge.c,v 2.1 86/10/24 00:24:01 path Exp	*/

/*
 * Copyright (c) 1986 by Rayan S. Zachariassen.
 *
 * Permission is granted to anyone to use this software for any
 * purpose on any computer system, and to redistribute it, subject
 * to the following four restrictions:
 *
 * 1. The author is not responsible for the consequences of use of
 *    this software, no matter how awful, even if they arise
 *    from imperfections in it.
 *
 * 2. The origin of this software must not be misrepresented, either
 *    by explicit claim or by omission.
 *
 * 3. Altered versions must be plainly marked as such, and must not
 *    be misrepresented as being the original software, but must be
 *    marked as being an alteration of this software.
 *
 * 4. Commercial or any for-profit redistribution of this software is
 *    prohibited unless by prior written agreement from the author.
 */

#include <stdio.h>
#include <ctype.h>
#include <syslog.h>
#include <sys/file.h>
#include <ndbm.h>

DBM	*dbworld, *dblocal;

char		*nsDB = 0;
char		*localDB = 0;
static char	*host;

extern char *makeroute(), *mklower(), *mkupper(), *mkmixed();

char *
ns_path(fromhost, dname)
char *fromhost, *dname;
{
	char	buf[BUFSIZ], path[BUFSIZ];
	static int	hadnspath = 0;

	if (!hadnspath) {
		if (nsDB)
			hadnspath = 1;
		else
			syslog(LOG_ERR, "nspath not initialized");
	}
	if (!nsDB)
		return 0;
	if (dbworld == 0)
		dbworld = dbm_open(nsDB, O_RDONLY, 0);
	if (dbworld == 0) {
		syslog(LOG_ERR, "nspath database (%s) unavailable", nsDB);
		return 0;
	}
	/* if this fails, that's ok. taken care of elsewhere */
	if (localDB && dblocal == 0)
		dblocal = dbm_open(localDB, O_RDONLY, 0);
	host = fromhost;
	strcpy(buf, dname);
	if (findPath(path, buf, (char *)0) || domainPath(path, buf, (char *)0))
		return makeroute(path, dname);
	dbm_close(dbworld);
	dbworld = NULL;
	if (dblocal) {
		dbm_close(dblocal);
		dblocal = NULL;
	}
	return 0;
}

/*
 *	Finds the path to the site if it exists in the database.
 *
 *	buf		- put path into this space if succesful.
 *	sitename	- name of the machine
 *	user		- name of the account, or "%" if printf string desired
 *
 *	returns buf if succesful or (char *)0 otherwise.
 *
 *	Requires dbminit() or mdbm_open() to have been called before first use.
 *		 host[] to contain the name of the site invoking this.
 */

char	lastfail[256] = { 0 };

char *
pathToSite(buf, sitename, user)
char	*buf, *sitename, *user;
{
	register char	*c, *cb;
	extern char	*rindex(), *index();
	char		save[512];
	datum		key, result;

	if (strcmp(sitename, lastfail) == 0)
		return (char *)0;
	key.dptr = sitename;
	key.dsize = strlen(key.dptr) + 1 ;
	result = dbm_fetch(dbworld, key);
	if (result.dptr != (char *)0) {
		c = index(result.dptr, '!');
		if (c != 0 && strlen(host) == (c - result.dptr)
		    && strncmp(host, result.dptr, c - result.dptr) == 0)
			strcpy(result.dptr, c+1);
		c = index(result.dptr, '!');
		if (dblocal != NULL && c != (char *)0
		    && (cb = index(c+1, '!')) != (char *)0) {
			char	*scp, buf[512];
			int	savesize;

			if (result.dptr != save) {
				strcpy(save, result.dptr);
				c = save + (c - result.dptr);
				cb = save + (cb - result.dptr);
			}
			savesize = result.dsize;
			/* copy 'neighbor!' */
			strncpy(buf, c + 1, cb - c);
			buf[cb - c] = '\0';
			/* append host to get 'neighbor!host' */
			strcat(buf, host);
			key.dptr = buf;
			key.dsize = strlen(buf) + 1;
			result = dbm_fetch(dblocal, key);
			scp = result.dptr;
			result.dptr = save;
			result.dsize = savesize;
			if (scp != (char *)0)
				result.dptr = c + 1;
		}
		if (user)
			;
		else if (strcmp("!%s",&result.dptr[result.dsize-4])==0)
			result.dptr[result.dsize-4] = '\0';
		else if (strncmp("%s", result.dptr, 2) == 0)
			result.dptr += 2 + 2*(result.dptr[2] == '%');
		if (!user)
			user = "<user>";
		pathprintf(buf, result.dptr, user);
		return buf;
	} else
		strcpy(lastfail, sitename);

	return 0;
}

/* unabashedly stolen from pathalias and added bugfixen... */

pathprintf(buf, path, user)
char	*buf, *path, *user;
{
	char	*cp;

	if (index(path, '@') == 0 && index(path, '!') == 0
	    && index(user, '@') == 0 && (cp = rindex(user, '%')) != 0)
		*cp = '@';
	for ( ; *buf = *path; path++) {
		if (*path == '%') {
			switch (path[1]) {
			case 's':
				if (index(user, '%') != (char *)0
				    && path[2] == '\0' && path[-1] == '!') {
					extern char *munge();

					user = munge(user);
				}
				strcpy(buf, user);
				buf += strlen(buf);
				path++;
				break;
			case '%':
				if (*user == '%')
					*++buf = '%';
				path++;
				/* fall through */
			default:
				buf++;
				break;
			}
		} else
			buf++;
	}
}

findPath(buffer, host, user)
char	*buffer, *host, *user;
{
	char	*cp;

	if (pathToSite(buffer, host, user) != (char *)0
	    || pathToSite(buffer, mklower(host), user) != (char *)0
	    || pathToSite(buffer, mkupper(host), user) != (char *)0
	    || pathToSite(buffer, mkmixed(host), user) != (char *)0)
		return 1;
	if ((cp = index(host+1, '.'))
	    && (mklower(host) || 1)
	    && (strcmp(cp, ".uucp") == 0)) {
		*cp = '\0';
		if (findPath(buffer, host, user)) {
			*cp = '.';
			return 1;
		}
		*cp = '.';
	}
	return 0;
}

domainPath(buffer, host, user)
char	*buffer, *host, *user;
{
	char	*dotp;
	char	useratdom[BUFSIZ];

	if (user == 0 || *user == '\0')
		user = "<user>";
	useratdom[0] = '\0';
	if (1|| (!index(user, '!') && !index(user, '%') && !index(user, '@'))) {
		strcat(useratdom, mklower(host));
		strcat(useratdom, "!");
	}
	strcat(useratdom, munge(user));
	for (dotp = index(host, '.'); dotp != 0; dotp = index(dotp+1, '.')) {
		if (findPath(buffer, mkupper(dotp), useratdom)
		    || findPath(buffer, dotp+1, useratdom))
			return 1;
	}
	return (!dotp && findPath(buffer, host, useratdom));
}

char *
mkupper(c)
register char	*c;
{
	char	*cp = c;

	for (; *c; c++)
		if (islower(*c))
			*c = toupper(*c);
	return cp;
}

char *
mklower(c)
register char	*c;
{
	char	*cp = c;

	for (; *c; c++)
		if (isupper(*c))
			*c = tolower(*c);
	return cp;
}

char *
mkmixed(c)
register char	*c;
{
	int	state = 0;
	char	*cp = c;

	/* turn abCD-Ef-GHijK into Abcd-Ef-Ghijk */
	for (; *c; c++) {
		if (state == 0 && islower(*c)) {
			*c = toupper(*c);
			state = 1;
		} else if (state == 1 && isupper(*c)) {
			*c = toupper(*c);
		} else if (*c == '-' || *c == '_')
			state = 0;
	}
	return cp;
}

char *
munge(address)
char	*address;
{
	char	*cp, buf[BUFSIZ];

	if ((cp = rindex(address, '@')) != (char *)0) {
		strcpy(buf, cp+1);
		strcat(buf, "!");
		*cp = '\0';
	} else
		buf[0] = '\0';
	while ((cp = rindex(address, '%')) != (char *)0) {
		strcat(buf, cp+1);
		strcat(buf, "!");
		*cp = '\0';
	}
	strcat(buf, address);
	strcpy(address, buf);
	return address;
}

char *
makeroute(uucppath, dname)
char *uucppath, *dname;
{
	char *cp, *name, *bp;
	char buf[BUFSIZ];
	int seendots, todot, dotted;

	cp = uucppath;
	bp = buf;
	*bp++ = '@';
	seendots = todot = 0;
	do {
		name = cp;
		if ((cp = index(name, '!')) == (char *)0)
			break;
		*cp++ = '\0';
		dotted = todot;
		if (!dotted && strcmp(name, dname) == 0)
			dotted = todot = 1;	/* so we can strip it later */
		while(*bp = *name++) {
			if (*bp == '.') seendots = dotted = 1;
			bp++;
		}
		/* the first non-domainized host gets a .uucp */
		if (seendots && !dotted) {
			*bp++ = '.';
			*bp++ = 'u'; *bp++ = 'u'; *bp++ = 'c'; *bp++ = 'p';
			todot = 1;
		}
		*bp++ = ',';
		*bp++ = '@';
	} while (cp);
	if (strcmp(name, "<user>") == 0) {
		if (bp > buf + 2)
			bp -= 2, *bp='\0';
		if ((name = rindex(buf, ',')) != NULL)
			*name = '\0';
	} else {
		strcpy(bp, name);
		if ((name = rindex(buf, ',')) != NULL) {
			if (*(name+1) == '@' && *(name+2) != '\0') {
				if (strcmp(name+2, dname) == 0)
					*name = '\0';
			}
		}
	}
	strcpy(uucppath, buf);
	return uucppath;
}
#endif	/* T_MP */
