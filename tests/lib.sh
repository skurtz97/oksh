# Helpers for the .t files.  Every test ends with `finish`, which prints the
# summary zopen_check_results() reads, and exits nonzero when something failed.

_t=0
_f=0

ok() {
	_t=$((_t + 1))
	printf 'ok %d - %s\n' "$_t" "$1"
}

not_ok() {
	_t=$((_t + 1))
	_f=$((_f + 1))
	printf 'not ok %d - %s\n' "$_t" "$1"
}

# is <what> <expected> <actual>
is() {
	if [ "$2" = "$3" ]; then
		ok "$1"
	else
		not_ok "$1"
		printf '#   expected: <%s>\n#   actual:   <%s>\n' "$2" "$3"
	fi
}

# Workspace for files a test creates, removed by finish().
tmpdir() {
	if [ -z "${_tmp:-}" ]; then
		_tmp=${TMPDIR:-$HOME/tmp}/oksh-test.$$
		mkdir -p "$_tmp"
	fi
	printf '%s\n' "$_tmp"
}

finish() {
	[ -n "${_tmp:-}" ] && rm -rf "$_tmp"
	printf '# total: %d\n# failed: %d\n' "$_t" "$_f"
	[ "$_f" -eq 0 ]
}
