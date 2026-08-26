# Third-party notices

Yaap links or distributes the following third-party components. Release builds
must replace version ranges with the exact resolved versions and append their
license texts to the package.

| Component | Purpose | License requirement |
|---|---|---|
| Qt 6 | GUI, networking, SQL, IPC | LGPL-3.0-only, GPL, or commercial Qt terms |
| FFmpeg | Demuxing and decoding | LGPL-compatible build required; configuration must be retained |
| miniaudio | Audio device output | Public domain or MIT-0 |
| Catch2 | Tests only | BSL-1.0 |

FFmpeg must be built without GPL or nonfree components for the intended LGPL
distribution. Codec patent obligations remain jurisdiction and product
dependent.

