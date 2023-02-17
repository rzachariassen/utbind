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
static char sccsid[] = "@(#)ns_init.c	4.31 (Berkeley) 10/4/89";
#endif /* not lint */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>
#include <ctype.h>
#include <string.h>
#include <arpa/nameser.h>
#include "ns.h"
#include "db.h"

struct	zoneinfo *zones;		/* zone information */
int	nzones;				/* number of zones in use */
int	forward_only = 0;		/* run only as a slave */
char	*cache_file;
char	*localdomain;			/* "default" for non-dotted names */
int	maint_interval = 15*60;		/* minimum ns_maint() interval */

extern	int lineno;

#ifdef	NET_ERRS
extern	struct in_addr err_interface;
#endif	/* NET_ERRS */


/*
 * Read boot file for configuration info.
 */

ns_init(bootfile)
	char *bootfile;
{
	register struct zoneinfo *zp;
	struct zoneinfo *find_zone();
	extern char *calloc(), *realloc();
	extern int needmaint;
	static int loads = 0;			/* number of times loaded */

#ifdef DEBUG
	if (debug)
		fprintf(ddt,"\nns_init(%s)\n", bootfile);
#endif
	gettime(&tt);

        if (loads == 0) {
		loads++;
		if ((zones =
			(struct zoneinfo *)calloc(8, sizeof(struct zoneinfo)))
					    == NULL) {
		    syslog(LOG_ERR,
			"Not enough memory to allocate initial zones array");
		    exit(1);
		}
		nzones = 1;		/* zone zero is cache data */
		/* allocate cache hash table, formerly the root hash table. */
		hashtab = savehash((struct hashbuf *)NULL);

		/* allocate root-hints/file-cache hash table */
		fcachetab = savehash((struct hashbuf *)NULL);
		/* init zone data */
		zones[0].z_type = Z_CACHE;
        } else {
		/* Mark previous zones as not yet found in boot file. */
		for (zp = &zones[1]; zp < &zones[nzones]; zp++)
			zp->z_state &= ~Z_FOUND;
		if (localdomain)
			free(localdomain);
		localdomain = NULL;
		free_forwarders();
		free_sort_list();
		free_tcp_list();
		forward_only = 0;
	}

#ifdef DEBUG
        if (debug >= 3) {
        	fprintf(ddt,"\n content of zones before loading \n");
        	content_zone(nzones - 1);
        }
#endif

	if (ns_load(bootfile) < 0) {
		syslog(LOG_ERR, "%s: %m", bootfile);
		exit(1);
	}

        /* erase all old zones that were not found */
        for (zp = &zones[1]; zp < &zones[nzones]; zp++)
            if (zp->z_type && (zp->z_state & Z_FOUND) == 0) {
		remove_zone(hashtab, zp - zones);
		bzero((char *) zp, sizeof(*zp));
#ifdef DEBUG
		if(debug >=2)
			fprintf(ddt,"\n zone no %d was removed \n", zp - zones);
#endif
        }
#ifdef DEBUG
        if(debug >= 2) {
        	fprintf(ddt,"\n content of zones after loading \n");
        	content_zone(nzones-1);
        }
#endif

	/*
	 * Schedule calls to ns_maint().
	 */
	if (needmaint == 0)
		sched_maint();
#ifdef DEBUG
	if (debug)
		fprintf(ddt,"exit ns_init()%s\n", needmaint ?
		    ", need maintenance immediately" : "");
#endif
}

