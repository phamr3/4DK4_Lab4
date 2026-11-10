#!/usr/bin/env python3
"""
Draw an ARQ timing (ladder) diagram from a simulator trace file.

The local end (sender) is the top line, the far end (receiver) is the bottom
line, and time runs from left to right. It shows:

  * every data frame transmission as a band from the sender to the receiver,
    labelled with its sequence number ("F3", or "F3 re" for a retransmission)
    and what the receiver did with it (accepted, error, discarded, buffered,
    duplicate),
  * every ACK / NAK as a line from the receiver back to the sender, labelled
    with the R_next value it carries,
  * lost frames and lost ACKs/NAKs as dashed lines ending in an "x",
  * sender timeouts,
  * state lanes, one segment per value, changing where the value changes:
      - send window: the sequence numbers [base, base + Ws - 1] the sender
        may have outstanding (above the sender),
      - R_next, and the receive window [R_next, R_next + Wr - 1]
        (below the receiver),
      - Selective-Repeat only: the receive buffer, i.e. which out-of-order
        frames are being held until the gap in front of them is filled.

The trace files come from make_traces.sh (built with -DTRACE_ON).

Usage:
  python traces/plot_trace.py traces/sr_err50.txt
  python traces/plot_trace.py traces/*.txt              # one PNG per trace
  python traces/plot_trace.py traces/gbn_err50.txt --start 40 --end 100
  python traces/plot_trace.py traces/sw_err50.txt --show

Each PNG is written next to its trace (same name, .png) unless --out is given.
"""

import argparse
import glob
import re
import sys
from pathlib import Path

import matplotlib
import matplotlib.patches

if "--show" not in sys.argv:
    matplotlib.use("Agg")  # no window needed: just write the PNG
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib.lines import Line2D  # noqa: E402
from matplotlib.patches import Patch, Polygon  # noqa: E402

# --------------------------------------------------------------------------
# Colours (reference data-viz palette). Identity is never colour alone: lost
# frames/ACKs are also dashed and end in an "x", and every mark is labelled.

SURFACE = "#fcfcfb"
TEXT = "#0b0b0b"
TEXT_2 = "#52514e"
TEXT_3 = "#8a8984"
AXIS = "#b9b8b2"
DATA = "#2a78d6"      # data frames            (categorical slot 1, blue)
NAK = "#eb6834"       # NAKs                   (categorical slot 2, orange)
ACK = "#1baf7a"       # ACKs                   (categorical slot 3, aqua)
LOST = "#d03b3b"      # the "x" where something was lost (status: critical)
TIMEOUT = "#6250d6"   # sender timeouts        (categorical slot 7, violet)

LANE_FILL = ("#ffffff", "#f0efec")  # alternating segment fills in a state lane

# Vertical positions (data units). The sender and receiver lines are 1 apart;
# the state lanes sit above the sender and below the receiver, clear of the
# rotated labels next to each line.
Y_SWIN = 1.80     # send window lane
Y_SEND = 1.0
Y_RECV = 0.0
Y_RNEXT = -0.62   # R_next lane
Y_RWIN = -1.06    # receive window lane
Y_BUF = -1.50     # receive buffer lane (SR only)
LANE_H = 0.34
IN_PER_UNIT = 2.1  # vertical inches per data unit

# --------------------------------------------------------------------------
# Parsing

LINE = re.compile(r"^\s*([0-9]+(?:\.[0-9]+)?): (.*)$")
TX = re.compile(r"^(Transmit new|Retransmit) seq (\d+)")
RX_ERR = re.compile(r"^Packet seq (\d+) received in error")
RX_OK = re.compile(r"^Packet seq (\d+) accepted")
RX_OOO = re.compile(r"^Packet seq (\d+) out of order \(R_next (\d+)\), (discarded|buffered)")
RX_BEYOND = re.compile(r"^Packet seq (\d+) beyond receive window")
RX_DUP = re.compile(r"^Duplicate packet seq (\d+)")
ACK_SEND = re.compile(r"^Send (ACK|NAK)\((\d+)\)")
ACK_RX = re.compile(r"^(ACK|NAK)\((\d+)\) (received|lost)")
TIMEOUT_RE = re.compile(r"^Timeout seq (\d+)(?:, go back to (\d+))?")
SUMMARY = re.compile(r"^(Protocol|p_d) = ")


