# oksh on z/OS -- process creation performance

This note documents the z/OS-specific work in this tree: what is slow on
z/OS UNIX System Services, what we changed, how it compares with bash,
zsh and dash, and how to measure it.

## 1. The problem

On classic Unix, fork() is cheap: children inherit the parent by
copy-on-write and execve() swaps the image.  On z/OS USS:

* `fork()` creates a **new address space**.  With the default
  `_BPX_SHAREAS=NO` the daughter receives a **complete copy** of the
  parent's 31/64-bit storage.  fork cost scales with the *size of the
  parent's allocations*, not with what the child will use.
* After the fork, the daughter runs its own full **Language
  Environment / RTL initialization** (CEEHDLR glue, heap setup, the
  works) before the exec'd program starts or -- for shells that exec
  immediately -- it is thrown away.  bine the fork+exec from a big C
  program (bash: ~2 MB of text and static data, zsh: ~2.5 MB) costs
  tens of milliseconds of *CPU* each time, on every simple command.
* `spawnp()` (BPX1SPN) never forks: the kernel creates the process
  directly in a fresh address space, hands it the supplied fd map,
  signal dispositions, process group and environment, and execs the
  program.  No copy of the parent's storage, no duplicated LE init for
  the shell image.  This is the cheapest possible way to run an
  external command on z/OS.