int
ns_load(file)
	char *file;
{
	register struct zoneinfo *zp;
	struct zoneinfo *find_zone();
	char buf[BUFSIZ], obuf[BUFSIZ], *source;
	extern char *calloc(), *realloc();
	int type, i;
	FILE *fp;
	extern int needmaint;
	extern char *xferprog;
        struct stat f_time;

	if ((fp = fopen(file, "r")) == NULL)
		return -1;

	lineno = 1;

	while (!feof(fp) && !ferror(fp)) {
		if (!getword(buf, sizeof(buf), fp))
			continue;
		/* read named.boot keyword and process args */
		if (strcasecmp(buf, "primary") == 0)
			type = Z_PRIMARY;
		else if (strcasecmp(buf, "secondary") == 0)
			type = Z_SECONDARY;
		else if (strcasecmp(buf, "cache") == 0)
			type = Z_CACHE;
		else if (strcasecmp(buf, "directory") == 0) {
			(void) getword(buf, sizeof(buf), fp);
			if (chdir(buf) < 0) {
				syslog(LOG_CRIT, "directory %s: %m\n",
					buf);
				exit(1);
			}
			endline(fp);
			continue;
		} else if (strcasecmp(buf, "xfer") == 0) {
			if (getword(buf, sizeof(buf), fp))
				xferprog = savestr(buf);
			endline(fp);
			continue;
		} else if (strcasecmp(buf, "sortlist") == 0) {
			get_sort_list(fp);
			continue;
		} else if (strcasecmp(buf, "forwarders") == 0) {
			get_forwarders(fp);
			continue;
		} else if (strcasecmp(buf, "slave") == 0) {
			forward_only++;
			endline(fp);
			continue;
		} else if (strcasecmp(buf, "tcplist") == 0) {
			get_tcp_list(fp);
			continue;
		} else if (strcasecmp(buf, "domain") == 0) {
			if (getword(buf, sizeof(buf), fp))
				localdomain = savestr(buf);
			endline(fp);
			continue;
		} else if (strcasecmp(buf, "include") == 0) {
			if (getword(buf, sizeof buf, fp)) {
				i = lineno;
				if (ns_load(buf) < 0)
					syslog(LOG_ERR, "%s: line %d: %s: %m\n",
							file, lineno, buf);
				lineno = i;
			} else
				syslog(LOG_ERR, "%s: line %d: missing path\n",
						file, lineno);
			endline(fp);
			continue;
		}
#ifdef	T_MP
		else if (strcasecmp(buf, "nspath") == 0) {
			extern char *nsDB, *localDB;

			if (getword(buf, sizeof buf, fp)) {
				nsDB = savestr(buf);
				if (getword(buf, sizeof buf, fp))
					localDB = savestr(buf);
			} else
				syslog(LOG_ERR,
				       "%s: line %d: missing database\n",
				       file, lineno);
			endline(fp);
			continue;
		}
#endif	/* T_MP */
		else if (strcasecmp(buf, "nserror") == 0) {
#ifdef	NET_ERRS
			if (!getword(buf, sizeof(buf), fp)) {
				syslog(LOG_ERR,
				  "%s: line %d: no nserror interface address\n",
				    file, lineno);
				continue;
			}
			err_interface.s_addr = inet_addr(buf);

			if (err_interface.s_addr == (u_long)-1) {
				syslog(LOG_ERR,
				    "%s: line %d: invalid address '%s'\n",
				    file, lineno, buf);
				endline(fp);
				continue;
			}
			if (!getword(buf, sizeof(buf), fp)) {
				syslog(LOG_ERR,
				    "%s: line %d: no socket count\n",
				    file, lineno);
				continue;
			}
			i = atoi(buf);
			if (i < 0 || i > 100) {	/* should define const */
				syslog(LOG_ERR,
			    "%s: line %d: socket count '%s' inappropriate\n",
				    file, lineno, buf);
				continue;
			}
			nerrsockets = i;
			endline(fp);
			continue;
#else	/* NET_ERRS */
			syslog(LOG_ERR,
		    "%s: line %d: daemon compiled without nserror support\n",
			    file, lineno);
#endif	/* NET_ERRS */
		}
		else {
			syslog(LOG_ERR, "%s: line %d: unknown field '%s'\n",
				file, lineno, buf);
			endline(fp);
			continue;
		}

		/*
		 * read zone origin
		 */
		if (!getword(obuf, sizeof(obuf), fp)) {
			syslog(LOG_ERR, "%s: line %d: missing origin\n",
			    file, lineno);
			continue;
		}
#ifdef DEBUG
		if (debug)
			fprintf(ddt, "zone origin %s", obuf);
#endif
		if (obuf[0] == '.' && obuf[1] == '\0')
			obuf[0] = '\0';
		/*
		 * read source file or host address
		 */
		if (!getword(buf, sizeof(buf), fp)) {
			syslog(LOG_ERR,
			    "%s: line %d: missing source or addr\n",
			    file, lineno);
			continue;
		}

		/* check for previous instance of this zone (reload) */
		if ((zp = find_zone(obuf, type)) == 0) {
			if (type == Z_CACHE) {
				zp = &zones[0];
				zp->z_origin = "";
				goto gotzone;
			}
			for (zp = &zones[1]; zp < &zones[nzones]; zp++)
				if (zp->z_type == 0)
					goto gotzone;
			/*
			 * this code assume that nzones never decreases
			 */
			if (nzones % 8  == 0) {
#ifdef DEBUG
			    if (debug > 1)
				fprintf(ddt, "Reallocating zones structure\n");
#endif DEBUG
			    /*
			     * Realloc() not used since it might damage zones
			     * if an error occurs
			     */
			    if ((zp = (struct zoneinfo *)
				malloc((8 + nzones) * sizeof(struct zoneinfo)))
					    == NULL) {
				    syslog(LOG_ERR, "no memory for more zones");
#ifdef DEBUG
				    if (debug)
					fprintf(ddt,
					    "Out of memory for new zones\n");
#endif DEBUG
				    endline(fp);
				    continue;
			    }
			    bcopy((char *)zones, (char *)zp,
					nzones * sizeof(struct zoneinfo));
			    bzero((char *)&zp[nzones],
					8 * sizeof(struct zoneinfo));
			    free((char *)zones);
			    zones = zp;
			}
			zp = &zones[nzones++];
	gotzone:
			zp->z_origin = savestr(obuf);
			zp->z_type = type;
		}
		zp->z_addrcnt = 0;

		switch (type) {
		case Z_CACHE:
			source = savestr(buf);
#ifdef DEBUG
			if (debug)
				fprintf(ddt,", source = %s\n", source);
#endif
			zp->z_refresh = 0;	/* by default, no dumping */
			if (getword(buf, sizeof(buf), fp)) {
#ifdef notyet
				zp->z_refresh = atoi(buf);
				if (zp->z_refresh <= 0) {
					syslog(LOG_ERR,
				"%s: line %d: bad refresh time '%s', ignored\n",
						file, lineno, buf);
					zp->z_refresh = 0;
				} else if (cache_file == NULL)
					cache_file = source;
#else
				syslog(LOG_WARNING,
				"%s: line %d: cache refresh ignored\n",
					file, lineno);
#endif
				endline(fp);
			}
			/*
			 * If we've loaded this file, and the file has
			 * not been modified and contains no $include,
			 * then there's no need to reload.
			 */
			if (zp->z_source && strcmp(source, zp->z_source) == 0 &&
			    (zp->z_state & Z_INCLUDE) == 0 && 
			    stat(zp->z_source, &f_time) == 0 &&
			    zp->z_ftime == f_time.st_mtime) {
#ifdef DEBUG
				if (debug)
					fprintf(ddt, "cache is up to date\n");
#endif
				break; /* zone is already up to date */
			}

			/* file has changed, or hasn't been loaded yet */
			if (zp->z_source) {
				free(zp->z_source);
				remove_zone(fcachetab, 0);
			}
			zp->z_source = source;
#ifdef DEBUG
			if (debug)
				fprintf(ddt, "reloading zone\n");
#endif
			(void) db_load(zp->z_source, zp->z_origin, zp, (char *)0);
			break;

		case Z_PRIMARY:
			source = savestr(buf);
    		        endline(fp);
#ifdef DEBUG
			if (debug)
				fprintf(ddt,", source = %s\n", source);
#endif
			/*
			 * If we've loaded this file, and the file has
			 * not been modified and contains no $include,
			 * then there's no need to reload.
			 */
			if (zp->z_source && strcmp(source, zp->z_source) == 0 &&
			    (zp->z_state & Z_INCLUDE) == 0 && 
			    stat(zp->z_source, &f_time) == 0 &&
			    zp->z_ftime == f_time.st_mtime) {
#ifdef DEBUG
				if (debug)
					fprintf(ddt, "zone is up to date\n");
#endif
				break; /* zone is already up to date */
			}
			if (zp->z_source) {
				free(zp->z_source);
				remove_zone(hashtab, zp - zones);
			}
                        zp->z_source = source;
                        zp->z_auth = 0;
#ifdef DEBUG
			if (debug)
				fprintf(ddt, "reloading zone\n");
#endif
			if (db_load(zp->z_source, zp->z_origin, zp, (char *)0) == 0)
				zp->z_auth = 1;
#ifdef ALLOW_UPDATES
			/* Guarantee calls to ns_maint() */
			zp->z_refresh = maint_interval;
#else ALLOW_UPDATES
			zp->z_refresh = 0;	/* no maintenance needed */
			zp->z_time = 0;
#endif ALLOW_UPDATES
			break;

		case Z_SECONDARY:
			source = 0;
#ifdef DEBUG
			if (debug)
				fprintf(ddt,"\n\taddrs: ");
#endif
			do {
				zp->z_addr[zp->z_addrcnt].s_addr =
					inet_addr(buf);
				if (zp->z_addr[zp->z_addrcnt].s_addr ==
						(unsigned)-1) {
					source = savestr(buf);
    		                        endline(fp);
					break;
				}
#ifdef DEBUG
				if (debug)
					fprintf(ddt,"%s, ",buf);
#endif
				if (++zp->z_addrcnt > NSMAX - 1) {
					zp->z_addrcnt = NSMAX - 1;
#ifdef DEBUG
					if (debug)
					    fprintf(ddt,
						"\nns.h NSMAX reached\n");
#endif
				}
			} while (getword(buf, sizeof(buf), fp));
#ifdef DEBUG
			if (debug)
				fprintf(ddt,"addrcnt = %d\n", zp->z_addrcnt);
#endif
			if (source == 0) {
#ifdef DEBUG
				if (debug)
					fprintf(ddt, "No source file\n");
#endif
				syslog(LOG_ERR,
				    "zone %s: missing backup file name\n",
				    zp->z_origin);
				break;
			}
			/*
			 * If we had a backup file name, and it was changed,
			 * free old zone and start over.  If we don't have
			 * current zone contents, try again now in case
			 * we have a new server on the list.
			 */
			if (zp->z_source && strcmp(source, zp->z_source)) {
#ifdef DEBUG
				if (debug)
					fprintf(ddt,"backup file changed\n");
#endif
				free(zp->z_source);
				zp->z_source = 0;
				zp->z_auth = 0;
                        	remove_zone(hashtab, zp - zones);
			}
			if (zp->z_source)
				free(source);
			else
				zp->z_source = source;
			if (zp->z_auth == 0)
				zoneinit(zp);
			break;

		}
                zp->z_state |= Z_FOUND;
#ifdef DEBUG
		if (debug)
			fprintf(ddt,"zone[%d] type %d: '%s'",
			        zp-zones, type,
				*(zp->z_origin) == '\0' ? "." : zp->z_origin);
#endif
		if (zp->z_refresh && zp->z_time == 0)
			zp->z_time = zp->z_refresh + tt.tv_sec;
		if (zp->z_time <= tt.tv_sec)
			needmaint = 1;
#ifdef DEBUG
		if (debug)
			fprintf(ddt, " z_time %d, z_refresh %d\n",
			    zp->z_time, zp->z_refresh);
#endif
	}
	(void) fclose(fp);
	return 0;
}

