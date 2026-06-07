"""Shared constants."""

DEFAULT_HOST = "192.168.4.1"

WHEELS = ["FL", "FR", "RL", "RR"]

# Optimal tyre grip window (°C). The full colour scale derives from this:
# cold 40%, optimal (green) 30%, high 15%, superhigh 15%. Defaults suit historic
# slicks; sessions recorded on v2 firmware carry the device's own window in the
# file header. Used as the fallback for v1 files that predate the field.
OPT_LO_DEFAULT = 80.0
OPT_HI_DEFAULT = 95.0
