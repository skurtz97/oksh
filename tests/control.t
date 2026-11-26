#!/bin/sh
# Compound commands, functions, exit status and set -e.

case $0 in
*/*)	d=${0%/*} ;;
*)	d=. ;;
esac
. "$d/lib.sh"

f() { echo "f:$1"; }
is 'function'		'f:arg'		"$(f arg)"

g() { return 3; }
g
is 'return status'	'3'		"$?"

if false; then not_ok 'if false'; else ok 'if false'; fi
if true; then ok 'if true'; else not_ok 'if true'; fi

n=0
while [ "$n" -lt 3 ]; do n=$((n + 1)); done
is 'while'		'3'		"$n"

out=
for i in 1 2 3; do out=$out$i; done
is 'for'		'123'		"$out"

case abc in
a*)	ok 'case glob' ;;
*)	not_ok 'case glob' ;;
esac

case xyz in
a*)	not_ok 'case default' ;;
*)	ok 'case default' ;;
esac

is 'and-or'		'yes'		"$(true && echo yes || echo no)"
is 'and-or false'	'no'		"$(false && echo yes || echo no)"
is 'background status'	'0'		"$(true & wait $!; echo $?)"
is 'command not found'	'127'		"$(nosuchcommand_oksh 2>/dev/null; echo $?)"
is 'exit status'	'2'		"$( (exit 2); echo $?)"
is 'set -e stops'	'1'		"$( (set -e; false; echo unreachable) 2>/dev/null; echo $?)"

finish
