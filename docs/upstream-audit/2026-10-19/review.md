Final disposition: both actionable findings in this snapshot review are corrected and regression-checked. The critical P4 section below describes earlier code; see the commit-linked dispositions at the end.

I have a complete picture now. Here's my review.

---

# Review: selective migration to upstream v0.12.1

## 🔴 Critical — P4 gate is commented out in the parallel venue (`src/pipeline/pass_parallel.c:3356-3359`)

The new suppression call was pasted **inside a block comment**:

```c
            /* Import-binding suppression (#2127) — see pass_calls.c ||
        cbm_suppress_weak_test_target(lang == CBM_LANG_PYTHON, source_node->qualified_name,
                                      res.qualified_name, res.strategy); this gate
             * MUST stay identical to the one there. */
```

Everything from `see pass_calls.c ||` through `res.strategy); this gate` is comment text — the call never executes. Consequences:

1. **P4 test-target protection is not applied on the parallel path at all.** Weak short-name resolutions from production code into `tests.*` namespaces still emit edges there.
2. **Sequential/parallel divergence**, exactly what every surrounding comment forbids ("this gate MUST stay identical to the one there" — MT determinism). Large corpora run the parallel venue, so the same repo indexed via the two routes yields different graphs.
3. The audit-fixture test `audit_pipeline_python_receiver_parallel_suppresses_weak_method_edges` (`tests/test_pipeline.c:23721`, `unknown_constructor → TestOnlyAccelerator` defined only under `tests/`) is annotated "only the test-target guard can reject the weak guess" — it should be failing right now, which is consistent with "tests in progress", but the port is incomplete.

**Fix:** restore the comment and add the clause to the `drop_plain_call` expression, mirroring `pass_calls.c:1130`:

```c
            cbm_suppress_weak_local_binding_call(...) ||
            cbm_suppress_weak_test_target(lang == CBM_LANG_PYTHON, source_node->qualified_name,
                                          res.qualified_name, res.strategy) ||
            (lang == CBM_LANG_PYTHON && cbm_suppress_weak_import_bound_call(...));
```

The `pass_calls.c` side itself is correct: the clause sits inside `drop_plain_call`, so only the plain-CALLS fall-through is suppressed and service/route classification is unaffected.

## 🟠 Medium — missing null guard on `consequence` in `py_lsp.c`

Two new sites call `ts_node_named_child_count(body)` without checking `body`:

- `py_classify_imports_for_root` (capacity loop, ~line 729)
- `py_lsp_process_file` (TYPE_CHECKING branch, ~line 5339)

`ts_node_child_by_field_name(c, "consequence", 11)` can return a null node on an error-recovered `if_statement`, and in the vendored runtime `ts_node_named_child_count` → `ts_node__subtree(self)` does `*(const Subtree *)self.id` — a null node is a NULL dereference, not a benign 0. The extractor twin (`extract_imports.c:process_python_type_checking_block`) explicitly guards `ts_node_is_null(body)`; do the same here. (The field-name lengths 9/11 match the `TS_FIELD` strlen convention used throughout the file — verified.)

## ✅ Verified correct