def parse(path):
    tx, rx, ack_send, ack_rx, timeouts, summary = [], [], [], [], [], []

    with open(path, encoding="utf-8", errors="replace") as f:
        for raw in f:
            raw = raw.rstrip("\n")
            if SUMMARY.match(raw):
                summary.append(raw.strip())
                continue
            m = LINE.match(raw)
            if not m:
                continue
            t, msg = float(m.group(1)), m.group(2)

            if (g := TX.match(msg)):
                tx.append({"t": t, "seq": int(g.group(2)),
                           "retx": g.group(1) == "Retransmit"})
            elif (g := RX_ERR.match(msg)):
                rx.append({"t": t, "seq": int(g.group(1)), "result": "error"})
            elif (g := RX_OK.match(msg)):
                rx.append({"t": t, "seq": int(g.group(1)), "result": "accepted"})
            elif (g := RX_OOO.match(msg)):
                rx.append({"t": t, "seq": int(g.group(1)), "result": g.group(3)})
            elif (g := RX_BEYOND.match(msg)):
                rx.append({"t": t, "seq": int(g.group(1)), "result": "discarded"})
            elif (g := RX_DUP.match(msg)):
                rx.append({"t": t, "seq": int(g.group(1)), "result": "duplicate"})
            elif (g := ACK_SEND.match(msg)):
                ack_send.append({"t": t, "kind": g.group(1), "n": int(g.group(2))})
            elif (g := ACK_RX.match(msg)):
                ack_rx.append({"t": t, "kind": g.group(1), "n": int(g.group(2)),
                               "lost": g.group(3) == "lost"})
            elif (g := TIMEOUT_RE.match(msg)):
                timeouts.append({"t": t, "seq": int(g.group(1)),
                                 "goback": g.group(2) is not None})

    return tx, rx, ack_send, ack_rx, timeouts, summary


def pair_fifo(sent, received, key, label):
    """Both links deliver in order with a fixed delay, so the i-th thing
    received is the i-th thing sent. Anything left over was still on the link
    when the run ended."""
    pairs = []
    for i, s in enumerate(sent):
        r = received[i] if i < len(received) else None
        if r is not None and s[key] != r[key]:
            sys.exit(f"{label}: send #{i} ({s[key]}) does not match "
                     f"receive #{i} ({r[key]}). Is this a trace from make_traces.sh?")
        pairs.append((s, r))
    return pairs


def rnext_timeline(rx, ack_send):
    """R_next starts at 0 and only changes when a frame is accepted; every
    ACK/NAK carries the current value, so read the changes off them."""
    changes = [(0.0, 0)]
    for a in sorted(ack_send, key=lambda a: a["t"]):
        if a["n"] != changes[-1][1]:
            changes.append((a["t"], a["n"]))
    return changes


def base_timeline(ack_rx):
    """The sender's base (oldest unacknowledged frame) starts at 0 and moves
    up to n whenever an ACK/NAK(n) that is not lost arrives with n > base."""
    changes = [(0.0, 0)]
    for a in sorted(ack_rx, key=lambda a: a["t"]):
        if not a["lost"] and a["n"] > changes[-1][1]:
            changes.append((a["t"], a["n"]))
    return changes


def buffer_timeline(rx, rnext):
    """Selective-Repeat receive buffer: the out-of-order frames being held.
    A frame joins when it is buffered and leaves when R_next moves past it
    (it is delivered)."""
    events = [(r["t"], 0, r["seq"]) for r in rx if r["result"] == "buffered"]
    events += [(t, 1, n) for t, n in rnext[1:]]
    held, changes = set(), [(0.0, ())]
    for t, kind, v in sorted(events):
        if kind == 0:
            held.add(v)
        else:
            held = {s for s in held if s >= v}
        state = tuple(sorted(held))
        if changes[-1][0] == t:
            changes[-1] = (t, state)      # several changes at one instant
        elif state != changes[-1][1]:
            changes.append((t, state))
    return changes