* `_BPX_SHAREAS=YES` makes fork() produce a *shared* daughter (it
  shares the parent's storage) -- much cheaper than an unshared fork,
  but not free [the daughter is still a new process-level obtainment of
  its own copy of the control blocks].  It is also optional and can be
  restricted (`att_noshareas` in the `f_attributes`/statx attribute
  block marks executables that must not run shared), and shared
  daughters are not permitted for owned/sharing cases.

Consequences:

* shells that always fork+exec every external command pay fork cost
  proportional to their own footprint; **dash is fast on z/OS mainly
  because dash itself is tiny** (~120 K of text, little static data).
  bash and zsh are big; zsh is generally slowest to fork here because
  of its larger static footprint + modules.
* A tek shell with spawn for plain commands is not size-bounded: the
  fork path stays fast if a shared daughter is used for the leftover
  cases, so oksh can match or beat dash on external-command throughput
  while keeping job control, command line editing, tab
  completion-through-its-own history, etc., that dash does not have.

## 2. What was changed in oksh

### New files

* `zos_spawn.c` / `zos_spawn.h` -- the z/OS process creation layer.
  Compiled into the build on z/OS (`__MVS__`), inert elsewhere.
* `configure` gains `zos_spawn.o` in `OBJS`.

### Hooks

* `main()`: calls `zos_spawn_init()` before anything else.
* `exchild()` (jobs.c): immediately before the fork loop, calls
  `zos_spawn_child()`.  If it returns 0, a child was created with
  `spawnp()`, the job/proc was registered with the real pid, and the
  function returns like the fork path would for the foreground
  wait-happens-later machinery.  If it returns 1 the normal fork path
  runs.  All existing bookkeeping (foreground/j state/savefd
  restores/tcsetpgrp with monitor) is unchanged.

### What uses spawnp()

`zos_spawn_child()` handles a TEXEC node exchild() receives from
comexec() (i.e. an external command after PATH lookup, except entry
when ioact means the child needs to run shell code):

* `plain cmd args... `and foreground/background (`cmd &`).
* Excluded: pipeline members (`XPIPEI|XPIPEO|XPCLOSE|XCCLOSE`),
  coprocs (`XCOPROC`) and command substitution (`XXCOM`).  Reason:
  spawn() children duplicate **all** parent fds except entries in the
  3-element `fds[]` map (0/1/2).  Sibling pipeline fds cannot be closed
  in the child, so a later pipeline stage would never see EOF.  These
  cases stay on fork, where every child inherits exactly the right fds
  and the parent's closures do not propagate.
* Redirections (`cmd >f`, `cmd 2>err`, heredocs, fd dups): are set up
  by ksh **in the parent** (`iosetup()`) before exchild() is called;
  the child inherits redirected 0/1/2, and so does a spawn child.  No
  special handling is needed -- and this is why the no-fork path is a
  transparent drop-in for redirs.
* Background without job control: fd 0 of the child is mapped to a
  private open of `/dev/null` performed in the parent just for the
  spawn (see the fds map in zos_spawn_child()); dispositions the shell
  ignores (SIGINT/SIGQUIT in scripts) are inherited ignored.
* Job control (`FMONITOR`): the child's process group is created
  atomically with `SPAWN_SETGROUP|SPAWN_NEWPGROUP`; `ctlttyfd` gives
  the kernel the controlling tty so the tcsetpgrp-fork-race that a
  shell normally closes via setpgid-in-parent-and-child disappears.
* Signal handling: `SPAWN_SETSIGMASK` sets the child's mask to the
  caller's mask (matches the SS_RESTORE_DFL path), `SPAWN_SETSIGDEF`
  resets the tty signals to default in monitor-foreground children.
  Children inherit the parent's *ignored* dispositions, exactly as
  forked exec children would.
* The fallback (return 1 → fork) keeps both correctness and the fork
  path's error reporting.

### Fork cost control

`zos_spawn_init()` sets `_BPX_SHAREAS=YES` (leaving it alone if the
user already set it) so any remaining fork (subshells, pipelines,
comsubs, coprocs) creates cheap shared daughters, and `_BPX_JOBNAME=OKSH`
for batch display.  `_OKSH_SHAREAS=NONE` in the environment disables
the env adjusting entirely (for sites where SHAREAS interacts badly
with some subsystem).  In that case, only spawn() path saves; pipelining
still works, it just pays for an unshared fork.

## 3. Comparisons with bash, zsh and dash

|                        | bash | zsh  | dash | oksh (this tree) on z/OS |
|------------------------|------|------|------|--------------------------|
| external command        | fork+exec (always) | fork+exec | fork+exec | spawnp on plain commands; fork+exec otherwise |
| fork cost driver        | parent size (~2 MB) | parent size (~2.5 MB + modules) | parent size (small) | parent size, but only for the non-spawnable cases |
| `_BPX_SHAREAS` handling | none (must be pre-set in env) | none | none | set/delegated to YES at startup by zos_spawn_init(), honors user/disable |
| process groups          | setpgid+tcsetpgrp race window | same | no job control | SPAWN_SETGROUP/SPAWN_NEWPGROUP, no race |
| `script` use in pipes   | heavy (bash subprocesses) | heavy | cheap fork | spawnp for the standalone parts, shared forks for the pipes |
| interactive             | rich, huge | rich, huge | none | oksh's emacs/vi line editing (~300 K total binary) vs. dash's none |

Expected shape of the results:

* `for i in {1..1000}; do :; done` loop of `cmd > /dev/null` style
  external commands, where the plain-command rule dominates -- oksh
  should be similar to or better than dash, and much better than bash
  or zsh (fork copy + LE init removed).
* Pipelines and command substitution remain fork-bound on z/OS for all
  four shells; the `_BPX_SHAREAS=YES` default means those forks create
  shared daughters, typically multiple x faster than an unshared fork.
* Only a tiny subset of forks is expected to use the *_SHRDIR_{NOPO|NO}
  wording that this feature is interleaved with; unreachable for most.

## 4. Headers used from the sysroot

/apps/include `/home/skurtz/sysroots/zos/usr/include`:

* `spawn.h` (and `le/spawn.h`), from IBM's HLE77E0: `spawnp()`,
  `struct inheritance`, `SPAWN_SETGROUP`, `SPAWN_NEWPGROUP`,
  `SPAWN_SETSIGMASK`, `SPAWN_SETSIGDEF`, `ctlttyfd`.
* `sys/stat.h` (le/), where the `f_attributes`/`att_*` bitfields live --
  documented if you want a `statx`-based "no shareas" preflight to
  avoid FORKing (we do not do it: not a public API surface in these
  headers; the spawn fast path doesn't need it since spawn never shares).
* `le/unistd.h` for `setenv` / proc matches (none of the rest of
  `execl`, `execle` use changes).

Note: `builtins.h`/`leawi.h` and friends are not used here -- the goal
was process creation, not a ptx wrapper layer.

## 5. How to measure on z/OS

Two clean, reproducible measurements; run from a batch job or an
OMVS telnet session, and repeat enough times to see stable numbers.

```sh
# (1) external-command throughput -- dominated by process creation
time ./oksh -c 'for i in $(seq 1 500); do /bin/true; done'
time /bin/bash -c 'for i in $(seq 1 500); do /bin/true; done'
time /bin/zsh  -c 'for i in $(seq 1 500); do /bin/true; done'
time /bin/dash -c 'for i in $(seq 1 500); do /bin/true; done'
# (2) the pure fork path (no /bin/true but shared daughters)
time ./oksh -c 'for i in $(seq 1 200); do ( : ); done'   # subshells
time ./oksh -c 'for i in $(seq 1 200); do echo x | :; done'
```

Server-style generators reported in SMF style -- the interesting
metrics are:

* elapsed wall time of the loop,
* TCB CPU of the shell (SMF30 / `time` builtin, jobs -l after),
* `BPXTRACE`-verified counts of BPX1FRK vs BPX1SPN.

A `-`spawn-advantage batch set (for i in 1..1000: `/bin/echo -n`) in a
loop inside OKSH should come in close, or under DASH's, as the number
of spawns approaches the total children while the fork-backed fallback
covers just pipeline and comsub workloads.

## 6. Future / not done here

* Maybe honor a stricter spawnability test by tracking which fds would
  be dirty in a pipeline child; this would let expanded pipelines use
  spawn with an fds[] map *only* if every stage only needs redirects
  into its own fds 0/1/2 and its pipe fds, which is representable if
  we synthesize the `fds[]` map from the pipeline structure.  Not done:
  first ships the safe, common wins.
* vfork() (real vfork is available on z/OS and shares the address
  space) as a fork replacement for comsub `$()` -- LE re-entry suffix;
  needs its own careful test around stack discipline of `vfork`'ed
  children running shell code.
* `SPAWN_SETREGIONSZ` / `SPAWN_SETCWD` (via `__spawn2`/`__spawnp2` on
  the same header) is available and could be exposed with a builtin
  like `dbg __bg` if anybody asks for per-job region/time controls.
* Possibly honor `_BPX_SHAREAS=CHILD` semantics vs YES and the
  `att_noshareas` preflight (`statx` if it is exported by the target
  libc level) to make the fork fallback risk-free for programs that
  confuse shared storage.
* Keeping print of `_BPX_*_CSV` vs actual internal spawn state as a
  `set -o` knob to diagnose whether BPX1SPN or BPX1FRK is being used
  (cheap `truss`-like trace in jobs.c/j_waitj logging when FMONITOR).