**P1 — TYPE_CHECKING imports (`extract_imports.c`, `py_lsp.c`)**
- Detection spellings (`TYPE_CHECKING`, `typing.TYPE_CHECKING`), exact-match only, module top-level only — conservative and *identical* across both venues.
- `py_classify_imports_for_root` capacity is a safe upper bound (all named guard children counted, only imports appended); `stmts` allocation failure falls back to `have_root=false` (UNKNOWN classification), preserving the fail-closed contract.
- Pass-1 replay of guarded imports plus `py_invalidate_possible_bindings` for non-import guard statements is the right runtime direction (names bound only under the guard don't exist at runtime).

**P4 — nested/local binding (`extract_unified.c`)**
- `py_bind_local_definitions` walk is correct: boundary subtrees (function/class/lambda) are named but not descended into; non-boundary subtrees (incl. `decorated_definition` wrappers, if/try blocks) are walked so the inner `function_definition` is reached; the parent-ascent loop terminates exactly at `body`. Bindings ride the existing frame stack (`prev_py_param_stack_count`), so lifetimes unwind precisely; `py_param_bind` NULL-guards names. Function-frame-only scope matches Python visibility (class-body and closure reads still see the binding).

**P2 — bounded C3 sibling mixins (`pass_semantic.c`, `pass_parallel.c`)**
- `base_index` parity: both venues write `{"base_index":b}` with the original (pre-skip) base index into identical edge types; skipped bases leave rank gaps that preserve relative order. Both venues merge edges into `gbuf` *before* `cbm_pipeline_override_explicit` runs.
- Python classes always get label "Class", so the Interface→IMPLEMENTS split never diverts Python bases away from the INHERITS-only MRO view.
- The C3 merge is a faithful implementation (head-not-in-any-tail, all-heads advance, bases list as last sequence); cycles/self-loops fail via the `state==1` cache check; all bounds (`n < LIMIT`, `count == LIMIT` pre-write, `positions[LIMIT+1]`) check out. Rank-based ordering (not insertion order) is validated by test scenarios 1/4; abstract masking and own-method shadowing by scenarios 2/3. The winner/contract branch corroboration correctly prevents edges when both come from one base subtree, and the ordering respects that an earlier abstract declaration masks a later concrete one.
- Edge insertion dedupes by (source,target,type) with later-wins props — repeated winner→contract pairs across assemblies are safe and deterministic (class-id iteration order).

**P3 — dispatch composition + topology (`store.c`, `graph_mem.c`, `mcp.c`)**
- SQL/mem parity holds: the OR-join + `CASE` next_id is exactly matched by the two-phase level expansion; the reverse phase is gated on `(reverse_mask & mask)` exactly as the SQL `e.type IN (...)` filter gates the OVERRIDE arm; the shared `seen` set prevents double-adds within a level; per-level id sort matches `ORDER BY hop, n.id`. OVERRIDE-only queries cannot cross CALLS edges (tested).
- `bfs_collect_edges` is endpoint-based (not direction-based) and runs before test filtering, so reverse-OVERRIDE edges appear in `full->edges` and test nodes drop out via `trace_topology_node` → NULL.
- Budget/cursor contracts: `topology` forces `need_edge_data` (edges populated), joins `emit_optional_fields` and `omitted_optional_fields`, and is hashed into `trace_params_hash` so cursors minted under a different topology flag are rejected (tested). Budget-hit re-render omits the topology tables (tested). `has_calls && has_override` is computed after `resolve_trace_edge_types`; default CALLS-only queries are unaffected.
- `direction` parsing: the `*_override_bidir` strings are internal only; user-facing `do_outbound`/`do_inbound` parsing is unchanged.

**P5 — device renumbering (`daemon/ipc.c`)**
- The rebase is genuinely narrow: persisted device ≠ the record file's *freshly stat'ed* device, anchor currently on the record file's device, and same inode. Crucially `posix_socket_identity_equal` still requires ctime equality, and all unlink guards use freshly observed dev/ino — the persisted number is never used for deletion authority. Rebase operates on local copies; records aren't mutated. Both call sites (publication recovery and stale-generation cleanup) pass the correct per-record device. The test's offset-8 byte flip lands exactly on `POSIX_SOCKET_IDENTITY_DEVICE_OFFSET`, and the nlink-2 hardlink trick mutates the published record too — sound simulation, and non-renumber cases keep the old strict path.

**Graph cache / versioning**
- `cbm_graph_mem_bfs` remains a thin wrapper over `cbm_graph_mem_bfs_dispatch(..., 0, ...)`, so existing callers are unchanged; `cbm_graph_mem_type_mask(OVERRIDE)` returning 0 when the type is absent matches SQL's no-rows. `CBM_SEMANTIC_INDEX_VERSION` is only ever compared for equality (no stepwise migration or range validation), so 1000009 safely forces a rebuild; the fork-range rationale is documented.

## 🟡 Minor

1. **Naming collision** (`mcp.c` tool schema): the new boolean `topology` coexists with the pre-existing `mode: "topology"` enum value, which means something entirely different (infra objects/routes). Confusing for tool clients; consider `include_topology` or call it out in the description.
2. **Behavior change beyond the opt-in flag**: `dispatch_composition` activates bidirectional OVERRIDE for *any* query with both CALLS and OVERRIDE in `edge_types`, even with `topology:false`. This matches the stated intent ("CALLS-forward/reverse-OVERRIDE composition"), but it silently changes results for pre-existing queries — worth an explicit note in the tool description since cursor hashes won't flag it (same args, same hash, different graph semantics after re-index).
3. `trace_topology_json` emits `confidence`, but `bfs_collect_edges` hardcodes `1.0` for every edge — the field is constant noise. Drop it or plumb real confidence through.
4. Same edge can appear on multiple pages (incident to a node on each). The description hints at this ("Other endpoints may occur on another page"), but clients aggregating pages will double-count; consider documenting edge identity (source,target,type) as the dedup key.
5. `override_python_sibling_mixins` allocates a fresh ~133KB cache per multi-base Python class and recomputes base MROs per assembly — bounded and freed correctly, just noted for the record.
6. In `py_lsp.c`, the guard's `else`/`elif` alternative clause is not processed (conservative; consistent with the extractor, which also only reads `consequence`).

## Summary

One blocking defect (the commented-out P4 gate in `pass_parallel.c` — a real functional regression and a sequential/parallel determinism break that the file's own invariants prohibit), one robustness fix (null `consequence` guard in `py_lsp.c`), and a few polish items. The remaining ported logic — C3 assembly, dispatch composition with SQL/CSR parity, topology paging/budget/cursor contracts, and the device-rebase recovery — is semantically sound, bounded, and well covered by the new tests.

---


## Disposition on final candidate

- P4 blocking finding: corrected in `537c5e86`; the veto now executes in the
  parallel path. Independent weak constructor/member targets cover the veto
  separately from nested-binding detection. The fresh real-project query has
  zero library-to-test CALLS edges in the indexed graph.
- P1 null-body finding: corrected in `d7cb070b` at all three guarded-body
  count sites (capacity, cached classification, replay).
- Confidence: topology now reads the recorded property, or emits null when
  absent; a regression asserts a recorded value of 0.73.
- Schema descriptions document mixed-direction dispatch and the
  `(source,target,type)` edge deduplication key. The boolean topology remains
  distinct from infrastructure `mode=topology`, as in the installed fork.

The review sampled a working tree while validation was still finding issues;
its critical P4 observation describes the earlier source, not the final candidate.
The corrective edits are small and were validated with the affected regressions;
the affected regression checks passed.