def window_label(lo, size):
    return f"{lo}" if size == 1 else f"{lo}–{lo + size - 1}"


def draw_state_lane(ax, y, changes, fmt, t0, t1, ms_per_inch, muted=None):
    """Draw a lane of segments, one per value: each segment runs from the
    time the value took effect to the next change. The label is written
    across the segment if it fits, otherwise vertically."""
    pts_per_ms = 72.0 / ms_per_inch
    k = 0
    for i, (t, v) in enumerate(changes):
        t_end = changes[i + 1][0] if i + 1 < len(changes) else t1
        a, b = max(t, t0), min(t_end, t1)
        if b <= a:
            continue
        ax.add_patch(matplotlib.patches.Rectangle(
            (a, y - LANE_H / 2), b - a, LANE_H, facecolor=LANE_FILL[k % 2],
            edgecolor=AXIS, lw=0.8, zorder=2))
        k += 1

        label = fmt(v)
        if not label:
            continue
        color = TEXT_2 if (muted and muted(v)) else TEXT
        fs = 8.5
        if len(label) * 0.6 * fs + 6 <= (b - a) * pts_per_ms:
            ax.text((a + b) / 2, y, label, fontsize=fs, color=color,
                    ha="center", va="center", zorder=6, clip_on=True)
        else:
            ax.text((a + b) / 2, y, label, fontsize=7, color=color, rotation=90,
                    ha="center", va="center", zorder=6, clip_on=True)


# --------------------------------------------------------------------------
# Drawing


