/*
 * flock(2) for systems without it (z/OS), emulated with whole-file
 * fcntl(2) record locks.  Released into the public domain.
 *
 * Differences from BSD flock that oksh does not depend on: the lock is
 * per process rather than per open file description, it is released
 * when the process closes any descriptor for the file, and a shared
 * lock needs a descriptor open for reading (an exclusive one, writing).
 */

#include "pconfig.h"

#ifndef HAVE_FLOCK

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "portable.h"

int
flock(int fd, int operation)
{
	struct flock fl;
	int cmd;

	cmd = (operation & LOCK_NB) ? F_SETLK : F_SETLKW;

	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 0;		/* to end of file, however far it grows */

	switch (operation & ~LOCK_NB) {
	case LOCK_SH:
		fl.l_type = F_RDLCK;
		break;
	case LOCK_EX:
		fl.l_type = F_WRLCK;
		break;
	case LOCK_UN:
		fl.l_type = F_UNLCK;
		break;
	default:
		errno = EINVAL;
		return -1;
	}

	if (fcntl(fd, cmd, &fl) == -1) {
		/* fcntl reports a held lock as EACCES or EAGAIN */
		if ((operation & LOCK_NB) && errno == EACCES)
			errno = EWOULDBLOCK;
		return -1;
	}
	return 0;
}

#endif /* !HAVE_FLOCK */
