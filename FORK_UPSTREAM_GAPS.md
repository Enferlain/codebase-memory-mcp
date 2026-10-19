# Graph Reliability Status

This is the living reliability ledger for the locally installed fork. Update it
when the fork is rebased, an upstream issue changes state, a local patch is
removed, or a new reproducer is confirmed.

Installed upstream-plus-fixes build: **2026-10-19**. Historical fork baseline:
**2026-09-07**; upstream replacement audit: **2026-10-18**.

The tested v0.12.1 plus P1-P5 build is now installed. The canonical checkout is
`/home/imi/Projects/codebase-memory-mcp` on `fork/v0.12.1-patches`; the separate
`codebase-memory-mcp-v0121` worktree has been removed. The old fork branch remains
in Git history and on origin. No binary or index backup was created, as requested.

- Installed version: `0.12.1-fork.1`
- Tested source/acceptance commit: `f0379962`
- Installed binary: `/home/imi/.local/bin/codebase-memory-mcp`
- Installed SHA-256: `4962cf320c2345cf47b7871fd5f1a14107aecf13e77a86b1ffd35689888ea026`
- Refresh base: release **v0.12.1** (`a7158ac2`), with metadata-only corrections
  from `9dba7fe1`; not a claim of synchronization with later upstream main.

See [acceptance and deployment results](docs/UPSTREAM_MIGRATION_2026-10-19.md)
and [client setup](LOCAL_MCP_CLIENT_SETUP.md). The audit and source map below
retain their historical comparison against old fork `33efd7c2`; their old commit
labels describe that audit, not the currently installed build.

## Refresh action labels

These actions compare installed fork `33efd7c2` with v0.12.1 / `9dba7fe1`.
They record the original porting decisions; the trial report above records implementation.

- **PORT**: upstream still misses tested behavior supplied by this fork. Adapt
  the relevant code and regressions to the new implementation.
- **DROP**: upstream supersedes the old implementation. Keep useful regression
  coverage, but do not replay the old fix.
- **VERIFY**: available evidence does not justify carrying the old workaround.
  Require an end-to-end failure on the refresh target first.
- **CANDIDATE**: an unresolved limitation, not an implemented fix to port.

See [the durable audit and regression results](docs/UPSTREAM_REFRESH_2026-10-18.md)
and [the test-only comparison patch](docs/upstream-audit/2026-10-18/tests.patch).
The audit completed 17 targeted checks: 10 passed and 7 failed. Six failures
reproduce behavior gaps; the seventh exercises an older synthetic field
representation and is **VERIFY**, not an additional proven integration blocker.
Trace edge-output absence was checked in source/schema, not by a dedicated MCP
runtime probe. This is not full cold `sd-scripts` integration acceptance.

## Refresh summary

