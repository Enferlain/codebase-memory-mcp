# Candidate in-action acceptance

Date: 2026-10-19. Candidate source: `70627910`; runtime version
`0.12.1-fork.1`. This uses the actual release binary via newline JSON-RPC
MCP stdio, with its private daemon and supervised indexing path. It does not
replace the installed server or repoint existing clients.

All 15 checks passed. The test copied the earlier frozen 943-file sd-scripts
snapshot at source commit `9c4d7addc7fcac2bdd194a3d7cbadb7ce8603df9` into a
private directory and initialized a separate Git repository there. No original
source, Git metadata, client configuration or live cache was modified.

- MCP initialize and tools/list advertised version 0.12.1-fork.1 and 17 tools.
- A cold full index, symbol searches, source snippet and coverage requests ran
  through tools/call. Both training-loop contract calls resolved. Outbound
  CALLS/OVERRIDE topology reached SD, SDXL and SD3 implementations.
- The scoped production-library-to-tests query returned no weak target edges.
- The running frontend and daemon executable hashes matched the candidate,
  proving the session did not silently use the installed binary.
- After closing the session and stopping the private daemon, a fresh MCP
  session loaded the saved graph as ready (15,157 nodes).
- With the normal Git watcher active, creating an untracked Python file made
  its function searchable. Editing that file added a function and a verified
  CALLS edge. Deleting it removed both test functions, without a manual reindex.
- Both private daemon lifecycles ended cleanly. The installed binary's SHA-256
  was identical before and after testing.

Raw request/response streams and daemon logs are compressed alongside the
summary. `training-trace.json` is the material dispatch result, not a claim
of exhaustive graph coverage. The cold index retains two unrelated partial
parse files and the existing configured exclusions.

Two setup attempts are preserved separately. The first harness incorrectly
expected JSON from index_status's default text output; requesting format=json
resolved it. A non-Git copy indexed and queried successfully but did not watch
edits with the default watch_non_git=false (daemon strategy=none). The final
Git-backed copy exercises the normal project setup. Non-Git folders require
watch_non_git=true if automatic updates are desired. Neither setup issue was
treated as a product pass or concealed by skipping an assertion.
