"""Lap-timing CSV parsing."""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from si_tyre_analyzer.lapdata import _mmss, parse_laps


def test_mmss_formats():
    assert _mmss("1:30.5") == 90.5
    assert _mmss("45.2") == 45.2
    assert _mmss("0:09") == 9.0
    assert _mmss("") is None
    assert _mmss("n/a") is None


def _write(tmp_path, rows):
    p = tmp_path / "laps.csv"
    p.write_text("Lap,Pos,Lap Time\n" + "\n".join(rows) + "\n")
    return str(p)


def test_cumulative_start_times(tmp_path):
    laps = parse_laps(_write(tmp_path, ["1,3,60.0", "2,2,65.0", "3,1,70.0"]))
    assert [lp.lap for lp in laps] == [1, 2, 3]
    assert [lp.start_s for lp in laps] == [0.0, 60.0, 125.0]
    assert [lp.pos for lp in laps] == [3, 2, 1]


def test_skips_unparseable_rows(tmp_path):
    laps = parse_laps(_write(tmp_path, ["1,1,60.0", "2,1,", "3,1,bogus", "4,1,30.0"]))
    assert [lp.lap for lp in laps] == [1, 4]
    assert [lp.start_s for lp in laps] == [0.0, 60.0]


def test_missing_pos_is_none(tmp_path):
    p = tmp_path / "nopos.csv"
    p.write_text("Lap,Lap Time\n1,60.0\n")
    laps = parse_laps(str(p))
    assert len(laps) == 1 and laps[0].pos is None


def test_empty_when_no_laps(tmp_path):
    assert not parse_laps(_write(tmp_path, []))


if __name__ == "__main__":
    import pytest

    raise SystemExit(pytest.main([__file__, "-q"]))
