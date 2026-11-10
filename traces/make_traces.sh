#!/bin/bash
#
# Regenerate the short ARQ event traces in this folder.
#
# For each run it copies the simulator sources into a temporary folder,
# sets the parameters in the copy's simparameters.h, builds with
# -DTRACE_ON, and keeps only the protocol trace lines and the summary.
# The real sources are never modified.
#
# Usage (from Git Bash or MSYS, with gcc on the PATH):
#   bash traces/make_traces.sh
#
# To add or change a run, edit the "run" lines at the bottom:
#   run <output name> <Ws> <Wr> <p_d> <p_a> <packets to deliver> [seed]

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC="$(dirname "$SCRIPT_DIR")"
OUT="$SCRIPT_DIR"
SEED_DEFAULT=400459789

run() {
  local name=$1 ws=$2 wr=$3 pd=$4 pa=$5 n=$6 seed=${7:-$SEED_DEFAULT}
  local tmp
  tmp="$(mktemp -d)"

  cp "$SRC"/*.c "$SRC"/*.h "$tmp"/

  sed -i \
    -e "s/^#define SEND_WINDOW_SIZE .*/#define SEND_WINDOW_SIZE $ws/" \
    -e "s/^#define RECEIVE_WINDOW_SIZE .*/#define RECEIVE_WINDOW_SIZE $wr/" \
    -e "s/^#define DATA_PACKET_ERROR_PROB .*/#define DATA_PACKET_ERROR_PROB $pd/" \
    -e "s/^#define ACK_PACKET_ERROR_PROB .*/#define ACK_PACKET_ERROR_PROB $pa/" \
    -e "s/^#define RUNLENGTH .*/#define RUNLENGTH $n/" \
    -e "s/^#define RANDOM_SEED_LIST .*/#define RANDOM_SEED_LIST $seed/" \
    "$tmp/simparameters.h"

  if ! (cd "$tmp" && gcc -O2 -DTRACE_ON -o sim *.c -lm); then
    echo "Build failed for $name" >&2
    rm -rf "$tmp"
    return 1
  fi

  # </dev/null answers the getchar() at the end of main().
  # Keep the protocol trace lines ("<time>: ...") and the summary; drop
  # simlib's own event-list trace lines.
  # The progress line ends in \r, so turn \r into a newline or the trace
  # line printed after it would be glued on and filtered out.
  (cd "$tmp" && ./sim </dev/null) | tr '\r' '\n' |
    grep -aE "^[0-9.]+: |^Protocol|^p_d|^Delivered Pkts =|^Data Trans|^Data Pkts|^ACKs Sent|^Throughput|^Mean" \
    > "$OUT/$name.txt"

  rm -rf "$tmp"
  echo "Wrote $OUT/$name.txt"
}

#   name        Ws Wr p_d  p_a  packets
run sw_noerr    1  1  0.0  0.0  4
run gbn_noerr   4  1  0.0  0.0  8
run sr_noerr    4  4  0.0  0.0  8
run sw_err50    1  1  0.5  0.5  4
run gbn_err50   4  1  0.5  0.5  6
run sr_err50    4  4  0.5  0.5  6
