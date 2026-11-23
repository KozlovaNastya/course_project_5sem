# Gaussian Blur 3×3 — Windows

Windows implementation of the classic Adobe Photoshop Filter Plug-in using C++ and the Adobe Photoshop C++ SDK.

The project is configured for **Microsoft Visual Studio** and is intended to produce a Photoshop filter plug-in with the `.8bf` extension.

## Features

- 3×3 Gaussian blur convolution.
- Classic Photoshop Filter Plug-in API integration.
- Image-processing core separated from Photoshop-specific code.
- Tile-based image processing.
- Independent tests that do not require Photoshop.

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
GaussianBlur_Windows/
├── GaussianBlur.sln
├── GaussianBlur.vcxproj
├── PhotoshopSdk.props
├── README_Windows.md
├── src/
│   ├── GaussianBlurCore.cpp
│   ├── GaussianBlurCore.h
│   └── GaussianBlurPlugin.cpp
├── win/
│   └── GaussianBlur.rc
├── pipl/
│   └── PiPLs.json
└── tests/
    ├── run_tests.bat
    ├── run_tests.sh
    └── test_core.cpp
```

### Main Components

| File or Directory | Purpose |
|---|---|
| `GaussianBlur.sln` | Visual Studio solution. |
| `GaussianBlur.vcxproj` | Project configuration and build settings. |
| `PhotoshopSdk.props` | Locates the Adobe Photoshop SDK. |
| `src/GaussianBlurCore.cpp` | Implementation of the Gaussian blur algorithm. |
| `src/GaussianBlurCore.h` | Declarations and interface of the image-processing core. |
| `src/GaussianBlurPlugin.cpp` | Photoshop plug-in entry point and Filter Plug-in API integration. |
| `win/GaussianBlur.rc` | Windows resource file. |
| `pipl/PiPLs.json` | Plug-in metadata, including its name, category, supported modes, and entry point. |
| `tests/` | Independent tests for the image-processing core. |

## Requirements

- Windows 10 or Windows 11 (x64).
- Microsoft Visual Studio 2019 or newer.
- Desktop development with C++ workload.
- Adobe Photoshop C++ SDK (`pluginsdk` directory).
- A compatible 64-bit Adobe Photoshop installation for plug-in runtime testing.

The Adobe Photoshop SDK is not included in this repository.

## SDK Configuration

By default, the project expects the SDK at:

```text
../photoshop-cpp-sdk-main/pluginsdk
```

relative to the project directory.

If the SDK is stored elsewhere, configure the `PHOTOSHOP_SDK_DIR` environment variable with the full path to the `pluginsdk` directory.

Alternatively, pass the path directly to MSBuild:

```bat
msbuild GaussianBlur.sln /p:PhotoshopSdkDir="C:\path\to\pluginsdk"
```

## Building

1. Open `GaussianBlur.sln` in Visual Studio.
2. Select the `Release` configuration.
3. Select the `x64` platform.
4. Build the solution.

The expected output is:

```text
build/x64/Release/GaussianBlur.8bf
```

The actual output location may depend on the project configuration.

## Running Tests

The image-processing core can be tested independently of Photoshop.

Run the test script from an x64 Native Tools Command Prompt:

```bat
tests\run_tests.bat
```

The expected result documented by the project is:

```text
63956 checks, 0 failed
```

## Installation

After a successful build:

1. Close Adobe Photoshop.
2. Copy `GaussianBlur.8bf` into the Photoshop `Plug-ins` directory.
3. Start Photoshop.
4. Locate the filter under:

```text
Filter → Course Project → Gaussian Blur 3x3
```

The filter does not provide a configuration dialog or adjustable radius.

## Verification Status

The image-processing core has independent test coverage. The Visual Studio build and execution inside Adobe Photoshop on Windows still require verification in a native Windows environment.