def draw(path, out, tx_time, start, end, ms_per_inch, show):
    tx, rx, ack_send, ack_rx, timeouts, summary = parse(path)
    if not tx:
        print(f"{path}: no trace lines found, skipped")
        return

    frames = pair_fifo(tx, rx, "seq", "data frames")
    acks = pair_fifo(ack_send, ack_rx, "n", "ACK/NAKs")
    rnext = rnext_timeline(rx, ack_send)
    base = base_timeline(ack_rx)

    # Window sizes from the summary line "Protocol = ... (Ws = 4, Wr = 4)".
    ws = wr = None
    for line in summary:
        m = re.search(r"Ws = (\d+), Wr = (\d+)", line)
        if m:
            ws, wr = int(m.group(1)), int(m.group(2))
    if ws is None:
        sys.exit(f"{path}: no 'Protocol = ... (Ws = .., Wr = ..)' line; "
                 "regenerate it with make_traces.sh")
    sr = wr > 1
    buf = buffer_timeline(rx, rnext) if sr else None

    # One-way delays, measured from the trace itself.
    matched = [(s, r) for s, r in frames if r]
    data_delay = (matched[0][1]["t"] - matched[0][0]["t"]) if matched else tx_time + 5
    matched_a = [(s, r) for s, r in acks if r]
    ack_delay = (matched_a[0][1]["t"] - matched_a[0][0]["t"]) if matched_a else 5

    # Include where in-flight frames/ACKs would land, so they are not cut off.
    t_last = max([e["t"] for e in tx + rx + ack_send + ack_rx + timeouts] +
                 [s["t"] + data_delay for s, r in frames if r is None] +
                 [s["t"] + ack_delay for s, r in acks if r is None])
    t0 = start if start is not None else 0.0
    t1 = end if end is not None else t_last + 2
    width = max(8.0, (t1 - t0) / ms_per_inch + 2.8)
    y_top = Y_SWIN + LANE_H / 2 + 0.08
    y_bot = (Y_BUF if sr else Y_RWIN) - LANE_H / 2 - 0.08
    height = (y_top - y_bot) * IN_PER_UNIT + 1.6

    fig, ax = plt.subplots(figsize=(width, height), facecolor=SURFACE)
    ax.set_facecolor(SURFACE)

    # The two ends.
    for y in (Y_SEND, Y_RECV):
        ax.plot([t0, t1], [y, y], color=TEXT_2, lw=1.4, zorder=1)

    def visible(*ts):
        return any(t0 <= t <= t1 for t in ts)

    label_box = dict(boxstyle="round,pad=0.12", fc=SURFACE, ec="none", alpha=0.85)

    # ---- data frames: sender (top) -> receiver (bottom) ---------------------
    for s, r in frames:
        ts = s["t"]
        if r is not None:
            tr = r["t"]
            result = r["result"]
        else:
            tr = ts + data_delay
            result = "in flight"
        if not visible(ts, tr):
            continue

        lost = result == "error"
        in_flight = result == "in flight"
        poly = [(ts, Y_SEND), (ts + tx_time, Y_SEND),
                (tr, Y_RECV), (tr - tx_time, Y_RECV)]
        ax.add_patch(Polygon(poly, closed=True, facecolor=DATA,
                             alpha=0.07 if (lost or in_flight) else 0.16,
                             edgecolor="none", zorder=2))
        ax.plot([ts + tx_time, tr], [Y_SEND, Y_RECV], color=DATA, lw=1.6,
                ls=(0, (4, 2)) if (lost or in_flight) else "-",
                alpha=0.55 if in_flight else 1.0, zorder=3)

        if lost:
            ax.plot(tr - 0.15, Y_RECV + 0.035, marker="x", ms=9, mew=2.2,
                    color=LOST, zorder=5)

        # Frame label just below the sender, outcome just below the receiver.
        f = 0.06
        ax.text(ts + tx_time + f * (tr - ts - tx_time), Y_SEND - f,
                f"F{s['seq']}" + (" re" if s["retx"] else ""),
                rotation=90, fontsize=7.5, color=TEXT, ha="center", va="top",
                zorder=6, clip_on=True, bbox=label_box)
        ax.text(tr, Y_RECV - 0.04, {"accepted": "accepted", "error": "error",
                                    "discarded": "discarded", "buffered": "buffered",
                                    "duplicate": "duplicate", "in flight": "(in flight)"}[result],
                rotation=90, fontsize=7, color=TEXT_2, ha="center", va="top",
                zorder=6, clip_on=True)

    # ---- ACKs / NAKs: receiver (bottom) -> sender (top) ---------------------
    for s, r in acks:
        ts = s["t"]
        if r is not None:
            tr, lost, in_flight = r["t"], r["lost"], False
        else:
            tr, lost, in_flight = ts + ack_delay, False, True
        if not visible(ts, tr):
            continue

        color = NAK if s["kind"] == "NAK" else ACK
        ax.plot([ts, tr], [Y_RECV, Y_SEND], color=color, lw=1.4,
                ls=(0, (4, 2)) if (lost or in_flight) else "-",
                alpha=0.55 if in_flight else 1.0, zorder=3)
        if lost:
            ax.plot(tr - 0.15, Y_SEND - 0.035, marker="x", ms=9, mew=2.2,
                    color=LOST, zorder=5)

        f = 0.06
        ax.text(ts + f * (tr - ts), Y_RECV + f,
                f"{s['kind']}({s['n']})" + (" lost" if lost else "") +
                (" in flight" if in_flight else ""),
                rotation=90, fontsize=7.5, color=TEXT, ha="center", va="bottom",
                zorder=6, clip_on=True, bbox=label_box)

    # ---- sender timeouts (above the sender line) ----------------------------
    for to in timeouts:
        if not visible(to["t"]):
            continue
        ax.plot(to["t"], Y_SEND, marker="D", ms=5.5, color=TIMEOUT, zorder=5)
        ax.text(to["t"], Y_SEND + 0.04,
                f"timeout F{to['seq']}" + (", go back" if to["goback"] else ""),
                rotation=90, fontsize=7, color=TEXT_2, ha="center", va="bottom",
                zorder=6, clip_on=True)

    # ---- state lanes -------------------------------------------------------
    # Send window: sequence numbers the sender may have outstanding.
    draw_state_lane(ax, Y_SWIN, base, lambda b: window_label(b, ws),
                    t0, t1, ms_per_inch)
    # R_next, and the receive window it starts.
    draw_state_lane(ax, Y_RNEXT, rnext, str, t0, t1, ms_per_inch)
    draw_state_lane(ax, Y_RWIN, rnext, lambda n: window_label(n, wr),
                    t0, t1, ms_per_inch)
    # SR receive buffer: out-of-order frames being held.
    if sr:
        draw_state_lane(ax, Y_BUF, buf,
                        lambda h: ", ".join(f"F{s}" for s in h) if h else "empty",
                        t0, t1, ms_per_inch, muted=lambda h: not h)

    # ---- axes, title, legend ----------------------------------------------
    ax.set_xlim(t0 - 0.5, t1)
    ax.set_ylim(y_bot, y_top)
    ticks = [(Y_SWIN, f"Send window\n(Ws = {ws})"),
             (Y_SEND, "Local end\n(sender)"),
             (Y_RECV, "Far end\n(receiver)"),
             (Y_RNEXT, "R_next"),
             (Y_RWIN, f"Receive window\n(Wr = {wr})")]
    if sr:
        ticks.append((Y_BUF, "Receive buffer\n(out of order)"))
    ax.set_yticks([y for y, _ in ticks])
    ax.set_yticklabels([s for _, s in ticks],
                       fontsize=9, color=TEXT, fontweight="bold")
    ax.tick_params(axis="y", length=0)
    ax.set_xlabel("time (ms)", color=TEXT_2, fontsize=9)
    ax.tick_params(axis="x", colors=TEXT_2, labelsize=8, length=3)
    for side in ("top", "right", "left"):
        ax.spines[side].set_visible(False)
    ax.spines["bottom"].set_color(AXIS)
    ax.grid(axis="x", color=AXIS, alpha=0.35, lw=0.5)

    title = Path(path).name
    if summary:
        title += "\n" + "   ".join(summary)
    fig.suptitle(title, fontsize=10, color=TEXT, y=0.995)

    handles = [
        Line2D([], [], color=DATA, lw=1.6, label="data frame"),
        Line2D([], [], color=ACK, lw=1.4, label="ACK(R_next)"),
        Line2D([], [], color=NAK, lw=1.4, label="NAK(R_next)"),
        Line2D([], [], color=TEXT_2, lw=1.4, ls=(0, (4, 2)), marker="x",
               mec=LOST, mew=2, ms=7, label="lost (error)"),
        Line2D([], [], color=TIMEOUT, lw=0, marker="D", ms=5.5, label="timeout"),
        Patch(facecolor=LANE_FILL[1], edgecolor=AXIS,
              label="lane segment: value until the next change"),
    ]
    ax.legend(handles=handles, loc="upper center", bbox_to_anchor=(0.5, -0.06),
              ncol=6 if width >= 14 else 3, fontsize=8, frameon=False,
              labelcolor=TEXT_2)

    fig.tight_layout()
    fig.savefig(out, dpi=150, facecolor=SURFACE, bbox_inches="tight")
    print(f"Wrote {out}")
    if show:
        plt.show()
    plt.close(fig)


