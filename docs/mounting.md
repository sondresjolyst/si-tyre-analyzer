# Mounting the tyre sensor

Each wheel unit points its MLX90640 (110° wide lens) at the tyre tread. Good
data needs the tread to fill the sensor's view, spread across the three
temperature bands (inner / middle / outer). This guide covers distance, aiming,
and orientation.

## How far from the tyre

The sensor sees a fixed cone. The tread runs across the sensor's **wide**
axis — the 110° horizontal field of view, the one split into the inner / middle
/ outer columns. The distance that makes a tread of width `W` just fill that
view is:

```text
distance = W / (2 · tan(110° / 2)) ≈ 0.35 · W
```

| Tread width | Distance |
|------------:|---------:|
| 155 mm      | 55 mm    |
| 185 mm      | 65 mm    |
| 205 mm      | 70 mm    |
| 225 mm      | 80 mm    |
| 245 mm      | 85 mm    |
| 265 mm      | 95 mm    |
| 285 mm      | 100 mm   |
| 305 mm      | 105 mm   |
| 325 mm      | 115 mm   |

These distances make the tread fill the frame edge to edge. Mount about 10 %
farther so the shoulders stay in view with a little margin.

You do not need to hit the distance exactly — tilting the sensor a few degrees
changes the covered width, so fine-tune the angle once it is roughly in place.

## Aiming

1. Power up the car and the wheel units.
2. In the app, open **Live** and connect to the master.
3. Turn on **Alignment guides**. Each wheel shows inner / middle / outer band
   labels and a centre line.
4. Warm the tyre (a heat gun or a few laps) so there is contrast.
5. Adjust each sensor until the warm tread fills the frame left to right and
   sits centred on the middle band.

## Orientation (flip)

Depending on how a sensor is bolted on, the image can come out mirrored — the
inner shoulder reading on the outer side, or the direction of travel reversed.
Correct it per device in the web config (open the device, **Configure**):

- **Mirror inner/outer** — use when the inner and outer shoulders are swapped.
- **Mirror direction of travel** — use when the rotation direction is reversed.

The flip is applied on the device, so both the live view and the recorded
sessions come out the right way round. Reboot the device for it to take effect.
You can also set it over serial: `flip <x> <y>` (e.g. `flip 1 0`).
