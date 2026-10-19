#!/usr/bin/env bash
# Gate canaries: real CTU/configuration/cache checks plus failed-worker results.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
if [ "$#" -eq 0 ]; then
    set -- cppcheck
fi
checker=("$@")
fixture=$(mktemp -d)
cleanup() {
    local status=$?
    if [ "$status" -ne 0 ] && [ -f "$fixture/result.log" ]; then
        cat "$fixture/result.log" >&2
    fi
    rm -rf "$fixture"
    exit "$status"
}
trap cleanup EXIT
cd "$fixture"
mkdir cache
cat > common.h <<'C'
int dereference(const int *p);
C
cat > a.c <<'C'
#include "common.h"
int dereference(const int *p) { return *p; }
C
cat > b.c <<'C'
#include "common.h"
int main(void) { int value = 1; return dereference(&value); }
C
run_real() {
    bash "$root/scripts/lint-cppcheck.sh" cache "${checker[@]}" -j2 \
        --cppcheck-build-dir=cache --showtime=file-total --quiet \
        --enable=warning,style,performance,portability --std=c11 --language=c \
        --error-exitcode=1 a.c b.c > result.log 2>&1
}
run_real
run_real # Cached results must still emit a completion for both files.
cat > b.c <<'C'
#include "common.h"
int main(void) { const int *p = 0; return dereference(p); }
C
if run_real; then
    echo 'ERROR: missed cross-file null dereference after source change (CI uses cppcheck 2.20.0)' >&2
    exit 1
fi
grep -q '\[ctunullpointer\]' result.log
# Restore the caller, then change only the included header. An optional
# preprocessor configuration must still be explored without a -D restriction.
cat > b.c <<'C'
#include "common.h"
int main(void) { int value = 1; return dereference(&value); }
C
run_real
cat >> common.h <<'C'
#ifdef CBM_GATE_CONFIG
static int header_probe(void) { const int *p = 0; return *p; }
#endif
C
if run_real; then
    echo 'ERROR: stale cache or missed header configuration (CI uses cppcheck 2.20.0)' >&2
    exit 1
fi
grep -q '\[nullPointer\]' result.log
# Fault injection at the worker-output boundary: even exit 0 must not hide
# missing/duplicate completions or an internal worker failure.
cat > fake-checker <<'BASH'
#!/usr/bin/env bash
case "$1" in
    missing) echo 'Check time: a.c: 0.001s' ;;
    duplicate) printf 'Check time: a.c: 0.001s\nCheck time: a.c: 0.001s\nCheck time: b.c: 0.001s\n' ;;
    crash) printf 'Check time: a.c: 0.001s\nCheck time: b.c: 0.001s\nInternal error: Child process crashed with signal 9 [cppcheckError]\n' ;;
    ctu_failure) printf 'Check time: a.c: 0.001s\nCheck time: b.c: 0.001s\nerror: missing analyzer information [internalError]\n' ;;
    failed) printf 'Check time: a.c: 0.001s\nCheck time: b.c: 0.001s\n'; exit 7 ;;
esac
BASH
for mode in missing duplicate crash ctu_failure failed; do
    status=0
    bash "$root/scripts/lint-cppcheck.sh" cache bash ./fake-checker "$mode" a.c b.c > result.log 2>&1 || status=$?
    if [ "$status" -eq 0 ]; then
        echo "ERROR: accepted $mode worker result" >&2
        exit 1
    fi
    if [ "$mode" = crash ] || [ "$mode" = ctu_failure ]; then
        [ "$status" -eq 1 ]
        grep -q 'worker/internal failure' result.log
        if grep -q 'incomplete or duplicate file results' result.log; then
            echo "ERROR: $mode tripped completion validation instead of its internal-error check" >&2
            exit 1
        fi
    fi
    if [ "$mode" = failed ] && [ "$status" -ne 7 ]; then
        echo "ERROR: checker exit 7 was changed to $status" >&2
        exit 1
    fi
done
echo 'cppcheck gate canaries passed: cold/cache, CTU, source/header/config changes, missing/duplicate/crashed/failed workers and CTU failure'
