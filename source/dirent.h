#ifndef __DIRENT_H__
#define __DIRENT_H__
/*
 * $Id: dirent.h,v 1.1 2000/04/25 19:52:09 harald Exp $
 *
 * @(#)msd_dir.h 1.4 87/11/06   Public Domain.
 *
 *  A public domain implementation of BSD directory routines for
 *  MS-DOS.  Written by Michael Rendell ({uunet,utai}michael@garfield),
 *  August 1897
 *
 *  Extended by Peter Lim (lim@mullian.oz) to overcome some MS DOS quirks
 *  and returns 2 more pieces of information - file size & attribute.
 *  Plus a little reshuffling of some #define's positions    December 1987
 *
 *  Some modifications by Martin Junius                      02-14-89
 *
 *	AK900712
 *	AK910410	abs_path - make absolute path
 *
 * $Log: dirent.h,v $
 * Revision 1.1  2000/04/25 19:52:09  harald
 * First OS/2 release
 *
 * Revision 1.7  1995/04/28  22:41:18  ak
 * Cleanup. Make dirent2 layout compatible to EMX to avoid trouble
 * with include-files. Moved struct defs to implementation.
 *
 * Revision 1.6  1993/12/08  13:59:01  edvkai
 * Removed false RCSids from log.
 *
 * Revision 1.5  1992/09/14  12:25:45  ak
 * K.U.R.'s fixes.
 *
 * Revision 1.4  1992/02/14  18:08:23  ak
 * *** empty log message ***
 *
 * Revision 1.3  1992/01/03  14:19:44  ak
 *
 * Revision 1.2  1992/01/03  13:45:12  ak
 * Zortech fixes.
 *
 * Revision 1.1.1.1  1991/12/12  16:10:27  ak
 * Initial checkin of server source, modified to contain RCS IDs.
 *
 * Revision 1.1  1991/12/12  16:10:23  ak
 * Initial revision
 *
 */
/*
 * Modified by harald.kipp@egnite.de.
 * All these additional modifications are public domain.
 */

#ifdef __EMX__
#include <sys/param.h>
#else
#include <param.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* attribute stuff */
#ifndef A_RONLY
# define A_RONLY   0x01
# define A_HIDDEN  0x02
# define A_SYSTEM  0x04
# define A_LABEL   0x08
# define A_DIR     0x10
# define A_ARCHIVE 0x20
#endif

struct dirent {
#ifdef OS2	/* use the layout of EMX to avoid trouble */
    int            d_ino;                 /* Dummy */
    int            d_reclen;		  /* Dummy, same as d_namlen */
    int            d_namlen;              /* length of name */
    char           d_name[MAXNAMLEN + 1];
    unsigned long  d_size;
    unsigned short d_attribute;           /* attributes (see above) */
    unsigned short d_time;                /* modification time */
    unsigned short d_date;                /* modification date */
#else
    char	   d_name[MAXNAMLEN + 1]; /* garentee null termination */
    char	   d_attribute;		  /* .. extension .. */
    unsigned long  d_size;		  /* .. extension .. */
#endif
};

typedef struct _dirdescr DIR;
/* the structs do not have to be defined here */

extern DIR		*opendir(const char *);
extern DIR		*openxdir(const char *, unsigned);
extern struct dirent	*readdir(DIR *);
extern void		seekdir(DIR *, long);
extern long		telldir(DIR *);
extern void 		closedir(DIR *);
#define			rewinddir(dirp) seekdir(dirp, 0L)

extern char *		abs_path(const char *name, char *buffer, int len);

#ifdef __cplusplus
}
#endif

#endif
