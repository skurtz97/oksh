/*	zos_spawn.c -- z/OS process creation layer for oksh
 *
 * Process creation on z/OS UNIX System Services is very unlike Unix:
 *
 *  - fork() creates a *new address space* that (with the default
 *    _BPX_SHAREAS=NO) is a complete copy of the parent's.  For a
 *    large statically-linked process this costs tens of milliseconds
 *    of CPU per fork, plus a full round of LE/RTL initialization in
 *    the child.  bash and zsh pay this on *every* external command.
 *  - With _BPX_SHAREAS=YES, fork() creates a *shared* daughter that
 *    shares the parent's 31/64-bit storage, which makes fork() much
 *    cheaper.  Fine for a shell child that is about to execve() and
 *    never touches shared state, and it is what we want for the
 *    children that can't use spawn() at all (subshells, pipelines,
 *    command substitution, coprocs).
 *  - spawnp() never forks at all: the kernel service BPX1SPN creates
 *    the child directly in a fresh address space and execs the named
 *    program there.  There is no copy of the parent's storage and no
 *    duplicated LE initialization for the shell image.  This is the
 *    cheapest possible external command on z/OS, typically several
 *    times faster than a shared-AS fork and an order of magnitude
 *    faster than an unshared fork of a big shell image.  dash is fast
 *    on Unix because it is tiny and forks a small parent; on z/OS the
 *    size of the parent does not matter with spawn(), so a full
 *    korn shell can match or beat dash while keeping job control,
 *    line editing, history etc. -- features dash simply does not have.
 *
 * Strategy in this file:
 *
 *   1. external commands that would, under fork(), do nothing but
 *      inherit fds, apply signal defaults, and execve() are created
 *      with spawnp() (zos_spawn_child(), hooked into exchild()).
 *   2. everything else keeps fork(); zos_spawn_init() sets
 *      _BPX_SHAREAS=YES so those forks get a cheap shared daughter.
 *
 * Why plain external commands are safe with spawn:
 *
 *   - ksh sets redirections for a forked TCOM up in the *parent*
 *     (execute()/iosetup()) before calling comexec()/exchild(); the
 *     forked child just inherits the redirected fds.  spawn() also
 *     duplicates the parent's fds into 0/1/2 by default, so `cmd >f'
 *     and `cmd 2>err' work and the parent's savefd machinery still
 *     restores the shell's fds afterwards.
 *   - process groups (FMONITOR) are set atomically at creation with
 *     SPAWN_SETGROUP|SPAWN_NEWPGROUP -- no setpgid/tcsetpgrp race.
 *   - `cmd &' without job control on the fork path redirects fd 0 to
 *     /dev/null and ignores SIGINT/SIGQUIT in the child.  With spawn
 *     we map the child's fd 0 to a parent-side open of /dev/null
 *     through the fds[3] map, and ignored dispositions are inherited
 *     automatically (the shell ignores INT/QUIT when it is not
 *     talking; when it is, and this is a background child, the
 *     inherited ignore is what the fork path wants to *keep*, so the
 *     "reset to default" case below only applies to foreground
 *     monitor commands).
 *
 * Headers on the z/OS sysroot that we rely on:
 *   <spawn.h>       spawnp(), struct inheritance, SPAWN_* flags
 *   <unistd.h>      setenv etc.
 *   <sys/wait.h>    pid bookkeeping is unchanged (jobs.c waitpid)
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "zos_spawn.h"
#include "sh.h"

#ifdef HAVE_ZOS_SPAWN

#include <spawn.h>

void	zos_spawn_init(void);
int	zos_spawn_child(int flags, struct op *t, int tty_fd,
	    pid_t *pgrp_out, pid_t *pid_out);

/* 		    Initialization 	    */

void
zos_spawn_init(void)
{
	const char *cp;

	/* Explicit opt-out for people whose site policy or daemon
	 * behaviour conflicts with shared address spaces; the spawned
	 * fast path (spawnp) works either way.
	 */
	cp = getenv("_OKSH_SHAREAS");
	if (cp != NULL && (strcmp(cp, "NONE") == 0 || strcmp(cp, "none") == 0))
		return;
	if (getenv("_BPX_SHAREAS") == NULL)
		setenv("_BPX_SHAREAS", "YES", 1);
	/* Batch (non-OMVS) environments: pin the job name so forked
	 * daughters show up usefully in JES.  Harmlessly ignored in
	 * interactive shells unless supported.
	 */
	if (getenv("_BPX_JOBNAME") == NULL)
		setenv("_BPX_JOBNAME", "OKSH", 1);
}

/* 		    The spawnp fast path 	    */