def main():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("traces", nargs="+", help="trace file(s) from make_traces.sh")
    p.add_argument("--out", help="output image (only with a single trace); "
                   "default: <trace>.png")
    p.add_argument("--start", type=float, help="first time to show (ms)")
    p.add_argument("--end", type=float, help="last time to show (ms)")
    p.add_argument("--tx-time", type=float, default=1.0,
                   help="data frame transmission time in ms (default 1.0)")
    p.add_argument("--ms-per-inch", type=float, default=5.0,
                   help="horizontal scale; smaller spreads events out (default 5)")
    p.add_argument("--show", action="store_true", help="also open a window")
    a = p.parse_args()

    # PowerShell and cmd.exe pass wildcards through unexpanded (bash expands
    # them itself), so expand any pattern like traces\*.txt here.
    traces = []
    for arg in a.traces:
        if glob.has_magic(arg):
            matches = sorted(glob.glob(arg))
            if not matches:
                p.error(f"no files match {arg}")
            traces.extend(matches)
        else:
            if not Path(arg).is_file():
                p.error(f"no such file: {arg}")
            traces.append(arg)

    if a.out and len(traces) > 1:
        p.error("--out can only be used with one trace file")

    for t in traces:
        out = a.out or str(Path(t).with_suffix(".png"))
        draw(t, out, a.tx_time, a.start, a.end, a.ms_per_inch, a.show)


if __name__ == "__main__":
    main()
