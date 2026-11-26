#!/bin/sh
# Word expansion, quoting, parameter expansion and arithmetic.

case $0 in
*/*)	d=${0%/*} ;;
*)	d=. ;;
esac
. "$d/lib.sh"

x=world
is 'plain expansion'	'hello world'	"hello $x"
is 'braced expansion'	'hello world'	"hello ${x}"
is 'single quotes'	'hello $x'	'hello $x'
is 'default value'	'fallback'	"${nosuch:-fallback}"
nosuch=set
is 'default when set'	'set'		"${nosuch:-fallback}"
is 'alternate value'	'altset'	"${nosuch:+altset}"
is 'length'		'5'		"${#x}"
is 'prefix strip'	'orld'		"${x#w}"
is 'suffix strip'	'worl'		"${x%d}"
is 'arith precedence'	'14'		"$((2 + 3 * 4))"
is 'arith comparison'	'1'		"$((7 > 3))"
is 'arith variable'	'11'		"$((10 + 1))"
is 'command substitution' 'sub'		"$(echo sub)"
is 'nested substitution' '[x]'		"[$(echo x)]"
is 'backquotes'		'bt'		"`echo bt`"
is 'quoted input kept'	'a b'		"$(printf '%s' 'a b')"

set -- a 'b c' d
is 'positional count'	'3'		"$#"
is 'positional split'	'b c'		"$2"

s=p:q:r
old=$IFS
IFS=:
set -- $s
IFS=$old
is 'IFS splitting'	'3'		"$#"
is 'IFS element'	'q'		"$2"

finish
