# Gaussian Blur 3x3

A classic Adobe Photoshop Filter Plug-in implemented in C++ using the Adobe Photoshop C++ SDK.

## Project Description

The plug-in applies a 3x3 Gaussian blur convolution kernel to an image.

The filter uses the following kernel:

```text
1 2 1
2 4 2   / 16
1 2 1
```

## Project Structure

- `win/` — Windows implementation and Visual Studio project.
- `mac/` — macOS implementation and Xcode project.

Both platform projects share the same image-processing core.

## Technologies

- C++
- Adobe Photoshop Classic Filter Plug-in API
- Visual Studio
- Xcode

## SDK

The Adobe Photoshop SDK is not included in this repository. It must be obtained separately and configured for the corresponding platform project.

## Notes

The project contains source code, build configurations, resources, and independent tests. Photoshop runtime compatibility and platform-specific builds require separate verification.
