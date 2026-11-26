#!/bin/sh
# z/OS only: the shell must agree with the system's own EBCDIC tools.
#
# Skipped (zero checks, no failure) anywhere else.

case $0 in
*/*)	d=${0%/*} ;;
*)	d=. ;;
esac
. "$d/lib.sh"

case $(uname -s) in
OS/390)	;;
*)	echo '# skipped: not z/OS'; finish; exit 0 ;;
esac

w=$(tmpdir)

# A file the shell writes has to be readable by the EBCDIC utilities.
echo hello-from-oksh >"$w/out"
is 'file readable by cat'	'hello-from-oksh'	"$(/bin/cat "$w/out")"
is 'file readable by sh'	'hello-from-oksh'	"$(/bin/sh -c "cat $w/out")"

# ... and a file the system writes has to be readable by the shell.
/bin/sh -c "printf 'sys line\n' > $w/sys"
is 'read system file'	'sys line'	"$(/bin/cat "$w/sys")"

got=
while IFS= read -r line; do got=$got"[$line]"; done <"$w/sys"
is 'read builtin'	'[sys line]'	"$got"

/bin/sh -c "printf 'a\nb\n' > $w/two"
is 'line count'		'2'		"$(wc -l <"$w/two" | tr -d ' ')"

is 'heredoc to system cat'	'nl'	"$(cat <<EOF
nl
EOF
)"

is 'external substitution'	'ext'	"$(/bin/echo ext)"

# The same, with the process-level auto-conversion switched off.
if [ -n "${TEST_SHELL:-}" ]; then
	got=$(_BPXK_AUTOCVT=OFF "$TEST_SHELL" -c "x=\$(/bin/echo off); echo \$x" 2>&1)
	is 'autocvt off substitution'	'off'		"$got"
	got=$(_BPXK_AUTOCVT=OFF "$TEST_SHELL" -c "echo piped | /bin/cat" 2>&1)
	is 'autocvt off pipeline'	'piped'		"$got"
	got=$(_BPXK_AUTOCVT=OFF "$TEST_SHELL" -c "read v < $w/sys; echo \$v" 2>&1)
	is 'autocvt off read'		'sys line'	"$got"
fi

finish
