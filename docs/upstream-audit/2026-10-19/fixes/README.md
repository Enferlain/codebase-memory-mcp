# Migration validation fixes

This directory records the follow-up to the three open items in
`docs/UPSTREAM_MIGRATION_2026-10-19.md`. Functional and static-gate qualification passes. Both required reviews returned;
actionable findings and minor follow-ups are addressed.

The first broad test run was invalidated by concurrently relinking its running
executable: `/proc/self/exe` retained the deleted inode path, and a self-spawn
returned ENOENT. Its early assertion also left a 35-byte test allocation live.
The compressed raw log is retained; it is not passing qualification. The
replacement run uses a separate stable executable path after both unity
objects and the runner have finished rebuilding.

`first-scoped-tidy.log` records the initial failures. `final-scoped-tidy.log`
and `python-unity-tidy.log` record the later focused rechecks. Full clang-tidy
still reports existing oversized upstream functions; this is not a claim that
an unfiltered whole-tree tidy sweep passes. Cognitive-complexity baseline
logs were compared against release a7158ac2. The separate upstream errno
probe reproduces that diagnostic in the unchanged publication helper.

Cppcheck uses its native parallel executor and a persistent build directory;
[the official manual](https://cppcheck.sourceforge.io/manual.pdf) documents
that the directory retains whole-program analysis with multiple threads.
The existing enabled checks, suppressions, default configuration search and
source list are unchanged. No -D restriction, reduced --max-configs, file
omission, or new suppression was added. The per-file manifest verifies every
intended translation unit returned, including cache hits. Gate canaries also
check CTU, header/source cache invalidation, conditional configurations and
worker failure propagation. This preserves the existing configuration policy;
it does not claim exhaustive enumeration beyond cppcheck's configured bound.

The stable affected-suite rerun completed: 1,031 passed, six platform skips,
no sanitizer findings. All 22 compatibility probes and nine process/artifact
checks pass. The first git-environment script invocation used its nonexistent
default build/c runner; supplying the built audit runner passed. Both invocations
are retained. The original combined upstream complexity probe encountered
unused-variable compiler diagnostics in generated unity members; the separately
retained upstream Python unity probe uses its grammar-equivalent warning mode.
No over-threshold function is new relative to upstream. Existing extended trace
and stale-cleanup functions remain 302/91 versus upstream's 278/86; Python's
large file processor remains upstream's60.

The main review's automated-wiring finding is addressed: canonical
`lint-cppcheck` now requires the canaries before it scans real sources. A silent
checker (`/bin/true`) was rejected by the canonical make target before the source
scan began. The canaries also support configured checker argv and print their
log on failure. Main-review dispositions, including verified primary libc sources
and the macOS runtime-validation limit, are in `review-dispositions.md`.
