# Changelog

## [0.6.1](https://github.com/sondresjolyst/si-tyre-analyzer/compare/si-tyre-analyzer-v0.6.0...si-tyre-analyzer-v0.6.1) (2026-06-29)


### Dependencies

* bump `actions/checkout` from 6.0.3 to 7.0.0 ([#49](https://github.com/sondresjolyst/si-tyre-analyzer/issues/49)) ([00d534b](https://github.com/sondresjolyst/si-tyre-analyzer/commit/00d534b52c4430a4710b293152a51d8e1ad4cf4e))
* bump `equinor/ops-actions/.github/workflows/release-please-manifest.yml` from 9.38.2 to 9.38.3 ([#50](https://github.com/sondresjolyst/si-tyre-analyzer/issues/50)) ([bf154a7](https://github.com/sondresjolyst/si-tyre-analyzer/commit/bf154a7173436922242c2d28c9d9f156b2529a87))
* bump `equinor/ops-actions/.github/workflows/super-linter.yml` from 9.38.2 to 9.38.3 ([#48](https://github.com/sondresjolyst/si-tyre-analyzer/issues/48)) ([6f4ba96](https://github.com/sondresjolyst/si-tyre-analyzer/commit/6f4ba96002c7a52cead63d6ca7a9748e5f635496))

## [0.6.0](https://github.com/sondresjolyst/si-tyre-analyzer/compare/si-tyre-analyzer-v0.5.0...si-tyre-analyzer-v0.6.0) (2026-06-22)


### Features

* native-resolution sensor alignment view ([#41](https://github.com/sondresjolyst/si-tyre-analyzer/issues/41)) ([5dd16ac](https://github.com/sondresjolyst/si-tyre-analyzer/commit/5dd16ac78c029fc5410ee1ffb02a95b9adc645de))
* web recording toggle, eFuse monitor, display anti-flicker ([#40](https://github.com/sondresjolyst/si-tyre-analyzer/issues/40)) ([0f3fc29](https://github.com/sondresjolyst/si-tyre-analyzer/commit/0f3fc299a533d61a276d8e8b20ce8ca68bf50a5c))

## [0.5.0](https://github.com/sondresjolyst/si-tyre-analyzer/compare/si-tyre-analyzer-v0.4.0...si-tyre-analyzer-v0.5.0) (2026-06-15)


### Features

* configurable optimal tyre temperature window ([#34](https://github.com/sondresjolyst/si-tyre-analyzer/issues/34)) ([b3b5299](https://github.com/sondresjolyst/si-tyre-analyzer/commit/b3b529906df3ff64a0d683e8c61845ffa945a692))
* redesign device web UI and TFT live view; unify heat scale ([#32](https://github.com/sondresjolyst/si-tyre-analyzer/issues/32)) ([8755f24](https://github.com/sondresjolyst/si-tyre-analyzer/commit/8755f24df01b26e0f5ef9b4f0909e047c884d9d8))

## [0.4.0](https://github.com/sondresjolyst/si-tyre-analyzer/compare/si-tyre-analyzer-v0.3.0...si-tyre-analyzer-v0.4.0) (2026-06-02)


### Features

* **app:** desktop icon, no-console launcher, detached self-update ([#14](https://github.com/sondresjolyst/si-tyre-analyzer/issues/14)) ([ff4b8fc](https://github.com/sondresjolyst/si-tyre-analyzer/commit/ff4b8fcb2b85869805f9647939ecf317c4abc33f))
* **app:** drag-drop import and right-click run menu ([#18](https://github.com/sondresjolyst/si-tyre-analyzer/issues/18)) ([4a53c1f](https://github.com/sondresjolyst/si-tyre-analyzer/commit/4a53c1f9916a3172701dd5ebe00eccdf5154e39b))
* **app:** header run name + About, shortcuts, edit button, Documents folder ([#23](https://github.com/sondresjolyst/si-tyre-analyzer/issues/23)) ([793ad93](https://github.com/sondresjolyst/si-tyre-analyzer/commit/793ad93b0fc2c9755446389a721f8ca2a967ae38))
* **app:** icon toolbars and themed combo/spin controls ([#20](https://github.com/sondresjolyst/si-tyre-analyzer/issues/20)) ([51636fc](https://github.com/sondresjolyst/si-tyre-analyzer/commit/51636fcce9496d112dc921af8c5f34f0e7ce8969))
* **app:** keyboard control in the heatmap viewer ([#19](https://github.com/sondresjolyst/si-tyre-analyzer/issues/19)) ([939d8fb](https://github.com/sondresjolyst/si-tyre-analyzer/commit/939d8fba899d2ee55057953faa058513099dfd96))
* **app:** macOS .app bundle ([#27](https://github.com/sondresjolyst/si-tyre-analyzer/issues/27)) ([0a46138](https://github.com/sondresjolyst/si-tyre-analyzer/commit/0a4613865817fccc021de8a1a0ea3914a3cb91f7))
* **app:** remember window size, empty-state hints, run search, status polish ([#21](https://github.com/sondresjolyst/si-tyre-analyzer/issues/21)) ([4d7f4d2](https://github.com/sondresjolyst/si-tyre-analyzer/commit/4d7f4d232f0cf15fdfc35098e5ead28f9e0eb474))
* **app:** safe delete, skip reporting, download progress ([#26](https://github.com/sondresjolyst/si-tyre-analyzer/issues/26)) ([1403cbd](https://github.com/sondresjolyst/si-tyre-analyzer/commit/1403cbd030ffcda73a3f648c5398ed30b123e5e8))
* **app:** sidebar icons and collapsible Analysis sub-nav ([#22](https://github.com/sondresjolyst/si-tyre-analyzer/issues/22)) ([1212ffd](https://github.com/sondresjolyst/si-tyre-analyzer/commit/1212ffd5e463e19e17bc67025536a9503fa54bbc))
* sensor mount-alignment aids (flip config, live guides, mounting doc) ([#24](https://github.com/sondresjolyst/si-tyre-analyzer/issues/24)) ([6d24cc8](https://github.com/sondresjolyst/si-tyre-analyzer/commit/6d24cc882df3227c0957e9b1f03d6f0f4504f2d9))


### Bug Fixes

* **ci:** use root release-please outputs for firmware build gate ([#17](https://github.com/sondresjolyst/si-tyre-analyzer/issues/17)) ([2e25880](https://github.com/sondresjolyst/si-tyre-analyzer/commit/2e25880c2e7017fcf6c52d5e885c9c1b07321d13))
* expand SVG width to prevent tagline text cutoff ([#25](https://github.com/sondresjolyst/si-tyre-analyzer/issues/25)) ([83d39eb](https://github.com/sondresjolyst/si-tyre-analyzer/commit/83d39eb8922a993c0061ceefe0ca230c38f1c2a8))

## [0.3.0](https://github.com/sondresjolyst/si-tyre-analyzer/compare/si-tyre-analyzer-v0.2.0...si-tyre-analyzer-v0.3.0) (2026-05-31)


### Features

* **app:** export Analysis report to PDF or PNG ([#8](https://github.com/sondresjolyst/si-tyre-analyzer/issues/8)) ([66c5f76](https://github.com/sondresjolyst/si-tyre-analyzer/commit/66c5f763ae645f4097aa036e0285a28af2833c4d))
* **app:** per-lap degradation tab (inner/mid/outer toggles) ([#11](https://github.com/sondresjolyst/si-tyre-analyzer/issues/11)) ([731ecc2](https://github.com/sondresjolyst/si-tyre-analyzer/commit/731ecc2b5d506278ed6398aae9fae52de482de19))
* **app:** per-run metadata + PDF cover page ([#12](https://github.com/sondresjolyst/si-tyre-analyzer/issues/12)) ([3a9a2cf](https://github.com/sondresjolyst/si-tyre-analyzer/commit/3a9a2cf68ad16c8c915ad375e02983cda7be03a1))
* dash gauge, in-app updates, and device firmware OTA ([#5](https://github.com/sondresjolyst/si-tyre-analyzer/issues/5)) ([cdc8200](https://github.com/sondresjolyst/si-tyre-analyzer/commit/cdc8200c65fa40121c8bcc1d7f0943ba098c7512))
* si tyre analyzer ([93635fe](https://github.com/sondresjolyst/si-tyre-analyzer/commit/93635fec2ff57678c81a71d37d5370152da7d712))


### Bug Fixes

* vendor Adafruit_MLX90640 as files instead of a submodule gitlink ([5ff4e6c](https://github.com/sondresjolyst/si-tyre-analyzer/commit/5ff4e6c241e6dffc087fb9fd50a2cdcf4eeb9be1))

## [0.2.0](https://github.com/sondresjolyst/si-tyre-analyzer/compare/v0.1.0...v0.2.0) (2026-05-29)


### Features

* si tyre analyzer ([93635fe](https://github.com/sondresjolyst/si-tyre-analyzer/commit/93635fec2ff57678c81a71d37d5370152da7d712))
