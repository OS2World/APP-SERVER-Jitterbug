/*
 * pipe.c
 *
 * (c) 1994 by Jochen Friedrich <jochen@audio.ruessel.sub.org>.
 *
 */
/*
 * The IBM CSet++ doesn't have pipe() and popen(), pclose() calls.
 * This module fixes this lack of important functions.
 */
/*
 * Modified by harald.kipp@egnite.de.
 * All these additional modifications are public domain.
 *
 * $Log: pipe.c,v $
 * Revision 1.1  2000/04/25 19:52:10  harald
 * First OS/2 release
 *
 */

#define INCL_DOSFILEMGR
#define INCL_DOSQUEUES
#include <os2.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <io.h>
#include <process.h>

#include <pipe.h>

int pipe(unsigned long fd[2])
{
    unsigned long status;

    if (DosCreatePipe(&(fd[0]), &(fd[1]), 4096))
	return -1;
    if (DosQueryFHState(fd[0], &status))
	return -1;
    status |= OPEN_FLAGS_NOINHERIT;
    status &= 0x7f88;
    if (DosSetFHState(fd[0], status))
	return -1;
    if (DosQueryFHState(fd[1], &status))
	return -1;
    status |= OPEN_FLAGS_NOINHERIT;
    status &= 0x7f88;
    if (DosSetFHState(fd[1], status))
	return -1;
    _setmode((int)(fd[0]), O_BINARY);
    _setmode((int)(fd[1]), O_BINARY);
    return 0;
}

FILE *popen(char *cmd, char *mode)
{
    FILE *our_end;
    unsigned long fd[2];
    int tempfd;
    int dupfd;

    pipe(fd);

    if (mode[0] == 'r') {
        our_end = fdopen((int)(fd[0]), mode);
	dupfd = 1;
	tempfd = dup(dupfd);
        dup2((int)(fd[1]), dupfd);
        close((int)(fd[1]));
    }
    else if (mode[0] == 'w') {
        our_end = fdopen((int)(fd[1]), mode);
	dupfd = 0;
	tempfd = dup(dupfd);
        dup2((int)(fd[0]), dupfd);
        close((int)(fd[0]));
    }

    _spawnlp(P_NOWAIT, getenv("COMSPEC"), getenv("COMSPEC"), "/c", cmd, NULL);
    dup2(tempfd, dupfd);
    close(tempfd);
    return our_end;
}

int pclose(FILE * pip)
{
    int i;

    if(fclose(pip))
        puts("closing pipe");
    if(_wait(&i) == -1)
        puts("child process");
    return i;
}
