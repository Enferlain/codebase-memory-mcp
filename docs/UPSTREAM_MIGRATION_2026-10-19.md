# v0.12.1 selective migration trial

**Installed on 2026-10-19.** The tested `fork/v0.12.1-patches` checkout now
occupies `/home/imi/Projects/codebase-memory-mcp`. The separate
`codebase-memory-mcp-v0121` worktree was removed. The installed executable at
`/home/imi/.local/bin/codebase-memory-mcp` is `0.12.1-fork.1`, SHA-256
`4962cf320c2345cf47b7871fd5f1a14107aecf13e77a86b1ffd35689888ea026`.
The guarded installer stopped old clients, replaced the binary, and discarded
21 old project indexes. No backup was created, as explicitly requested; existing
client integrations and CBM settings were preserved. Live reindexing and
post-install checks are recorded in [the deployment receipt](upstream-audit/2026-10-19/deployment/README.md).

The trial and in-action sections below describe the earlier isolated validation
before installation, including their original temporary paths and installation
state. Those historical statements do not describe the current installed state.

## What was carried forward

| Group | Status and implementation | Evidence |
| --- | --- | --- |
| P1 guarded imports | **PORT implemented.** Recognize direct `TYPE_CHECKING` / `typing.TYPE_CHECKING` imports and replay them through upstream's classifier. Preserve current typed-field export/ownership. | Guarded and ordinary-import integration controls; malformed-body null guards. |
| P2 sibling mixins | **PORT implemented.** Bounded C3 assembly respects base order, abstract masking, and own-method shadowing. Preserve upstream nearest-ancestor matching. | Sibling assembly, order/shadowing, nearest-ancestor tests. |
| P3 dispatch and edge output | **PORT implemented.** SQL and CSR BFS compose CALLS with bidirectional OVERRIDE. Optional `topology=true` emits typed incident edges, with pagination, filtering, budget, and cursor checks. | SQL/CSR parity; public MCP tests in both directions; fresh real-project trace. |
| P4 false targets | **PORT implemented.** Bind nested declarations in upstream's carried lexical state; suppress weak production-to-test guesses in both sequential and parallel venues. | Independent constructor/member negatives and genuine-call positives; fresh graph query. |
| P5 crash recovery | **PORT implemented.** Rebase stale device identity only with retained inode/current-filesystem corroboration, including interrupted publication. | Committed socket, temporary publication windows, existing rejection checks. |

**DROP:** old typed-field export/fold, old ancestor walk, old parameter walk,
and historic fixes already in common upstream history. No wholesale replay of
the old fork commits was used.

**VERIFY resolved for this trial:** canonical upstream exported field QNs
resolve successfully; the older synthetic relative-QN surface remains an
unsupported representation in its adapted regression. No normalization
workaround was ported. The dotted `os.write` workaround was also omitted.

**CANDIDATE remains:** complete nested Python graph coverage and full trace
path-instance multiplicity are not implemented by these ports. Typed incident
edge output is not a complete list of path instances.

An upstream test-fault stub also triggered cppcheck in production builds.
Its caller now compiles the conditional only with test seams enabled;
production behavior and the release binary checksum are unchanged.

The official tag's package metadata still said 0.12.0. The branch also carries
upstream's subsequent metadata-only 0.12.1 corrections from `9dba7fe1`.
The semantic index version is `1000009`, deliberately outside upstream's
current range, so existing generations rebuild under the changed semantics.

## Fresh real-project acceptance

A fresh, private cache indexed a 943-file snapshot of `sd-scripts` at
`9c4d7addc7fcac2bdd194a3d7cbadb7ce8603df9`: 15,157 nodes, 64,676 edges,
zero skipped files. Two unrelated files have recorded partial parse coverage
(dashboard HTML and benchmark PowerShell); excluded vendor/assets subtrees
remain excluded. Coverage for all eight material Python files matched the
new generation and reported no recorded issue. This remains a best-effort
coverage signal.

- `_run_epoch_steps` reaches `DiffusionTrainingStrategy.process_batch`.
- `_maybe_cache_epoch_tokens` reaches the caching contract.
- The first trace reaches SD, SDXL, and SD3 concrete implementations at hop 2,
  returning 26 typed incident edges. Recorded CALLS confidence is preserved;
  missing edge confidence is null.
- The five previously observed weak library-to-test edges are gone. The query
  finds zero such edges in the indexed graph, within its exclusions and parse
  coverage limits.

Two same-source cold trials had identical normalized CALLS and OVERRIDE edge
sets. Their total edge counts differed because SEMANTICALLY_RELATED edges
varied; no deterministic similarity-graph claim is made.

The current source uses direct concrete family implementations, so the real
snapshot supplies no sibling-mixin assembly example; P2 is qualified by the
dedicated fixture, not inferred from this snapshot.

## Validation

The three recorded follow-ups have been implemented in the isolated candidate.
The installed binary, client configuration, and live indexes remain unchanged.
The current validation record is in [fixes/](upstream-audit/2026-10-19/fixes/).

- All 22 compatibility probes pass after the C3 and Python import refactors.
- The portable subprocess suite passes: 57 passed and two Windows-only skips.
  A native runner probe checks the child's exact PATH. Shell fixtures retain
  distinct lookup markers for both shebang and ENOEXEC execution, unchanged
  caller environment, and exit 127 when only the child PATH contains a tool.
- Seven affected suites pass under ASan/UBSan: **1,031 passed, six platform
  skips**, no sanitizer findings. Both test and release unity objects were
  rebuilt after changing the Python resolver. The passing run uses a separate
  stable runner path after rebuilding.
- Every `scripts/test.sh --contracts-only` step passes. Nine process/artifact
  scripts pass, including 17 binary-composition assertions; production has no
  test-only seams. The git-environment contract uses the explicit audit runner.
