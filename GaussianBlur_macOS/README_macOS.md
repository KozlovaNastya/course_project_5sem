# Gaussian Blur 3×3 — macOS

macOS implementation of the classic Adobe Photoshop Filter Plug-in using C++ and the Adobe Photoshop C++ SDK.

The project is configured for **Apple Xcode** and is intended to produce a Photoshop filter plug-in bundle with the `.plugin` extension.

## Features

- 3×3 Gaussian blur convolution.
- Classic Photoshop Filter Plug-in API integration.
- Image-processing core shared with the Windows implementation.
- Tile-based image processing.
- Independent tests that do not require Photoshop.
- Universal build configuration for Apple Silicon (`arm64`) and Intel (`x86_64`).

The convolution kernel is:

\[
K = \frac{1}{16}
\begin{bmatrix}
1 & 2 & 1 \\
2 & 4 & 2 \\
1 & 2 & 1
\end{bmatrix}
\]

## Project Structure

```text
GaussianBlur_MacOS/
├── GaussianBlur.xcodeproj/
├── PhotoshopSdk.xcconfig
├── README_macOS.md
├── src/
│   ├── GaussianBlurCore.cpp
│   ├── GaussianBlurCore.h
│   └── GaussianBlurPlugin.cpp
├── mac/
│   └── Info.plist
├── pipl/
│   └── PiPLs.json
└── tests/
    ├── run_tests.sh
    └── test_core.cpp
```

### Main Components

| File or Directory | Purpose |
|---|---|
| `GaussianBlur.xcodeproj/` | Xcode project and build configurations. |
| `PhotoshopSdk.xcconfig` | SDK path and build configuration settings. |
| `src/GaussianBlurCore.cpp` | Implementation of the Gaussian blur algorithm. |
| `src/GaussianBlurCore.h` | Declarations and interface of the image-processing core. |
| `src/GaussianBlurPlugin.cpp` | Photoshop plug-in entry point and Filter Plug-in API integration. |
| `mac/Info.plist` | macOS plug-in bundle metadata. |
| `pipl/PiPLs.json` | Plug-in metadata, including its name, category, supported modes, and entry points. |
| `tests/` | Independent tests for the image-processing core. |

## Requirements

- macOS.
- Apple Xcode with Command Line Tools.
- Adobe Photoshop C++ SDK (`pluginsdk` directory).
- A compatible Adobe Photoshop installation for plug-in runtime testing.

The Adobe Photoshop SDK is not included in this repository.

## SDK Configuration

By default, the project expects the SDK at:

```text
../photoshop-cpp-sdk-main/pluginsdk
```

relative to the project directory.

If the SDK is stored elsewhere, create a local configuration file named `PhotoshopSdk.local.xcconfig` in the project directory:

```text
PHOTOSHOP_SDK_DIR = /full/path/to/photoshop-cpp-sdk-main/pluginsdk
```

This local file is intended for machine-specific configuration and should not be committed to the repository.

## Building

1. Open `GaussianBlur.xcodeproj` in Xcode.
2. Select the `GaussianBlur` scheme.
3. Choose the `Release` configuration.
4. Build the project using **Product → Build**.

Alternatively, use the command line:

```bash
xcodebuild \
  -project GaussianBlur.xcodeproj \
  -target GaussianBlur \
  -configuration Release
```

The expected output is:

```text
build/Release/GaussianBlur.plugin
```

The exact output location may depend on the Xcode build settings.

## Running Tests

The image-processing core can be tested independently of Photoshop.

Run:

```bash
tests/run_tests.sh
```

The expected result documented by the project is:

```text
63956 checks, 0 failed
```

## Installation

After a successful build:

1. Close Adobe Photoshop.
2. Copy `GaussianBlur.plugin` into the Photoshop `Plug-ins` directory.
3. Start Photoshop.
4. Locate the filter under:

```text
Filter → Course Project → Gaussian Blur 3x3
```

The filter does not provide a configuration dialog or adjustable radius.

## Verification Status

The image-processing core has independent test coverage. The Xcode build and execution inside Adobe Photoshop on macOS still require verification on a Mac.

