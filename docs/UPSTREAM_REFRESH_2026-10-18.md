# Upstream replacement audit — 2026-10-18

Decision: upstream is not yet a behavior-preserving replacement for the installed fork. Refreshing onto upstream and retaining a reduced, behavior-tested patch set is the appropriate next integration step.

For refresh actions and exact local source pointers, use the
[PORT / DROP / VERIFY ledger](../FORK_UPSTREAM_GAPS.md#refresh-summary).
The evidence below is a historical comparison against the pinned revisions;
no port has been implemented yet.

## Pinned comparison

- Fork: `fork/usable-graph-refresh`, `33efd7c21323048821b14069a5aa488a5b35780c`.
- Installed fork checksum verified: `41bf9d6983401bb5af5256ab840f37475f05f92ef04b2a33923f5742206739d5` (`--version` reports `dev`).
- Fork common upstream base: `3de05cd607f72889e89908852b1877fb08c942c1`.
- Latest stable release: v0.12.1, published 2026-10-16 23:35 UTC, commit `a7158ac2`.
- Current upstream main fetched and confirmed through GitHub API: `9dba7fe1af7b33c44127932c6aff31b173a7ddec`, 2026-10-17.
- Divergence: 8 fork-only commits, 1,596 upstream-only commits (includes merge commits).
- All 11 compared resolver, inheritance, traversal, and IPC implementation files are identical between v0.12.1 and the tested upstream main. Main-only changes concern other areas.

## Landed upstream

The previously superseded fixes remain in the shared history: #1196 (unlabelled Cypher candidates), #1273 (mode upgrades), #1284 (compound requested fields), #1287 (persisted coverage), the core #1276 weak-member fix, and Python base qualification (#1908).

New since our baseline:

- #1278 indirect overrides: merged in #2514, `c202539e`, October 8. Uses nearest declaring ancestors and explicit cycle/dedup handling. Prefer the new upstream implementation over replaying ours.
- #1277 ordinary-import typed instance fields: merged in #2498, `dffec08c`, October 4. Export is implemented through `field_types` and the cross-file fold, rather than our per-file `field_defs` export. Issue #1277 remains open; PR text explicitly excludes TYPE_CHECKING imports.
- Bare Python parameter shadowing: #1912, including `97517a46`. The new upstream implementation carries binding state through the walk and removes our old depth-limited ancestor scan. Do not replay our older implementation over it.

## Missing or requiring qualification

- #1283: sibling-base methods satisfying another base's abstract contract. Upstream's new indirect-override algorithm starts from methods declared on the assembled class and returns immediately when there are none. It does not implement our sibling-branch assembly logic.
- #1271 traversal: upstream still follows one direction for all selected edge types. Selecting CALLS and OVERRIDE does not compose caller -> contract <- implementation. Keep directional dispatch expansion if that workflow is required.
- #1286: upstream trace_path schema lacks our boolean topology edge output. Its mode="topology" selects infrastructure relationships; it is a different feature. Both issue #1286 and our documented lack of full path-instance multiplicity remain relevant.
- TYPE_CHECKING imports: ordinary Python import extraction still visits module-root siblings only, and the typed-field PR explicitly defers this feature. The fork also adds static LSP replay of recognized guards.
- Extra weak-call hardening: the completed probes and database inspection reproduce wrong test-class and nested-call targets. Carry those protections. The old `os.write` false edge is absent in both fixtures; do not carry the dotted-call tweak unless a separate current-upstream case fails.
- Daemon device renumbering: upstream still compares persisted device/inode identities without our narrowly corroborated device rebasing. Our crash-cleanup reproducer is included in the isolated test run.

## Evidence and boundaries

Git history and exact current source were inspected. Tier 2 graph discovery used the existing fork generation (2026-09-07); reported parser gaps were read directly. Upstream was not inferred from the fork's graph: its files were read directly from the fetched revision in a detached checkout.

No installed binary, client configuration, daemon, working index, or working-branch implementation was replaced. The upstream checkout contains test-only additions under `/tmp/cbm-upstream-audit-20261018`.

Audit artifacts:

- Final build log: `/tmp/cbm-upstream-audit-build-final.log` (initial dependency compilation: `/tmp/cbm-upstream-audit-build.log`)
- Test additions: `/tmp/cbm-upstream-audit-tests.patch`
- Selected tests: `/tmp/cbm-upstream-audit-tests.txt`
- Preparation script: `/tmp/cbm-prepare-audit.py`

This is a replacement assessment, not a completed rebase, production build approval, or cold sd-scripts integration acceptance.

## Primary references

- https://github.com/DeusData/codebase-memory-mcp/releases/tag/v0.12.1
- https://github.com/DeusData/codebase-memory-mcp/pull/2498
- https://github.com/DeusData/codebase-memory-mcp/pull/2514
- https://github.com/DeusData/codebase-memory-mcp/issues/1283
- https://github.com/DeusData/codebase-memory-mcp/issues/1271
- https://github.com/DeusData/codebase-memory-mcp/issues/1286

Build environment: GCC 15.3, canonical Makefile test source list and flags, with ASan/UBSan and warnings as errors. The host shell lacked zlib headers, so the final compiler invocation explicitly includes zlib development headers and library paths from the repository's evaluated Nix development environment. Dependency objects remain compiled by the same GCC 15.3 toolchain. Initial compiler-driver attempts were stopped before running tests; these are environment corrections, not upstream test failures.

The canonical test link recipe compiles all 451 translation units serially. To avoid extending the audit for that unrelated serial build cost, `/tmp/cbm-audit-parallel-build.py` compiles each source separately with the exact prefix flags from the expanded recipe and links the resulting objects in the original argument order. No source list or test selection was reduced. Vendored dependency objects are reused from the initial Makefile build. The expanded original invocation is saved in `/tmp/cbm-audit-expanded-command.json`; parallel-build and link logs are `/tmp/cbm-audit-parallel-build.log` and `/tmp/cbm-audit-link.log`.

## Integration implications

A refresh should retain behavior, not replay the eight fork commits wholesale. In particular, upstream now has an in-memory adjacency BFS as well as SQL/trail traversal; a port of our dispatch extension must cover the active memory traversal too, rather than modifying only the older SQL joins. Preserve upstream's typed-field ownership/overlay handling and nearest-ancestor override logic. Keep daemon recovery narrowly corroborated by retained inode and current filesystem identity, including the interrupted-record publication cases; do not relax identity checks generally.

## Completed regression results

The instrumented runner linked successfully. The 17 selected checks completed: **10 passed, 7 failed, 610 other tests in the selected suites deselected**. Exit status 1 is the expected compatibility-probe result. No ASan or UBSan diagnostic was emitted. Leak detection was disabled because these existing assertion-based tests return early on expected compatibility failures, before their cleanup; this run is not a leak-clean certification.

Command, from the detached upstream checkout:

```sh
CBM_TEST_ONLY_FILE=/tmp/cbm-upstream-audit-tests.txt ASAN_OPTIONS=detect_leaks=0 \
  build/audit/test-runner pipeline py_lsp store_search daemon_ipc
```

| Probe | Result | Interpretation |
| --- | --- | --- |
| Fork bare parameter-binding suppression, sequential | Pass | Old patch superseded |
| Fork bare parameter-binding suppression, parallel | Pass | Old patch superseded |
| Fork indirect override through empty intermediate | Pass | Old patch superseded |
| Fork sibling-base abstract-contract assembly | Fail: 0 override edges | Still needed |
| Fork CALLS plus reverse OVERRIDE traversal | Fail: implementation unreachable | Dispatch expansion still needed |
| Fork device-renumbered committed-socket recovery | Fail: cleanup returns -1 instead of 1 | Recovery still needed |
| Fork extra receiver safeguards, sequential | Fail: local class binds to a test class | Hardening still needed |
| Fork extra receiver safeguards, parallel | Fail: nested callable binds to unrelated method | Hardening still needed |
| Fork inherited typed-field fixture, TYPE_CHECKING imports | Fail: no run-to-process_batch edge | Guarded imports still needed |
| Same inherited typed-field fixture with ordinary imports | Pass | Use upstream core field inference |
| Older low-level field surface with relative/project-prefixed QNs | Fail: unresolved | Synthetic diagnostic; not by itself a reason to port our old suffix workaround. Upstream exports canonical field QNs through a different path, and the end-to-end ordinary-import fixture passes. |
| Upstream #1277 sequential end-to-end fixture | Pass | Core fix works for its tested contract |
| Upstream #1277 parallel end-to-end fixture | Pass | Core fix works for its tested contract |
| Upstream nearest-declaring-ancestor override fixture | Pass | Prefer upstream implementation |
| Upstream Python intermediate-base override fixture | Pass | Prefer upstream implementation |
| Upstream cross-file typed-field resolver fixture | Pass | Core fix works for its tested contract |
| Upstream shared-registry typed-field fixture | Pass | Covered ownership contract passed |

Direct inspection of the failed fixture databases confirms actual wrong targets, rather than newly supported nested nodes:

- Sequential `caller.local_constructor` -> `tests.test_targets.MockAccelerator` (`unique_name`, confidence 0.75).
- Sequential `caller.nested_call` -> `worker.WorkerProcessor.process_batch` (`unique_name`, confidence 0.38).
- Parallel `caller.local_constructor` -> `tests.test_targets.TestOnlyAccelerator` (`unique_name`, confidence 0.38).
- Parallel `caller.train` -> `targets.OtherProcessor.process_batch` (`suffix_match`, confidence 0.28), although its callee is the nested local `process_batch`.
- Neither fixture has the old false os.write -> project write edge. Do not carry the dotted-call tweak automatically: the tested os.write case is now handled upstream through other resolver changes.

The six failed behavioral probes and source-level trace schema difference establish that a direct replacement loses existing behavior. The seventh failure is the old synthetic field-surface case, not an additional demonstrated end-to-end blocker. Full cold sd-scripts indexing, all-suite gates, and production swap validation were not run because direct compatibility already fails.

Recommended retained work is five groups: (1) guarded static imports, adapting field/annotation handling only as needed; (2) sibling-mixin assembly; (3) dispatch expansion plus trace edge output; (4) the reproduced nested/local and weak test-target protections; (5) daemon device-renumbering recovery. Discard the superseded parameter-shadowing, indirect-override, and core typed-field patches; independently verify any remaining name-normalization code before carrying it.

Raw output: `/tmp/cbm-upstream-audit-results.log`. Wrong-edge evidence: `/tmp/cbm-upstream-audit-false-edges.json`.

## Durable repository evidence

The following artifacts are stored beside this report for the next refresh;
the `/tmp` build paths above describe the original run and may be removed later.

- [Test-only comparison patch](upstream-audit/2026-10-18/tests.patch): apply to clean upstream `9dba7fe1`; it adds audit-prefixed probes and an ordinary-import control, without changing product code. This is not the implementation patch to port.
- [Selected tests](upstream-audit/2026-10-18/selected-tests.txt): the 17 strict suite/test selectors used in the run.
- [Raw regression output](upstream-audit/2026-10-18/results.log): complete pass/fail and pipeline output.
- [Wrong-edge database observations](upstream-audit/2026-10-18/false-edges.json): source/target qualified names and stored resolution properties.

After applying the comparison patch and building the runner in the clean
checkout, set `CBM_TEST_ONLY_FILE` to the absolute path of the preserved
`selected-tests.txt` and rerun the command above. Failure before porting is
expected. A successful refresh needs new acceptance for P1–P5, including public
MCP trace output and complete daemon publication/rejection cases not covered by
the first failing audit assertion.
