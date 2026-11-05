/*
 * strsignal(3) for systems without it (z/OS).  Released into the
 * public domain.
 *
 * Looked up by the system's own SIG* values rather than by index, so
 * the messages are right whatever the signal numbering.
 */

#include "pconfig.h"

#ifndef HAVE_STRSIGNAL

#include <signal.h>

static const struct {
	int sig;
	const char *mess;
} sigmess[] = {
#ifdef SIGHUP
	{ SIGHUP, "Hangup" },
#endif
#ifdef SIGINT
	{ SIGINT, "Interrupt" },
#endif
#ifdef SIGQUIT
	{ SIGQUIT, "Quit" },
#endif
#ifdef SIGILL
	{ SIGILL, "Illegal instruction" },
#endif
#ifdef SIGTRAP
	{ SIGTRAP, "Trace/BPT trap" },
#endif
#ifdef SIGABRT
	{ SIGABRT, "Abort trap" },
#endif
#ifdef SIGEMT
	{ SIGEMT, "EMT trap" },
#endif
#ifdef SIGFPE
	{ SIGFPE, "Floating point exception" },
#endif
#ifdef SIGKILL
	{ SIGKILL, "Killed" },
#endif
#ifdef SIGBUS
	{ SIGBUS, "Bus error" },
#endif
#ifdef SIGSEGV
	{ SIGSEGV, "Segmentation fault" },
#endif
#ifdef SIGSYS
	{ SIGSYS, "Bad system call" },
#endif
#ifdef SIGPIPE
	{ SIGPIPE, "Broken pipe" },
#endif
#ifdef SIGALRM
	{ SIGALRM, "Alarm clock" },
#endif
#ifdef SIGTERM
	{ SIGTERM, "Terminated" },
#endif
#ifdef SIGURG
	{ SIGURG, "Urgent I/O condition" },
#endif
#ifdef SIGSTOP
	{ SIGSTOP, "Suspended (signal)" },
#endif
#ifdef SIGTSTP
	{ SIGTSTP, "Suspended" },
#endif
#ifdef SIGCONT
	{ SIGCONT, "Continued" },
#endif
#ifdef SIGCHLD
	{ SIGCHLD, "Child exited" },
#endif
#ifdef SIGTTIN
	{ SIGTTIN, "Stopped (tty input)" },
#endif
#ifdef SIGTTOU
	{ SIGTTOU, "Stopped (tty output)" },
#endif
#ifdef SIGIO
	{ SIGIO, "I/O possible" },
#endif
#ifdef SIGPOLL
	{ SIGPOLL, "Pollable event" },
#endif
#ifdef SIGXCPU
	{ SIGXCPU, "Cputime limit exceeded" },
#endif
#ifdef SIGXFSZ
	{ SIGXFSZ, "Filesize limit exceeded" },
#endif
#ifdef SIGVTALRM
	{ SIGVTALRM, "Virtual timer expired" },
#endif
#ifdef SIGPROF
	{ SIGPROF, "Profiling timer expired" },
#endif
#ifdef SIGWINCH
	{ SIGWINCH, "Window size changes" },
#endif
#ifdef SIGINFO
	{ SIGINFO, "Information request" },
#endif
#ifdef SIGUSR1
	{ SIGUSR1, "User defined signal 1" },
#endif
#ifdef SIGUSR2
	{ SIGUSR2, "User defined signal 2" },
#endif
#ifdef SIGPWR
	{ SIGPWR, "Power failure" },
#endif
	/* z/OS */
#ifdef SIGABND
	{ SIGABND, "Abend" },
#endif
#ifdef SIGIOERR
	{ SIGIOERR, "I/O error" },
#endif
#ifdef SIGDANGER
	{ SIGDANGER, "System resources low" },
#endif
#ifdef SIGTHSTOP
	{ SIGTHSTOP, "Thread stop" },
#endif
#ifdef SIGTHCONT
	{ SIGTHCONT, "Thread continue" },
#endif
#ifdef SIGTRACE
	{ SIGTRACE, "Trace" },
#endif
#ifdef SIGDCE
	{ SIGDCE, "DCE event" },
#endif
#ifdef SIGDUMP
	{ SIGDUMP, "Dump" },
#endif
};

char *
strsignal(int sig)
{
	unsigned int i;

	/* first match wins, so aliases (e.g. SIGPOLL == SIGIO) are fine */
	for (i = 0; i < sizeof(sigmess) / sizeof(*sigmess); i++) {
		if (sigmess[i].sig == sig)
			return (char *)sigmess[i].mess;
	}
	return "Unknown signal";
}

#endif /* !HAVE_STRSIGNAL */
