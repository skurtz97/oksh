#!/bin/sh
# Run each tests/*.t with the shell under test.
#
#   sh tests/run.sh ./oksh
#
# Prints one line per test file and a final tally; exits nonzero on failure.

set -u

shell=${1:-./oksh}
case $shell in
/*)	;;
*)	shell=$(pwd)/$shell ;;
esac
if [ ! -x "$shell" ]; then
	echo "tests/run.sh: $shell is not executable" >&2
	exit 2
fi

dir=$(cd "$(dirname "$0")" && pwd)
TEST_SHELL=$shell
export TEST_SHELL

total=0
failed=0
for t in "$dir"/*.t; do
	name=${t##*/}
	name=${name%.t}
	log="$dir/.$name.log"
	(cd "$dir" && "$shell" "$t") >"$log" 2>&1
	n=$(sed -n 's/^# total: //p' "$log")
	f=$(sed -n 's/^# failed: //p' "$log")
	total=$((total + ${n:-0}))
	failed=$((failed + ${f:-0}))
	if [ -z "$n" ]; then
		echo "FAIL $name (no result)"
		cat "$log"
	elif [ "${f:-0}" -ne 0 ]; then
		echo "FAIL $name (${f:-0} of ${n:-0})"
		sed -n '/^not ok/p;/^#   /p' "$log"
	else
		echo "PASS $name ($n checks)"
	fi
	rm -f "$log"
done

echo "tests: $total, failed: $failed"
[ "$failed" -eq 0 ]
