#!/usr/bin/env bash
# Keep cppcheck's native parallel/whole-program analysis and verify every TU
# returned. Retain the command, source manifest, diagnostics and timings.
set -euo pipefail

if [ "$#" -lt 2 ]; then
    echo 'usage: lint-cppcheck.sh <build-directory> <cppcheck> [arguments...]' >&2
    exit 2
fi
cache_dir=$1
shift
mkdir -p "$cache_dir"
run_dir=$(mktemp -d "$cache_dir/run.XXXXXX")
printf '%q ' "$@" > "$run_dir/command.txt"
printf '\n' >> "$run_dir/command.txt"
for arg in "$@"; do
    case "$arg" in
        -*) ;;
        *.c) printf '%s\n' "$arg" ;;
    esac
done | LC_ALL=C sort -u > "$run_dir/expected.txt"
if [ ! -s "$run_dir/expected.txt" ]; then
    echo 'cppcheck: no source files supplied' >&2
    exit 2
fi
status=0
LC_ALL=C "$@" > "$run_dir/output.log" 2>&1 || status=$?
cat "$run_dir/output.log"
printf '%s\n' "$status" > "$run_dir/checker-exit-status.txt"
# --showtime=file-total prints once for every processed file, including cache
# hits. A worker that exits without a normal return cannot satisfy this check.
awk '/^Check time: / { sub(/^Check time: /, ""); sub(/: [0-9].*$/, ""); print }' \
    "$run_dir/output.log" | LC_ALL=C sort > "$run_dir/completed.txt"
# An invalid aggregate exits 1; the original checker code remains recorded.
if ! diff -u "$run_dir/expected.txt" "$run_dir/completed.txt"; then
    echo "cppcheck: incomplete or duplicate file results; evidence: $run_dir" >&2
    echo "cppcheck: expected CI-pinned 2.20.0 --showtime=file-total completion output" >&2
    status=1
fi
# cppcheckError also covers taken #error directives; both must fail the gate.
if grep -Eq 'Internal error:|\[(cppcheckError|internalError)\]' "$run_dir/output.log"; then
    echo "cppcheck: worker/internal failure; evidence: $run_dir" >&2
    status=1
fi
printf '%s\n' "$status" > "$run_dir/exit-status.txt"
printf 'cppcheck: %s source files, exit %s; evidence: %s\n' \
    "$(wc -l < "$run_dir/expected.txt" | tr -d ' ')" "$status" "$run_dir"
exit "$status"
