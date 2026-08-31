# Graph Reliability Status

This is the living reliability ledger for the locally installed fork. Update it
when the fork is rebased, an upstream issue changes state, a local patch is
removed, or a new reproducer is confirmed.

Last verified: **2026-08-31**

- Working branch: `fork/usable-graph-refresh`
- Validated code commit: `3814d323`
- Upstream base: `upstream/main` at `3de05cd6`
- Preserved comparison branch: `fork/usable-graph` at `4df2afe2`
- Installed binary: `/home/imi/.local/bin/codebase-memory-mcp`
- Installed SHA-256: `0c261f40a546a64148a40938b588a05394b64d94b45ad2367f9b2e81b8fdc419`

The working branch starts from current upstream and carries only behavior still
needed locally. Do not merge or replay the old comparison branch wholesale.
See [LOCAL_MCP_CLIENT_SETUP.md](LOCAL_MCP_CLIENT_SETUP.md) for installing this
exact binary into other agent environments.

## Status meanings

- **Upstream**: merged into the upstream base and no local patch is needed.
- **Fork-fixed**: still open or incomplete upstream, but covered by this branch.
- **Partial**: the fork improves the normal workflow but does not implement the
  complete requested contract.
- **Candidate**: independently observed, but not yet reduced and filed as its
  own upstream issue.

## Summary

| Area | Tracking | Upstream state | This fork | Remaining work |
| --- | --- | --- | --- | --- |
| Unlabelled-source Cypher matching | [#1196](https://github.com/DeusData/codebase-memory-mcp/issues/1196) | Closed via [#1323](https://github.com/DeusData/codebase-memory-mcp/pull/1323) and [#1698](https://github.com/DeusData/codebase-memory-mcp/pull/1698) | Upstream | None known |
| Index mode capability upgrades | [#1273](https://github.com/DeusData/codebase-memory-mcp/issues/1273) | Closed via [#1263](https://github.com/DeusData/codebase-memory-mcp/pull/1263) | Upstream | None known |
| Weak Python member-call false positives | [#1276](https://github.com/DeusData/codebase-memory-mcp/issues/1276) | Closed via [#1903](https://github.com/DeusData/codebase-memory-mcp/pull/1903) | Upstream plus local hardening | Recheck the extra dotted-call cases before proposing a follow-up |
| Cross-file Python receiver fields | [#1277](https://github.com/DeusData/codebase-memory-mcp/issues/1277) | Open | Fork-fixed | Upstream implementation/PR |
| Indirect `OVERRIDE` discovery | [#1278](https://github.com/DeusData/codebase-memory-mcp/issues/1278) | Open | Fork-fixed | Upstream implementation/PR |
| Sibling-base/mixin override assembly | [#1283](https://github.com/DeusData/codebase-memory-mcp/issues/1283) | Open | Fork-fixed | Upstream implementation/PR |
| List/object requested fields in `search_graph` | [#1284](https://github.com/DeusData/codebase-memory-mcp/issues/1284) | Closed via [#1325](https://github.com/DeusData/codebase-memory-mcp/pull/1325) | Upstream | None known |
| Polymorphic call representation and traversal | [#1271](https://github.com/DeusData/codebase-memory-mcp/issues/1271) | Open | Fork-fixed for the tested contract/override path | Upstream implementation and broader language coverage |
| `trace_path` topology/path multiplicity | [#1286](https://github.com/DeusData/codebase-memory-mcp/issues/1286) | Open | Partial | Distinct path instances and multiplicity are not represented |
| Persisted partial-parse summaries | [#1287](https://github.com/DeusData/codebase-memory-mcp/issues/1287) | Closed via [#1326](https://github.com/DeusData/codebase-memory-mcp/pull/1326) | Upstream | None known |
| Nested Python function graph coverage | Not filed | Candidate | Guarded against false binding only | Model nested declarations and their calls |

The upstream issues are grouped under the maintainer's umbrella issues
[#391](https://github.com/DeusData/codebase-memory-mcp/issues/391),
[#592](https://github.com/DeusData/codebase-memory-mcp/issues/592), and
[#594](https://github.com/DeusData/codebase-memory-mcp/issues/594).

## Fork-fixed open issues

### #1277 — cross-file receiver-type inference

Upstream loses Python instance-field types when a typed object crosses a file
boundary. The fork exports those field types through the cross-file LSP
contract, consumes them at imported call sites, and bridges an imported
annotation to a project-qualified type only when the short name is unique.

The fork also extracts imports nested under `TYPE_CHECKING` guards. The
maintainer explicitly requested that nested-import extraction be kept separate
from #1277 if it is pursued upstream; it has not been filed separately yet.

Binding tests:

- `pipeline_python_crossfile_field_resolves_inherited_contract_method`
- `pylsp_exports_and_consumes_crossfile_instance_fields`

### #1278 — indirect overrides

Upstream compares methods on direct class/base pairs and can miss an override
inherited through an intermediate class. The fork walks the ancestor chain and
keeps the implementation-to-contract `OVERRIDE` direction consistent.

Binding test: `explicit_override_walks_empty_intermediate`.

### #1283 — multiple inheritance and sibling mixins

Upstream reasons about each `INHERITS` edge independently, so a method supplied
by one sibling base may not be recognized as satisfying the abstract contract
from another sibling base. The fork models this Python assembly case.

Binding test: `explicit_override_models_python_sibling_mixin`.

### #1271 — conservative polymorphic calls

The original failure stored a low-confidence call to one arbitrary concrete
strategy. The fork resolves the typed call to its declared base contract, then
allows outbound tracing to compose:

```text
caller -CALLS-> contract <-OVERRIDE- possible implementations
```

The traversal marks the dispatch expansion with Python sibling/MRO metadata,
so possible runtime implementations remain distinguishable from the declared
call target. The static graph still cannot select the runtime family.

Binding test: `store_bfs_composes_calls_with_reverse_override`.

## Partially addressed issue

### #1286 — topology and path identity

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

The fork retains narrower hardening around #1276:

- a dotted Python callee is treated as receiver-like even when extraction did
  not set `is_method`;
- weak production-to-explicit-test targets are rejected; and
- a proven nested local callable suppresses unrelated short-name matches.

These rules are general resolver safeguards, not `sd-scripts` name checks. They
need a small upstream-main reproducer before deciding whether #1276 should be
reopened or a follow-up issue should be filed.

## Candidate issue: nested Python functions are absent from the graph

During the final `sd-scripts` validation, the inner function `tokenize_fn` in
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

## Validation baseline

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

When pulling a newer upstream:

1. Fetch upstream and recheck every linked issue and PR.
2. Diff upstream behavior before replaying any local patch.
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
