/*	zos_spawn.h -- z/OS process creation layer for oksh		*/

#ifndef ZOS_SPAWN_H_
#define ZOS_SPAWN_H_

#include <signal.h>
#include <sys/types.h>

/* fwd decls to avoid dragging in the private jobs structs */
struct ioword;

typedef struct op	Op;
typedef struct tbl	Tbl;

#if defined(__MVS__)
#define HAVE_ZOS_SPAWN	1
#endif

#ifdef HAVE_ZOS_SPAWN
/* One-time initialization called from main(): sets _BPX_SHAREAS=YES
 * (if absent) so that any fork() fallback (subshells and other
 * non-exec children) creates a *shared* daughter address space
 * instead of a full copy.
 * Controlled by the environment:
 *   _OKSH_SHAREAS=NONE   -> leave _BPX_SHAREAS alone
 */
void zos_spawn_init(void);

/* Try to create a child for `t' (a TEXEC node) using spawnp()
 * instead of fork/exec.  Called from exchild() after the Job/Proc
 * has been linked in.
 *
 * j and p come in from exchild and may be updated here.
 *
 * Returns:
 *   0  -> child was created with spawnp(); caller must unblock
 *         SIGCHLD and return (rv == 0).
 *   1  -> fall back to the caller's normal fork() path.
 */
int zos_spawn_child(int flags, struct op *t, int tty_fd,
    pid_t *pgrp_out, pid_t *pid_out);

#else /* !HAVE_ZOS_SPAWN */

#define zos_spawn_init()	((void) 0)
#define zos_spawn_child(flags, t, tty_fd, pgrp, pidp)	1

#endif /* HAVE_ZOS_SPAWN */

#endif /* !ZOS_SPAWN_H_ */
