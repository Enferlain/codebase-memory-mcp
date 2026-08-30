# Local Fork Changes Not Yet in Upstream

Snapshot: 2026-08-30

- Refresh branch: `fork/usable-graph-refresh`
- Base: `upstream/main` at `3de05cd6`
- Preserved legacy branch: `fork/usable-graph` at `4df2afe2`

This branch starts from current upstream and carries only behavior that is still
missing there. The legacy branch remains available for comparison; it should not
be merged or replayed wholesale.

## Local graph-reliability behavior

The refresh ports the still-open portions of local commit `76fb8ef1` and the
receiver-resolution portion of `4df2afe2`:

| Local behavior | Upstream tracking |
| --- | --- |
| Export Python instance-field types and consume them across files | [#1277](https://github.com/DeusData/codebase-memory-mcp/issues/1277) |
| Recover an imported cross-file annotation only when its short type name is unique | [#1277](https://github.com/DeusData/codebase-memory-mcp/issues/1277) |
| Extract imports nested under `TYPE_CHECKING` guards | Follow-up requested in [#1277](https://github.com/DeusData/codebase-memory-mcp/issues/1277) |
| Discover `OVERRIDE` through intermediate ancestor classes | [#1278](https://github.com/DeusData/codebase-memory-mcp/issues/1278) |
| Model a sibling Python base method satisfying an abstract sibling contract | [#1283](https://github.com/DeusData/codebase-memory-mcp/issues/1283) |
| Compose `CALLS` traversal with reverse `OVERRIDE` traversal | [#1271](https://github.com/DeusData/codebase-memory-mcp/issues/1271) |
| Optionally expose every traversed edge with `trace_path(topology=true)` | [#1286](https://github.com/DeusData/codebase-memory-mcp/issues/1286) |

The topology option preserves the traversed edge set, but does not yet represent
separate path instances when the same edge participates in multiple converging
paths. The eventual upstream #1286 contract may supersede it.

Primary binding tests:

- `pipeline_python_crossfile_field_resolves_inherited_contract_method`
- `pylsp_exports_and_consumes_crossfile_instance_fields`
- `explicit_override_walks_empty_intermediate`
- `explicit_override_models_python_sibling_mixin`
- `store_bfs_composes_calls_with_reverse_override`

## Deliberately not replayed from the legacy branch

The old nested-local-call implementation in `4df2afe2` repeatedly calls
`ts_node_parent()`. Upstream later demonstrated that this pattern can become
cubic on deeply nested Python. Do not restore that implementation unchanged.
This refresh instead includes the three maintainer-authored commits from
`upstream/feat/python-bare-local-binding`. They suppress weak matches for bare
calls shadowed by parameters using the unified walk cursor and a fail-open depth
bound; they are not yet part of `upstream/main` at this snapshot.

Bare Python base-class qualification from `4df2afe2` is superseded by upstream
PR [#1908](https://github.com/DeusData/codebase-memory-mcp/pull/1908), which landed
as `2910e284`.

## Local commits superseded by upstream

Do not port these legacy commits:

| Local commit | Upstream result |
| --- | --- |
| `9476c73c` — suppress weak Python member calls | Merged through [#1903](https://github.com/DeusData/codebase-memory-mcp/pull/1903) |
| `8c03a590` — rebuild when a requested mode adds capabilities | Merged through [#1263](https://github.com/DeusData/codebase-memory-mcp/pull/1263) |
| `d64596da` — scan all unlabeled Cypher candidates | Merged through [#1323](https://github.com/DeusData/codebase-memory-mcp/pull/1323) |
| `95336c63` — preserve compound requested fields | Merged through [#1325](https://github.com/DeusData/codebase-memory-mcp/pull/1325) |
| `be4c1655` — preserve persisted coverage summaries | Merged through [#1326](https://github.com/DeusData/codebase-memory-mcp/pull/1326) |

Upstream also includes the second #1196 fix through PR #1698. Updating the base
therefore gains that behavior without a local patch.

## Refresh verification

Before using or publishing a refreshed build:

1. Run the relevant sanitized C suites.
2. Build the production binary from this branch.
3. Full-index `sd-scripts` into a disposable project/cache.
4. Recheck cross-file receiver inference, indirect and sibling overrides,
   polymorphic trace composition, topology output, and false-positive Python
   calls.
5. Install the binary only after those probes pass.