zoneinit(zp)
	register struct zoneinfo *zp;
{
	struct stat sb;

	/*
	 * Try to load zone from backup file,
	 * if one was specified and it exists.
	 * If not, or if the data are out of date,
	 * we will refresh the zone from a primary
	 * immediately.
	 */
	if (zp->z_source == NULL)
		return;
	if (stat(zp->z_source, &sb) == -1 ||
	    db_load(zp->z_source, zp->z_origin, zp, (char *)0) != 0) {
		/*
		 * Set zone to be refreshed immediately.
		 */
		zp->z_refresh = INIT_REFRESH;
		zp->z_retry = INIT_REFRESH;
		zp->z_time = tt.tv_sec;
		needmaint = 1;
	} else
		zp->z_auth = 1;
}

#ifdef ALLOW_UPDATES
/*
 * Look for the authoritative zone with the longest matching RHS of dname
 * and return its zone # or zero if not found.
 */
findzone(dname, class)
	char *dname;
	int class;
{
	char *dZoneName, *zoneName, *index(), *dotPos;
	int dZoneNameLen, zoneNameLen;
	int maxMatchLen = 0;
	int maxMatchZoneNum = 0;
	int zoneNum;

#ifdef DEBUG
	if (debug >= 4)
		fprintf(ddt, "findzone(dname=%s, class=%d)\n", dname, class);
	if (debug >= 5) {
		fprintf(ddt, "zone dump:\n");
		for (zoneNum = 1; zoneNum < nzones; zoneNum++)
			printzoneinfo(zoneNum);
	}
#endif DEBUG

	dZoneName = index(dname, '.');
	if (dZoneName == NULL)
		dZoneName = "";	/* root */
	else
		dZoneName++;	/* There is a '.' in dname, so use remainder of
				   string as the zone name */
	dZoneNameLen = strlen(dZoneName);
	for (zoneNum = 1; zoneNum < nzones; zoneNum++) {
		zoneName = (zones[zoneNum]).z_origin;
		zoneNameLen = strlen(zoneName);
		/* The zone name may or may not end with a '.' */
		dotPos = index(zoneName, '.');
		if (dotPos)
			zoneNameLen--;
		if (dZoneNameLen != zoneNameLen)
			continue;
#ifdef DEBUG
		if (debug >= 5)
			fprintf(ddt, "about to strncasecmp('%s', '%s', %d)\n",
			        dZoneName, zoneName, dZoneNameLen);
#endif
		if (strncasecmp(dZoneName, zoneName, dZoneNameLen) == 0) {
#ifdef DEBUG
			if (debug >= 5)
				fprintf(ddt, "match\n");
#endif
			/*
			 * See if this is as long a match as any so far.
			 * Check if "<=" instead of just "<" so that if
			 * root domain (whose name length is 0) matches,
			 * we use it's zone number instead of just 0
			 */
			if (maxMatchLen <= zoneNameLen) {
				maxMatchZoneNum = zoneNum;
				maxMatchLen = zoneNameLen;
			}
		}
#ifdef DEBUG
		else
			if (debug >= 5)
				fprintf(ddt, "no match\n");
#endif
	}
#ifdef DEBUG
	if (debug >= 4)
		fprintf(ddt, "findzone: returning %d\n", maxMatchZoneNum);
#endif DEBUG
	return (maxMatchZoneNum);
}
#endif ALLOW_UPDATES

