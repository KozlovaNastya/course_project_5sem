# Gaussian Blur 3×3 — Adobe Photoshop Filter Plug-in

A cross-platform C++ implementation of a classic Adobe Photoshop filter plug-in that applies a 3×3 Gaussian blur convolution.

The repository contains platform-specific project configurations for **Windows** and **macOS**, a shared image-processing implementation, independent tests, and the accompanying course paper.

## Overview

The plug-in applies a Gaussian blur using the following 3×3 convolution kernel:

\[
K = \frac{1}{16}
\begin{bmatrix}
1 & 2 & 1 \\
2 & 4 & 2 \\
1 & 2 & 1
\end{bmatrix}
\]

The filter is designed for the classic Adobe Photoshop Filter Plug-in API and is implemented in C++ using the Adobe Photoshop C++ SDK.

## Repository Contents

| Directory | Description |
|---|---|
| `GaussianBlur_Windows/` | Windows project for Visual Studio. Intended output: `.8bf` plug-in. |
| `GaussianBlur_macOS/` | macOS project for Xcode. Intended output: `.plugin` bundle. |
| `docs/` | Course paper in PDF format. |

Both platform projects contain the image-processing core, plug-in integration code, PiPL metadata, and tests.

## Course Paper

The repository also includes the written course project:

[**Read the course paper (PDF)**](docs/course-paper.pdf)

## Technologies

- C++
- Adobe Photoshop Classic Filter Plug-in API
- Adobe Photoshop C++ SDK
- Microsoft Visual Studio
- Apple Xcode
- CMake-independent test scripts

## Getting Started

Choose the project for your target platform:

- [Windows — build instructions](GaussianBlur_Windows/README_Windows.md)
- [macOS — build instructions](GaussianBlur_macOS/README_macOS.md)

The Adobe Photoshop SDK is not included in this repository. It must be obtained separately and configured before building the plug-in.

## Verification Status

The image-processing core includes independent tests.

In particular, the Windows build requires verification in Visual Studio, while the macOS build requires verification in Xcode on a Mac.

## Notes

This repository contains source code, project files, resources, tests, and documentation. Compiled plug-in binaries are not included.
