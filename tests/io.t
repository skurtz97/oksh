#!/bin/sh
# Redirection, heredocs, pipelines and the read builtin.

case $0 in
*/*)	d=${0%/*} ;;
*)	d=. ;;
esac
. "$d/lib.sh"

w=$(tmpdir)
f=$w/file

echo one >"$f"
is 'write and read'	'one'		"$(cat "$f")"

echo two >>"$f"
is 'append'		'2'		"$(wc -l <"$f" | tr -d ' ')"

printf 'l1\nl2\n' >"$f"
got=
while IFS= read -r line; do got=$got"[$line]"; done <"$f"
is 'read loop'		'[l1][l2]'	"$got"

read -r first rest <"$f"
is 'read first line'	'l1'		"$first"

v=val
h=$(cat <<EOF
here $v
EOF
)
is 'heredoc expansion'	'here val'	"$h"

h=$(cat <<'EOF'
raw $v
EOF
)
is 'heredoc quoted'	'raw $v'	"$h"

is 'pipeline'		'x'		"$(echo x | cat)"
is 'pipeline status'	'0'		"$(echo x | cat >/dev/null; echo $?)"
is 'stderr redirect'	'err'		"$( (echo err >&2) 2>&1 )"
is 'stdout to stderr'	'out'		"$( (echo out 1>&2) 2>&1 )"

exec 3<"$f"
read -r l <&3
exec 3<&-
is 'descriptor 3 read'	'l1'		"$l"

: >"$w/empty"
is 'empty file'		'0'		"$(wc -c <"$w/empty" | tr -d ' ')"

echo captured >"$w/captured"
is 'heredoc from file'	'captured'	"$(cat <"$w/captured")"

finish