soa_zinfo(zp, cp, eom)
	register struct zoneinfo *zp;
	register u_char *cp;
	u_char *eom;
{
	cp += 3 * sizeof(u_short);
	cp += sizeof(u_long);
	cp += dn_skipname(cp, eom);
	cp += dn_skipname(cp, eom);
	GETLONG(zp->z_serial, cp);
	GETLONG(zp->z_refresh, cp);
	gettime(&tt);
	zp->z_time = tt.tv_sec + zp->z_refresh;
	GETLONG(zp->z_retry, cp);
	GETLONG(zp->z_expire, cp);
	GETLONG(zp->z_minimum, cp);
}

get_forwarders(fp)
	FILE *fp;
{
	char buf[BUFSIZ];
	register struct fwdinfo *fip = NULL, *ftp = NULL;
	extern struct sockaddr_in nsaddr;
	extern struct fwdinfo *fwdtab;

#ifdef DEBUG
	if (debug)
		fprintf(ddt,"forwarders ");
#endif
	while (getword(buf, sizeof(buf), fp)) {
		if (strlen(buf) == 0)
			break;
#ifdef DEBUG
		if (debug)
			fprintf(ddt," %s",buf);
#endif
		if (ftp == NULL)
			ftp = (struct fwdinfo *)malloc(sizeof(struct fwdinfo));
		if (isdigit(buf[0]) &&
		    (ftp->fwdaddr.sin_addr.s_addr = inet_addr(buf)) !=
		    (unsigned)-1) {
			ftp->fwdaddr.sin_port = nsaddr.sin_port;
			ftp->fwdaddr.sin_family = AF_INET;
		} else {
			syslog(LOG_ERR, "'%s' (ignored, NOT dotted quad)", buf);
#ifdef DEBUG
			if (debug)
				fprintf(ddt," (ignored, NOT dotted quad)");
#endif
			continue;	
		}
		ftp->next = NULL;
		if (fwdtab == NULL)
			fwdtab = ftp;	/* First time only */
		else
			fip->next = ftp;
		fip = ftp;
		ftp = NULL;
	}
	if (ftp)
		free((char *)ftp);
	
#ifdef DEBUG
	if (debug) 
		fprintf(ddt,"\n");
	if (debug > 2)
		for (ftp = fwdtab; ftp != NULL; ftp = ftp->next)
			fprintf(ddt,"ftp x%x %s next x%x\n", ftp,
				inet_ntoa(ftp->fwdaddr.sin_addr), ftp->next);
#endif
}

free_forwarders()
{
	register struct fwdinfo *ftp, *fnext;

	for (ftp = fwdtab; ftp != NULL; ftp = fnext) {
		fnext = ftp->next;
		free((char *)ftp);
	}
	fwdtab = NULL;
}

struct zoneinfo *
find_zone(name, type) 
	char *name;
	int type;
{
	register struct zoneinfo *zp;

        for (zp = &zones[1]; zp < &zones[nzones]; zp++)
		if (zp->z_type == type && strcasecmp(name, zp->z_origin) == 0) {
#ifdef DEBUG
			if (debug > 1)
				fprintf(ddt, ", old zone (%d)", zp - zones);
#endif
			return (zp);
		}
#ifdef DEBUG
	if(debug > 1)
		fprintf(ddt, ", new zone");
#endif
	return ((struct zoneinfo *)NULL);
}

/* prints out the content of zones */
int content_zone(end) 
	int  end;
{
	int i;
	for (i = 1; i <= end; i++)
		printzoneinfo(i);
}
