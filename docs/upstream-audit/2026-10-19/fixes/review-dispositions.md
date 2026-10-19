# Review dispositions

The main review returned no product-code correctness finding. Its substantive
follow-ups were handled as follows; the raw review remains in `review-main.md`.

1. **Canaries must run automatically:** addressed. `lint-cppcheck` now depends
   on `lint-cppcheck-canaries`, so both canonical CI and full lint run the CTU,
   cache/configuration and failed-worker probes before the real source scan.
   The canary script accepts the complete configured checker argv. Its failure
   trap prints the otherwise temporary diagnostic log. Both raw checker and
   aggregate gate exit status are retained. The completion error explains the
   CI-pinned cppcheck format expectation.
2. **Caller PATH portability:** the reported musl PATH-source difference is
   not supported by the actual primary source: musl's `__execvpe` reads
   `getenv("PATH")` and passes `envp` only to `execve`. Apple's `posix_spawnp`
   also reads `getenv("PATH")` and separately forwards `envp`; it explicitly
   handles ENOEXEC via the resolved script path and `/bin/sh`. No subprocess
   product change was justified by that review claim. This Linux/glibc host
   ran both fixture paths successfully. macOS runtime CI was not run here.
   Musl's source lacks glibc's ENOEXEC shell fallback, so this work does not
   claim new musl runtime qualification for that pre-existing test contract.
   Sources inspected on 2026-10-19:
   [musl execvp.c](https://git.musl-libc.org/cgit/musl/tree/src/process/execvp.c),
   [Apple posix_spawn.c](https://github.com/apple-oss-distributions/Libc/blob/main/sys/posix_spawn.c).
3. **Confidence bound:** addressed with `TRACE_CONFIDENCE_MAX` rather than the
   unrelated index-skip constant. This retains the exact numeric range.
4. **Partial state naming:** intentionally scoped to the edited proof blocks;
   untouched upstream comparisons were retained to avoid a file-wide style
   rewrite. The modified corroboration function now uses named path/link states.
5. **Evidence retention and knobs:** evidence stays under the existing build
   directory lifecycle, preserving failed qualification evidence. Invalid job
   settings fail closed; no broad tool-option normalization was added.

The bounded follow-up review (`review-followup.md`) approves the Python refactor
and gate scripts, with no semantic regression or gate-reliability hole found.
Its minor follow-ups were handled without changing product behavior:

- Documented cppcheckError's additional #error meaning and aggregate-vs-native
  exit handling; preserving a failing aggregate is intentional.
- Added exact status/message assertions for crash and CTU-internal canaries.
  Those two modes already emit complete manifests, so removing the internal
  error check would have made their previous assertions fail too; the added
  assertions make that isolation explicit.
- Added the CI-version hint to CTU/configuration canary errors.
- Kept the manifest contract specific to the canonical .c source argv and
  evidence under the existing build-directory lifecycle.

The affected canaries and canonical CI lint were rerun after these small
assertion/comment improvements. No further significant implementation change
followed review.
