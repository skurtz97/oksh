#!/bin/sh
# Builtins a script relies on: test, export, unset, cd, dot, exit status.

case $0 in
*/*)	d=${0%/*} ;;
*)	d=. ;;
esac
. "$d/lib.sh"

[ -n abc ] && ok 'test -n' || not_ok 'test -n'
[ -z '' ] && ok 'test -z' || not_ok 'test -z'
[ abc = abc ] && ok 'test =' || not_ok 'test ='
[ 1 -lt 2 ] && ok 'test -lt' || not_ok 'test -lt'
[ -d "$(tmpdir)" ] && ok 'test -d' || not_ok 'test -d'

export FOO=bar
is 'export'		'bar'		"$(sh -c 'echo $FOO')"
unset FOO
is 'unset'		''		"$(sh -c 'echo ${FOO:-}')"

oldp=$(pwd)
cd "$(tmpdir)"
is 'cd'			"$(tmpdir)"	"$(pwd)"
cd "$oldp"

printf 'sourced=1\n' >"$(tmpdir)/src"
sourced=
. "$(tmpdir)/src"
is 'dot sourcing'	'1'		"$sourced"

: && ok 'colon is true' || not_ok 'colon is true'
false || ok 'or restores success' 

is 'echo -n'		'abc'		"$(echo -n abc)"
is 'echo several args'	'a b c'		"$(echo a b c)"
is 'subshell isolation'	'inner'		"$(x=inner; (x=outer); echo ${x:-unset})"

finish