/* Telltale signals that the fork path would SS_RESTORE_DFL (jobs.c:
 * tt_sigs[]).  Kept local so we don't need to touch jobs.c.
 */
static const int	zos_tt_sigs[] = { SIGTSTP, SIGTTIN, SIGTTOU };

/*
 * Decide whether the child described by (flags, t) is a "pure exec":
 * a TEXEC node that, in the fork path, would do nothing but inherit
 * fds, reset signals, and execve().
 */
static int
spawnable(int flags, struct op *t)
{
	if (t->type != TEXEC)
		return 0;		/* subshells, () children etc. */

	/*
	 * Pipeline members inherit *all* parent pipe fds on spawn and
	 * we cannot close the siblings' ends in the child (the fds[]
	 * map only covers 0/1/2), so a later stage would wait forever
	 * for EOF.  Keep fork for those -- and for coprocs, command
	 * substitution and `exec'-redirect forms that need real shell
	 * code in the child.
	 */
	if (flags & (XPIPEI | XPIPEO | XPCLOSE | XCCLOSE | XCOPROC | XXCOM))
		return 0;
	return 1;
}

int
zos_spawn_child(int flags, struct op *t, int tty_fd,
    pid_t *pgrp_out, pid_t *pid_out)
{
	struct inheritance	inh;
	const char		 **argv;
	const char		 **envp;
	int			 fds[3];
	int			 nullfd = -1;
	pid_t			 pid;
	sigset_t		 omask, sigdef;
	int			 i, mon;

	if (!spawnable(flags, t))
		return 1;

	if (t->args == NULL || t->args[0] == NULL)
		return 1;
	argv = (const char **)t->args;
	envp = (const char **)makenv();
	if (envp == NULL)
		return 1;

	memset(&inh, 0, sizeof(inh));

	/* fd map: the parent's 0/1/2 already carry exactly the
	 * redirections the fork path would have applied in iosetup()
	 * before fork(); spawn() duplicates them for the child.  Also
	 * handle the background-without-monitor /dev/null case here.
	 */
	fds[0] = 0;
	fds[1] = 1;
	fds[2] = 2;
	mon = Flag(FMONITOR);
	if ((flags & XBGND) && !mon) {
		nullfd = open("/dev/null", O_RDONLY);
		if (nullfd >= 0)
			fds[0] = nullfd;	/* mapped; closed below */
	}

	if (mon && !(flags & XXCOM)) {
		/* Create the child in its own process group atomically
		 * (pgroup = child pid).  This removes the
		 * setpgid/tcsetpgrp race that fork-based shells paper
		 * over.  ctlttyfd lets the kernel set the controlling
		 * tty process group like tcsetpgrp(tty_fd, pgrp) would.
		 */
		inh.flags |= SPAWN_SETGROUP;
		inh.pgroup = SPAWN_NEWPGROUP;
		if (tty_fd != -1)
			inh.ctlttyfd = tty_fd;
		/* Fork path does SS_RESTORE_DFL on the tty signals for
		 * monitor children (see exchild()).
		 */
		sigemptyset(&sigdef);
		for (i = 0; i < (int)NELEM(zos_tt_sigs); i++)
			sigaddset(&sigdef, zos_tt_sigs[i]);
		inh.flags |= SPAWN_SETSIGDEF;
		memcpy(&inh.sigdefault, &sigdef, sizeof(sigdef));
	}

	/* The child's signal mask must match what the fork path leaves
	 * behind: omask here is the caller's mask (SIGCHLD *not*
	 * blocked; exchild() blocked SIGCHLD in the parent only, which
	 * is trivially true because it's a mask on the parent).
	 */
	sigemptyset(&omask);
	inh.flags |= SPAWN_SETSIGMASK;
	memcpy(&inh.sigmask, &omask, sizeof(omask));

	pid = spawnp(t->str, 3, fds, &inh, argv, envp);

	if (nullfd != -1)
		close(nullfd);

	if (pid == -1) {
		/* spawn unavailable for this child (whatever the errno):
		 * fall back to the fork path -- it reports errors, exit
		 * statuses, etc. exactly as before.
		 */
		return 1;
	}

	/* Success: report the pid (and pgrp) to exchild(), which will
	 * record it on the job, unblock SIGCHLD, and return control to
	 * the normal foreground/background wait machinery -- exactly
	 * as if the child had been forked.
	 */
	*pid_out = pid;
	if (mon && !(flags & XXCOM))
		*pgrp_out = pid;	/* the child is its own pgrp leader */

	return 0;
}

#endif /* HAVE_ZOS_SPAWN */
