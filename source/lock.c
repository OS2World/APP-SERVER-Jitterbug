/*
 * Modified for OS/2 by harald.kipp@egnite.de.
 * All these additional modifications are public domain.
 *
 * $Log: lock.c,v $
 * Revision 1.2  2000/04/25 19:52:09  harald
 * First OS/2 release
 *
 */
#include "jitterbug.h"

#define TIMEOUT 20
#define LOCKFILE "/.lock"

/* lock and unlock the bug tracking system */

static int locked;
extern int guest;

void lock_system(void)
{
	int i;
	int fd;
	int pid = getpid();
	char fname[1000];

	if (guest) return;

	if (locked) {
		locked++;
		return;
	}

	check_overflow(strlen(root_directory()) + strlen(LOCKFILE) + 10, sizeof(fname));
	sprintf(fname,"%s/%s", root_directory(), LOCKFILE);

	for (i=0;i<TIMEOUT;i++) {
		fd = open(fname, O_EXCL | O_CREAT | O_WRONLY, 0644);
		if (fd != -1) {
			write(fd, &pid, sizeof(pid));
			close(fd);
			locked = 1;
			return;
		}
		sleep(1);
	}

	if (unlink(fname)) fatal("unable to create/remove lockfile %s : %s\n",
				 fname, strerror(errno));
	
	/* damn, something probably died while it was locked. We'll
	   claim the lock for ourselves so that we can recover */
	locked = 1;
}

void unlock_system(void)
{
	char fname[1000];
	if (guest) return;

	if (locked > 1) {
		locked--;
		return;
	}

	if (!locked) return;

	check_overflow(strlen(root_directory()) + strlen(LOCKFILE) + 10, sizeof(fname));
	sprintf(fname,"%s/%s", root_directory(), LOCKFILE);
	
	unlink(fname);
	locked = 0;
}
