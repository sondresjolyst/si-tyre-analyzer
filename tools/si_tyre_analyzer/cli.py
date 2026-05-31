"""SI Tyre Analyzer CLI.

Examples:
  si-tyre info FL_12345.bin
  si-tyre heatmap FL_12345.bin --save out.mp4
  si-tyre timeseries FL_12345.bin
  si-tyre dashboard ./sessions
  si-tyre fetch --host 192.168.4.1 --all --dest ./sessions
"""

from __future__ import annotations

import argparse
import glob
import os

from . import fetch
from .constants import DEFAULT_HOST
from .logreader import read_session


def _cmd_info(args):
    s = read_session(args.file)
    print(
        f"wheel={s.wheel} session_id={s.session_id} grid={s.cols}x{s.rows} "
        f"rate={s.sample_rate_hz}Hz records={len(s.t_offsets_ms)} "
        f"duration={s.duration_s:.1f}s fw={s.fw_version} mac={s.mac} "
        f"group={s.group_id} epoch_ms={s.epoch_ms}"
    )


def _cmd_heatmap(args):
    from .heatmap import animate

    animate(read_session(args.file), save=args.save)


def _cmd_timeseries(args):
    from .timeseries import plot

    plot(read_session(args.file), save=args.save)


def _cmd_dashboard(args):
    from .heatmap import dashboard

    files = sorted(glob.glob(os.path.join(args.dir, "*.bin")))
    sessions = [read_session(f) for f in files]
    if not sessions:
        raise SystemExit(f"no .bin files in {args.dir}")
    # Group by the most common session_id.
    sid = max(
        {s.session_id for s in sessions},
        key=lambda x: sum(1 for s in sessions if s.session_id == x),
    )
    dashboard([s for s in sessions if s.session_id == sid], save=args.save)


def _cmd_fetch(args):
    if args.all:
        for p in fetch.download_all(args.host, args.dest):
            print(f"downloaded {p}")
    elif args.name:
        print(f"downloaded {fetch.download(args.host, args.name, args.dest)}")
    else:
        for s in fetch.list_sessions(args.host):
            print(f"{s['wheel']:>2}  {s['name']:<24} {s['records']} recs {s['size']} B")


def main(argv=None):
    p = argparse.ArgumentParser(prog="si-tyre")
    sub = p.add_subparsers(dest="cmd", required=True)

    sp = sub.add_parser("info")
    sp.add_argument("file")
    sp.set_defaults(fn=_cmd_info)

    sp = sub.add_parser("heatmap")
    sp.add_argument("file")
    sp.add_argument("--save")
    sp.set_defaults(fn=_cmd_heatmap)

    sp = sub.add_parser("timeseries")
    sp.add_argument("file")
    sp.add_argument("--save")
    sp.set_defaults(fn=_cmd_timeseries)

    sp = sub.add_parser("dashboard")
    sp.add_argument("dir")
    sp.add_argument("--save")
    sp.set_defaults(fn=_cmd_dashboard)

    sp = sub.add_parser("fetch")
    sp.add_argument("--host", default=DEFAULT_HOST)
    sp.add_argument("--name")
    sp.add_argument("--all", action="store_true")
    sp.add_argument("--dest", default=".")
    sp.set_defaults(fn=_cmd_fetch)

    args = p.parse_args(argv)
    args.fn(args)


if __name__ == "__main__":
    main()
