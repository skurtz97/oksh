/*
 * z/OS character set handling.
 *
 * The shell is built as an ASCII program, but an untagged file on z/OS
 * is EBCDIC as far as every other program on the system is concerned.
 * The shell therefore converts untagged files itself, in and out, and
 * leaves tagged files alone: for those the kernel converts them (the
 * file tag plus _BPXK_AUTOCVT).
 */
#ifndef OKSH_CCSID_H
#define OKSH_CCSID_H

#include <sys/types.h>

#ifdef __MVS__
int	zos_untagged(int);
void	zos_etoa(char *, int);
void	zos_atoe(char *, int);
ssize_t	zos_write(int, const char *, size_t);
#else
typedef int oksh_ccsid_unused;
#define zos_write(fd, s, len)	write((fd), (s), (len))
#endif

#endif /* !OKSH_CCSID_H */