- The configured LLVM 21 memory gate passes on the modified production files,
  including the real Python unity compilation unit. The allocation ratchet,
  no-skips policy, NOLINT whitelist, and whole-tree format checks pass.
- The canonical `scripts/lint.sh --ci` passes with cppcheck 2.20.0: all 241
  intended source files return one completion each. The initial cold parallel
  scan finished in about ten minutes, within the 45-minute CI bound. Per-run
  timings are retained in [CI evidence](upstream-audit/2026-10-19/fixes/ci-lint-result.json).
  Native `-j4` uses a persistent build directory to retain whole-program analysis. Check categories, suppressions,
  default configuration search, and source inventory are unchanged. The gate
  rejects missing/duplicate completions, nonzero exits, worker crashes and
  whole-program internal errors. Canaries prove CTU, conditional configurations,
  source/header cache invalidation and failure propagation.
- Main follow-up review returned no product-code correctness bug. Its gate
  wiring and confidence-bound suggestions were addressed; the libc PATH claim
  was checked against primary sources. The bounded follow-up review approves
  the Python extraction and gate scripts; minor assertion/documentation notes
  were addressed and affected checks rerun. See [review dispositions](upstream-audit/2026-10-19/fixes/review-dispositions.md).
  The earlier port review and its two corrected findings remain in
  [review.md](upstream-audit/2026-10-19/review.md).

The introduced C3, BFS, and guarded-import helpers are below the configured
complexity threshold of 25. Strict checks on the actual Python unity unit
have no introduced findings. There are no newly over-threshold functions
relative to v0.12.1. An unfiltered full clang-tidy sweep is still not green:
existing large upstream functions remain oversized. In particular, the
trace handler is 302 versus upstream's 278, and stale cleanup is 91 versus
86 after the ports; these are existing warnings in functions extended by
P3/P5, not unchanged complexity scores. The large Python file processor is
back to upstream's 60. The unchanged publication helper also reproduces its
errno diagnostic on pristine v0.12.1. Raw diagnostics and baseline comparison
are retained; no suppressions or thresholds were changed to hide them.

Historical results remain historical: the initial 190-suite command returned
9,726 passed, two failed, and ten platform skips. Its CLI fixture issue was
resolved with a standalone native fixture (`CBM_TEST_NATIVE_FIXTURE`), and all
362 CLI tests passed. The old shell/PATH failure is now resolved by the split
native/shell fixture above. The previous 45-minute single-process cppcheck
attempt remains failed evidence; the completed parallel gate supersedes it.
All 464 extraction tests and 13 fault-injection convention tests passed in
the earlier trial. This does not claim a new full 190-suite command was run.

One intermediate broad run was invalidated by relinking its own executable
while tests were running. A self-spawn then returned ENOENT through the
`/proc/self/exe` deleted path, and its early assertion leaked a test allocation.
That raw failure is preserved. The replacement run uses a separate stable
executable after rebuilding; no test was skipped to hide it.

## Recorded fix checklist

- [x] **Portable subprocess environment test.** Native exact-environment
  inspection and shell executable-lookup checks are separate; both shebang
  and ENOEXEC fallback paths, caller isolation, and missing-tool behavior pass.
- [x] **Introduced-helper strict lint cleanup.** C3 ordering/merge/matching,
  guarded-import collection/replay, and BFS direction selection are split into
  bounded helpers. MRO states and buffer/link sizes are named. Fail-closed
  cycles, ordering/shadowing, SQL/CSR parity and topology controls retain their
  compatibility probes. Existing upstream diagnostics are recorded separately.
- [x] **Whole-tree cppcheck gate.** Native parallel execution with persistent
  analysis data completes the unchanged inventory/check policy within the CI
  bound. Per-file completion evidence and failure canaries prevent a missing
  or crashed worker from producing a passing aggregate.

The final rebuilt release also passed a fresh private index of the same
943-file source snapshot. All 12,964 normalized CALLS/OVERRIDE edges match
the previous candidate exactly, both training-loop contract calls resolve,
and the production-library-to-tests query remains empty. The source manifest
is identical. Only SEMANTICALLY_RELATED edges differ (109 removed, 127 added);
no deterministic similarity-graph claim is made. Snapshot:
`/tmp/cbm-v0121-validation-kufyz1dy`.

## In-action MCP acceptance

A real MCP stdio client ran the release candidate side by side with the installed
server, using a fresh private cache/runtime and a Git-backed copy of the same
943-file source snapshot. All **15 live checks passed**: tool discovery, cold
indexing, searches/snippets, training and caching contract calls, three-family
SD/SDXL/SD3 dispatch topology, executable identity, saved-graph reload after a
daemon restart, and automatic file creation/edit/deletion updates. The edited
file produced its expected CALLS edge without a manual reindex.

The running frontend and daemon matched the candidate SHA-256; the installed
binary hash remained unchanged. Both private daemons stopped cleanly. A plain
folder copy needs `watch_non_git=true` for automatic updates; the successful
run used the normal Git watcher. See [live results and raw sessions](upstream-audit/2026-10-19/in-action/README.md).

## Deployment

The candidate is installed for this tested Linux/glibc environment. Existing
client applications must reconnect or restart their MCP sessions to use the new
binary. The guarded install retired the previous frontend/daemon processes.
The client configuration files were left intact (`--skip-config`). Project
graphs are rebuilt from source under semantic index version `1000009`, rather
than reusing the old generated graphs. macOS runtime CI was not run here.

Keep this branch as a small upstream-plus-patches line. On each later release,
rerun the compatibility probes against clean upstream first and drop each port
as soon as its behavior is supplied upstream.
