# Heatmap colour scale

The colour scale comes from the **Optimal window** (low/high °C) set on the
**master**, which applies it to the whole car when recording. The window is the
green zone; the scale extends below and above it in fixed proportions.

Example: a 70–100 °C window gives a 30 → 130 °C scale:

| Band      |   % | Range (°C) | Colour          |
| --------- | --: | ---------- | --------------- |
| Cold      | 40% | 30 – 70    | blue → green    |
| Optimal   | 30% | 70 – 100   | green           |
| High      | 15% | 100 – 115  | yellow → orange |
| Superhigh | 15% | 115 – 130  | orange → red    |
