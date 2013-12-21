/*
 * Definitions etc. for regexp(3) routines.
 *
 * Caveat:  this is V8 regexp(3) [actually, a reimplementation thereof],
 * not the System V one.
 */
/*
 * Modified for OS/2 by harald.kipp@egnite.de.
 * All these additional modifications are public domain.
 *
 * $Log: regexp.h,v $
 * Revision 1.1  2000/04/25 19:52:10  harald
 * First OS/2 release
 *
 */

#ifndef _REGEXP
#define _REGEXP 1

#define NSUBEXP  10
typedef struct regexp {
	char *startp[NSUBEXP];
	char *endp[NSUBEXP];
	char regstart;		/* Internal use only. */
	char reganch;		/* Internal use only. */
	char *regmust;		/* Internal use only. */
	int regmlen;		/* Internal use only. */
	char program[1];	/* Unwarranted chumminess with compiler. */
} regexp;

#if defined(__STDC__) || defined(__cplusplus) || defined(__OS2__)
#   define _ANSI_ARGS_(x)       x
#else
#   define _ANSI_ARGS_(x)       ()
#endif

extern regexp *regcomp _ANSI_ARGS_((char *exp));
extern int regexec _ANSI_ARGS_((regexp *prog, char *string));
extern void regsub _ANSI_ARGS_((regexp *prog, char *source, char *dest));
extern void regerror _ANSI_ARGS_((char *msg));

#endif /* REGEXP */