| Area | Tracking | Audited upstream behavior | Refresh action |
| --- | --- | --- | --- |
| Unlabelled-source Cypher matching | [#1196](https://github.com/DeusData/codebase-memory-mcp/issues/1196) | Already in common history via #1323 / #1698 | **DROP** old fix |
| Index mode capability upgrades | [#1273](https://github.com/DeusData/codebase-memory-mcp/issues/1273) | Already in common history via #1263 | **DROP** old fix |
| Core weak Python member-call suppression | [#1276](https://github.com/DeusData/codebase-memory-mcp/issues/1276) | Already in common history via #1903; extra local guards are split below | **DROP** core old fix |
| Ordinary-import typed Python fields | [#1277](https://github.com/DeusData/codebase-memory-mcp/issues/1277) | Merged via [#2498](https://github.com/DeusData/codebase-memory-mcp/pull/2498); ordinary-import integration tests pass; issue remains open | **DROP** old core export/fold implementation |
| Imports under `TYPE_CHECKING` | Separate follow-up to #1277 | Guarded integration fixture still fails; ordinary-import control passes | **PORT P1** |
| Relative/project-prefixed field-QN recovery | Part of local #1277 hardening | Old synthetic surface fails, but current exporter and ordinary-import integration pass | **VERIFY**, not automatic port |
| Indirect `OVERRIDE` discovery | [#1278](https://github.com/DeusData/codebase-memory-mcp/issues/1278) | Fixed via [#2514](https://github.com/DeusData/codebase-memory-mcp/pull/2514); local and upstream regressions pass | **DROP** old ancestor walk |
| Python sibling-base/mixin override assembly | [#1283](https://github.com/DeusData/codebase-memory-mcp/issues/1283) | Still missing; no assembly override edge in the probe | **PORT P2** |
| List/object requested search fields | [#1284](https://github.com/DeusData/codebase-memory-mcp/issues/1284) | Already in common history via #1325 | **DROP** old fix |
| Contract-to-implementation traversal | [#1271](https://github.com/DeusData/codebase-memory-mcp/issues/1271) | Typed declared target works in supported cases; mixed-direction dispatch expansion is still missing | **PORT P3** traversal only |
| Trace edge output | [#1286](https://github.com/DeusData/codebase-memory-mcp/issues/1286) | Our `topology=true` output is absent; upstream `mode="topology"` is infrastructure traversal | **PORT P3** edge output; full path multiplicity remains unimplemented in this fork |
| Persisted partial-parse summaries | [#1287](https://github.com/DeusData/codebase-memory-mcp/issues/1287) | Already in common history via #1326 | **DROP** old fix |
| Bare parameter-shadowed Python calls | Local fix / upstream #1912 | Sequential and parallel probes pass; upstream now carries binding state through the walk | **DROP** old parameter-scope walker |
| Nested local callable / weak test-target safeguards | Additional #1276 hardening | Sequential and parallel fixtures still contain unrelated targets | **PORT P4** |
| Dotted non-method callee guard | Additional #1276 hardening | Reproduced `os.write` case now has no false edge in either fixture | **VERIFY** other shapes; omit this tweak unless they fail |
| Filesystem device-renumbering recovery | Local operational fix | Committed-socket cleanup still returns -1 instead of 1 | **PORT P5** |
| Nested Python declaration/call coverage | Not filed | Not retested as an exhaustive coverage claim | **CANDIDATE**; local safeguard does not implement nested graph coverage |

## Porting checklist and source map

These five behavior groups are implemented and pass the focused regressions
on the trial branch. The checkboxes denote behavior qualification; remaining
whole-repository and host limits are recorded separately in the trial report.

| Group | Local source to consult | Binding regression / acceptance |
| --- | --- | --- |
| [x] **P1 — guarded static imports** | `internal/cbm/extract_imports.c`: `process_python_type_checking_block`; `internal/cbm/lsp/py_lsp.c`: `py_is_type_checking_if`, guarded import classification/replay | `pipeline_python_crossfile_field_resolves_inherited_contract_method` must resolve the declared contract with guarded imports, not just ordinary imports. Also retain `py_type_checking_guard` / `py_qualified_type_checking_guard` extraction cases. |
| [x] **P2 — sibling mixins** | `src/pipeline/pass_semantic.c`: `override_python_sibling_mixins`, `collect_branch_methods` and supporting method/abstract checks | `explicit_override_models_python_sibling_mixin`: implementation -> abstract contract with `python_mro_sibling` / assembly metadata. Preserve upstream nearest-ancestor and cycle/dedup behavior. |
| [x] **P3 — dispatch and trace edges** | `src/store/store.c`: mixed-direction OVERRIDE joins; `src/mcp/mcp.c`: dispatch composition and `bfs_topology_json` / `bfs_topology_table` | `store_bfs_composes_calls_with_reverse_override`, then public `trace_path` checks in both directions with `CALLS` + `OVERRIDE`, pagination, test filtering, and edge metadata. Add explicit MCP output coverage; the audit only source-checked the topology schema. |
| [x] **P4 — false-target safeguards** | `internal/cbm/extract_calls.c`: nested-function binding detection; `src/pipeline/registry.c`: `cbm_suppress_weak_test_target`; matching guards in `pass_calls.c`, `pass_parallel.c`, declarations in `pipeline.h` | `pipeline_python_receiver_suppresses_weak_method_edge` and `pipeline_python_receiver_parallel_suppresses_weak_method_edges`, plus test-target unit controls. Keep genuine imported/test and local helper calls. |
| [x] **P5 — daemon recovery** | `src/daemon/ipc.c`: `posix_socket_record_rebase_device` and both committed/temporary-publication recovery call sites | `daemon_ipc_posix_crash_cleanup_after_device_renumbering` and the renumbered cases in `daemon_ipc_posix_record_publication_windows_recover_from_crash`; preserve inode/ctime/device-conflict rejection and ownership/lock checks. |

Porting boundaries:

- **P1:** use upstream's typed-field export/fold and sealed-registry ownership
  handling. Do not replay `py_export_instance_fields`, `py_parse_field_defs`, or
  the old `field_defs` transfer just to add guarded imports. Treat QN-normalizing
  helpers as **VERIFY** until a current-exporter end-to-end case requires them.
- **P2:** add assembly handling around upstream's new nearest-declaring-ancestor
  matcher; do not replace it with our old ancestor traversal. The local helper
  is a reference for the tested assembly case, not proof of complete Python MRO.
- **P3:** upstream now has in-memory adjacency BFS and SQL/trail paths. Changing
  only our old SQL joins is insufficient. Edge output preserves the fork's
  partial topology contract; it does not provide full path instances or
  multiplicity, which remain outstanding work under #1286.
- **P4:** preserve upstream's carried parameter-binding state and external-import
  fixes. Add nested-definition protection to that mechanism instead of restoring
  our bounded ancestor scan. The wrong targets were confirmed in the persisted
  fixture databases, not inferred from names alone.
- **P5:** port the complete identity-rebasing fix, including interrupted record
  publication. The latest audit reproduced failure in the committed-socket case;
  all remaining recovery/rejection cases must be rerun after porting.

### Commit handling

| Local commit(s) | Handling on the new branch |
| --- | --- |
| `6fa0d1b6` | **Mixed:** extract P1, P2, P3. Core field export and indirect overrides are superseded. Do not cherry-pick whole commit. |
| `96455ff1`, `1e636fcf`, `2c8dc3d4` | **DROP** old parameter-shadowing implementation and formatting-only replay. Keep useful tests adapted to upstream's stateful walk. |
| `3814d323` | **Mixed:** extract P4. Field-QN helpers and dotted-call tweaks are **VERIFY**. Do not cherry-pick whole commit. |
| `33efd7c2` | **PORT P5**, adapted to current IPC code; its old doc snapshot is superseded by this ledger. |
| `49e015ce`, `e862b000` | Historical documentation; use this updated ledger and the separate client setup guide. |

The upstream issues are grouped under the maintainer's umbrella issues
[#391](https://github.com/DeusData/codebase-memory-mcp/issues/391),
[#592](https://github.com/DeusData/codebase-memory-mcp/issues/592), and
[#594](https://github.com/DeusData/codebase-memory-mcp/issues/594).

## PORT P5 — local daemon recovery after device renumbering

**2026-10-18:** the committed-socket reproducer still fails on audited upstream
(cleanup returns -1 instead of 1). The details and validation below describe
the installed fork, not a completed port.

Confirmed on 2026-09-07: stale socket metadata recorded device `8:48`, but
its retained socket inode now lived on device `8:80`. The installed build
recovered from a normal daemon crash, but changing only the persisted device
number reproduced the 30-second startup failure. This is consistent with a
WSL filesystem remount; the available logs do not establish why the original
daemon exited.

Recovery now rebases a persisted device number onto the record's current
filesystem only when the retained anchor has the recorded inode. Startup and
lifetime locks, ownership/mode checks, hard-link shape, committed socket ctime,
and current-device unlink checks remain enforced. Unknown or mismatched socket
identities still block recovery.

Regression: `daemon_ipc_posix_crash_cleanup_after_device_renumbering` covers a
committed socket, interrupted cleanup, pending publication before/after linking,
and rejection of mismatched inode/ctime or conflicting record devices. The
publication-window regression also covers remounts at all four temporary-record
crash boundaries. Malformed identities remain rejected.

Validation: the regression fails against the pre-fix source; all 44 IPC tests
pass under AddressSanitizer/UndefinedBehaviorSanitizer with warnings treated as
errors. The final production binary survived two simulated device renumberings
and three MCP handshakes in an isolated runtime. Required code review approved
the core fix; its temporary-publication finding is covered by the follow-up
above. Installed using `install --force --skip-config -y`, preserving indexes
and client configuration.


## Detailed behavior status

### #1277 — DROP core field fix; PORT P1 guarded imports; VERIFY QN workarounds

Upstream [#2498](https://github.com/DeusData/codebase-memory-mcp/pull/2498)
now carries annotated instance-field types across files through `field_types`
and the cross-file fold. Both upstream end-to-end fixtures and the ordinary-import
version of our inherited-field fixture pass. The old fork field-export path is
superseded even though issue #1277 remains open.

The guarded version of our fixture still fails. The fork's recognized
`TYPE_CHECKING` import extraction and LSP replay remain **P1**; keep this work
separate from the upstream core field fix, as requested by the maintainer.

`pylsp_crossfile_field_accepts_project_prefixed_registry_qns` fails with the old
synthetic relative-QN input. That does not prove the current upstream exporter
needs the fork's suffix/unique-short-name workaround. Require a failing case
using current extraction before porting those helpers. The old
`pylsp_exports_and_consumes_crossfile_instance_fields` test also assumes the
fork's export representation; adapt assertions to upstream's public behavior.

Binding integration test:
`pipeline_python_crossfile_field_resolves_inherited_contract_method` (guarded
case fails, ordinary-import control passes on audited upstream).

### #1278 — DROP local indirect-override implementation

Upstream [#2514](https://github.com/DeusData/codebase-memory-mcp/pull/2514)
walks to nearest declaring ancestors with cycle protection and deduplication.
`explicit_override_walks_empty_intermediate` and both selected upstream override
fixtures pass. Preserve upstream's matcher; only **P2** remains to add around it.

### #1283 — PORT P2 multiple inheritance and sibling mixins

Upstream's new ancestor matcher starts from methods declared on the assembled
class and produces no relation for our empty assembly combining a concrete
sibling method with another sibling's abstract contract. The fork models this
assembly case; the audited upstream probe produces zero override edges.

Binding test: `explicit_override_models_python_sibling_mixin`.

### #1271 — PORT P3 dispatch traversal; DROP superseded declared-target fixes

The original failure stored a low-confidence call to one arbitrary concrete
strategy. Upstream has fixed that core weak-member behavior and now supports
the typed declared target for ordinary-import field fixtures. Guarded imports
still need **P1**. The remaining traversal gap is composition:

```text
caller -CALLS-> contract <-OVERRIDE- possible implementations
```

The traversal marks the dispatch expansion with Python sibling/MRO metadata,
so possible runtime implementations remain distinguishable from the declared
call target. The static graph still cannot select the runtime family.

Binding test: `store_bfs_composes_calls_with_reverse_override`.

## Partially addressed issue

### #1286 — PORT P3 trace edge output; path multiplicity remains unimplemented

Audited upstream lacks the fork's boolean `topology` output option. Its new
`mode="topology"` follows infrastructure relationships and does not replace
this output contract. This absence was verified from the tool schema/source;
a dedicated public MCP regression is still needed for the port.

`trace_path(topology=true)` in this fork returns the traversed edge set, including
source, target, edge type, and dispatch metadata. That is enough to tell which
strategy implementations were reached and how.

It is not a full path-instance representation. When paths converge, the same
edge is deduplicated and the response does not preserve every predecessor chain
or multiplicity. Do not interpret its node list as one ordered path. Until
#1286 is implemented upstream, use the topology edges or an explicit
`query_graph` query when exact route identity matters.

## Closed upstream issues retained in the baseline

These old local patches are superseded and must not be replayed during the next
upstream refresh:

| Old local commit | Upstream replacement |
| --- | --- |
| `9476c73c` — suppress weak Python member calls | [#1903](https://github.com/DeusData/codebase-memory-mcp/pull/1903) |
| `8c03a590` — rebuild when mode adds capabilities | [#1263](https://github.com/DeusData/codebase-memory-mcp/pull/1263) |
| `d64596da` — scan all unlabeled Cypher candidates | [#1323](https://github.com/DeusData/codebase-memory-mcp/pull/1323), completed by [#1698](https://github.com/DeusData/codebase-memory-mcp/pull/1698) |
| `95336c63` — preserve compound requested fields | [#1325](https://github.com/DeusData/codebase-memory-mcp/pull/1325) |
| `be4c1655` — preserve persisted coverage summaries | [#1326](https://github.com/DeusData/codebase-memory-mcp/pull/1326) |

Bare Python base-class qualification from the old branch is also superseded by
upstream [#1908](https://github.com/DeusData/codebase-memory-mcp/pull/1908).

### Additional #1276 hardening — split PORT P4 from VERIFY

**PORT P4:** the latest fixture databases still show both wrong-target classes:

- A locally declared constructor binds to an unrelated class under `tests`
  through `unique_name` (sequential confidence 0.75; parallel 0.38).
- A nested local `process_batch` binds to an unrelated project method through
  `unique_name` (sequential 0.38) or `suffix_match` (parallel 0.28).

Use the existing sequential/parallel receiver regression fixtures and
`weak_test_target_suppress_is_namespace_and_confidence_bounded`. Do not remove
strong explicit imports from tests or real helper calls.

**VERIFY / omit unless reproduced:** the old dotted-callee workaround. Both
latest fixture databases have no false `os.write` -> project `write` edge;
upstream's other resolver changes handle that tested case. This evidence does
not establish every possible dotted-call shape is fixed.

These are general resolver safeguards. No upstream issue was reopened or new
follow-up filed by this audit.

## CANDIDATE: nested Python function graph coverage

The full extraction-coverage question was not revalidated in the 2026-10-18
audit. It is separate from the reproduced false-target bug in **P4**, and is
not an implemented fix available to port.

During the historical `sd-scripts` validation, the inner function `tokenize_fn` in
`_maybe_cache_epoch_tokens` was not represented as its own graph node. Its call
to `strategies.tokenize_captions(...)` therefore cannot appear as
`tokenize_fn -> tokenize_captions`.

The local nested-binding guard prevents the name `process_batch` inside another
nested function from being falsely linked to an unrelated project method. It
does not index nested declarations or their call edges.

Before filing:

1. Reduce this to a two-function Python fixture with one nested declaration.
2. Confirm the nested symbol is absent after a cold full index of upstream main.
3. Check whether omission of nested declarations is documented/indexer policy.
4. If it is intended to be indexed, file a separate extraction-coverage issue;
   do not fold it into #1276 or #1277.

Current workaround: inspect the containing function's source with
`get_code_snippet` when a local callback or closure is material to the trace.

## Historical installed-fork validation baseline (2026-08-31)

A cold full index of `sd-scripts` was run against commit `3814d323` on
2026-08-31. It produced 12,782 nodes and 60,869 edges. The persisted artifact is
`/home/imi/Projects/sd-scripts/.codebase-memory/graph.db.zst`.

Confirmed behavior:

- `_run_epoch_steps` calls the base
  `DiffusionTrainingStrategy.process_batch` with `strategy=lsp_method` and
  `confidence=0.90`.
- A two-hop `CALLS` + `OVERRIDE` trace reaches the SD, SDXL, and SD3
  implementations and returns topology edges with dispatch metadata.
- `_maybe_cache_epoch_tokens` calls the base
  `CachingStrategy.get_token_cache_encoder_names` with the same strong method
  resolution.
- Explicit `OVERRIDE` queries find the family implementations for
  `process_batch`, `tokenize_captions`, and
  `get_token_cache_encoder_names`.
- `trace_path("process_batch")` returns an ambiguity response with five
  candidates rather than silently selecting one.
- Production-to-test `CALLS` edges and the reproduced nested-local false edge
  both return zero rows.
- A separate 40-node/107-edge fixture preserved the intended typed contract
  call without introducing unrelated weak edges.
- Coverage metadata reported no recorded issue for the six material source
  paths. Existing partial-parse ranges in `dashboard_extras.html` and
  `run_benchmark.ps1` were unrelated to these checks.

The production `-Werror` build and focused/end-to-end tests passed. The optional
external review service reached its 900-second timeout without a verdict; that
is recorded here so it is not later mistaken for a completed review.

## Operational observations, not current product issues

- `index_repository(mode=full)` may reuse an unchanged generation. For a true
  cold validation, use a disposable cache/project or deliberately remove the
  disposable project before indexing; do not delete a working graph casually.
- The daemon intentionally rejects a client built from different bytes. Stop
  all MCP clients and the daemon before replacing the installed fork binary.
- CLI list arguments are repeated flags, for example
  `--edge-types CALLS --edge-types OVERRIDE`; a JSON-looking string supplied to
  one flag is one literal value.
- One stale rendezvous/socket was encountered during forced live-binary
  replacement. It has not been reduced or reproduced independently, so it is
  not yet tracked as a defect.

## Refresh checklist

For the audited v0.12.1 / `9dba7fe1` refresh, start with P1–P5 and the **DROP** /
**VERIFY** labels above. For a later target, recheck these decisions against it.

1. Pin the target in a separate branch/worktree; fetch/recheck upstream again if
   moving beyond the audited revision.
2. Use the durable test-only comparison patch and selection list to reproduce
   the baseline; adapt only P1–P5, with the boundaries above.
3. Remove local fixes that have genuinely landed, including their redundant
   tests only when equivalent upstream binding tests exist.
4. Run the relevant sanitized C suites and the production build.
5. Cold-index `sd-scripts` in a disposable project/cache.
6. Repeat receiver inference, indirect/sibling overrides, polymorphic trace,
   topology, ambiguity, false-positive, and coverage probes.
7. Update this document's commit, checksum, states, remaining limitations, and
   validation counts.
8. Replace the installed binary only through the coordinated restart procedure
   in [LOCAL_MCP_CLIENT_SETUP.md](LOCAL_MCP_CLIENT_SETUP.md).
