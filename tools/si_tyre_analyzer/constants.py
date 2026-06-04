"""Shared constants."""

DEFAULT_HOST = "192.168.4.1"

WHEELS = ["FL", "FR", "RL", "RR"]

# Fixed temperature range (°C) the heatmap colours map to. A fixed scale keeps
# colour meaning an absolute temperature — a given red is always the same heat,
# instead of tracking whatever the current hottest pixel happens to be (which is
# misleading while driving). Bounds cover a cold/pit tyre (~ambient) up through
# an overheating one; racing slick surface temps run ~60–110°C in their window.
TEMP_SCALE_LO = 60.0
TEMP_SCALE_HI = 110.0
