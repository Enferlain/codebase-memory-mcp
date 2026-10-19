# v0.12.1 selective migration trial

Candidate: `fork/v0.12.1-patches` in
`/home/imi/Projects/codebase-memory-mcp-v0121`, based on release `v0.12.1`
(`a7158ac2`). Binary: `build/release/codebase-memory-mcp`, version
`0.12.1-fork.1`. This is an isolated trial: the installed binary, client
configuration, and live indexes have not been replaced.

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

Final validation is in progress. Completed checks:

- 22 focused regressions and all 464 extraction tests pass under ASan/UBSan,
  with no sanitizer report.
- Every `scripts/test.sh --contracts-only` step passes.
- Nine process/artifact scripts pass, including parent-death cleanup, worker
  error transport, watcher disabling, request scope, TLS size, and 17 binary
  composition assertions. The release contains no test-only seams.
- The canonical clang-analyzer memory gate passes with the LLVM 21 analyzer.
- CLI: all 362 tests pass with the standalone native fixture.
- Required code review returned; both actionable findings were corrected.
  See [review and dispositions](upstream-audit/2026-10-19/review.md).

The original broad run is preserved as failed evidence. Its long PATH broke
Git fixture discovery; the rerun uses a compact PATH. One upstream extraction
test expected nested definitions to be unbound; its expectation now matches
the intentional P4 safeguard. On this Nix host, `/bin/true` is a multicall
executable that fails after staging renames it; the CLI test harness now
accepts a standalone native fixture through `CBM_TEST_NATIVE_FIXTURE`, with
all update/lease/staging checks retained. No product behavior was changed for
that fixture correction.

One subprocess regression still fails on this host: its script assumes
`/bin/sh` preserves PATH exactly, but the Nix `wrapped-sh` executable changes
it. The same selected test fails on unmodified upstream `9dba7fe1`; that test
body is identical in v0.12.1. A direct `/bin/sh` probe also reproduces the
PATH rewrite independently of CBM. This is a recorded host qualification
limit, not a passing test, and it has not been skipped or weakened.

The host cppcheck 2.18.3 produced parser errors and other diagnostics, so the
final lint run uses the CI-pinned cppcheck 2.20.0 and clang-format 20.1.8.
The memory analyzer uses clang-tidy 21.1.8 with `CC=clang` for Makefile flag
selection; the GCC-only warning flags otherwise prevent analysis. The initial
empty analyzer result with GCC flags is not counted as valid evidence.

An additional, noncanonical clang-tidy sweep is **not clean**: the touched
functions include existing complexity warnings, and the new C3 helpers also
exceed the configured complexity threshold and retain style/magic-number
findings. Missing braces in the added code were corrected. Directly analyzing
`py_lsp.c` alone is invalid because it is a unity member; its standalone
diagnostics are not counted as confirmed runtime defects. The canonical
memory gate and sanitizer integration checks are the applicable evidence.
This trial does not claim a clean full clang-tidy sweep.

Raw cold-index responses, selected tests, focused results, process results,
and checksums are under [the trial evidence](upstream-audit/2026-10-19/).

## Moving the trial into service

After the remaining validation passes, keep a backup of the installed binary,
use the repository's guarded installer, and coordinate a restart of all MCP
clients so the daemon and clients run identical bytes. Retain the old fork
branch for rollback. The new semantic version requires a fresh index; an old
graph must not be reused as evidence of the port's behavior.

Keep this branch as a small upstream-plus-patches line. On each later release,
rerun the compatibility probes against clean upstream first and drop each port
as soon as its behavior is supplied upstream.
