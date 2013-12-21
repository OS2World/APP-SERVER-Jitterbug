#ifndef PIPE_H
#define PIPE_H

/*
 * $Id: pipe.h,v 1.1 2000/04/25 19:52:10 harald Exp $
 */
/*
 * Modified by harald.kipp@egnite.de.
 * All these additional modifications are public domain.
 *
 * $Log: pipe.h,v $
 * Revision 1.1  2000/04/25 19:52:10  harald
 * First OS/2 release
 *
 */

int pipe(unsigned long fd[2]);
FILE *popen(char *cmd, char *mode);
int pclose(FILE * pip);

#endif
