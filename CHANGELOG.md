# Changelog

## 2026-11-26

- z/OS: build as an ASCII program and convert untagged and tagged EBCDIC data
  in the shell's own I/O layer. Untagged descriptors (files, pipes, the
  terminal) are translated on read and written back as EBCDIC; descriptors
  tagged for a conversion-capable codeset are left to the kernel and
  `_BPXK_AUTOCVT`. Output now uses EBCDIC NL (0x15) and both NL and LF read as
  newline, so files written by the shell are readable by `/bin/cat`, `/bin/sh`
  and friends, and files written by them are readable by the shell, with or
  without auto-conversion enabled.
- Add `configure` detection for `OS/390` (feature macros, ASCII character
  mode) and a zopen community `buildenv`.
- Add a test suite (`tests/`) and `make check`, used as the zopen build gate.
- Add include guards to the headers that lacked them.
