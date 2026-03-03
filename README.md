# Mouse Jiggler Detector

A Windows desktop application that detects hardware mouse jigglers and automated mouse movement patterns in real-time. This tool monitors all connected mouse devices and alerts users when suspicious automated movement patterns are detected.

## Overview

Mouse Jiggler Detector (FMJ_GUI) is designed to identify physical mouse jiggler devices and software-based automated mouse movements. It analyzes mouse movement patterns using multiple detection algorithms to catch various types of automated movements including:

- Small repetitive movements
- Large repetitive movement patterns
- Continuous movements over extended periods
- Constant speed anomalies
- Circular motion patterns
- Oscillation patterns
- External wheel-based jigglers on the primary mouse

## Features

- **Real-time Detection**: Monitors all connected mouse devices using Windows Raw Input API
- **Multiple Detection Algorithms**: Uses advanced pattern analysis including:
  - Continuous movement duration tracking
  - Speed variance analysis (Coefficient of Variation)
  - Circular pattern detection
  - Oscillation pattern recognition
  - Repetitive movement analysis
- **Touchpad Filtering**: Automatically distinguishes between mouse devices and touchpads
- **User-Friendly GUI**: Clean Windows interface showing device statistics and detection status
- **Device Statistics**: Displays variance analysis for all connected mouse devices

## Prerequisites

Before building this project, ensure you have the following installed:

### Required

- **CMake** (version 3.25 or higher)
- **C++ Compiler** with C++17 support:
  - Visual Studio 2017 or later, OR
  - MinGW-w64 (for GCC on Windows)
- **Windows SDK** (for Windows API headers)

### Optional

- **Git** (for cloning the repository)

## Building the Project

### Option 1: Using Visual Studio (Recommended for Windows)

1. **Clone the repository**:
   ```bash
   git clone https://github.com/anchormehra/Jiggler_Detector.git
   cd Jiggler_Detector
   ```

2. **Generate Visual Studio project files**:
   ```bash
   mkdir build
   cd build
   cmake .. -G "Visual Studio 17 2022"
   ```
   
   Note: Replace the generator with your installed Visual Studio version:
   - Visual Studio 2022: `"Visual Studio 17 2022"`
   - Visual Studio 2019: `"Visual Studio 16 2019"`
   - Visual Studio 2017: `"Visual Studio 15 2017"`
   - Visual Studio 2015: `"Visual Studio 14 2015"`

   You can also specify the architecture:
   ```bash
   cmake .. -G "Visual Studio 17 2022" -A x64    # 64-bit build
   cmake .. -G "Visual Studio 17 2022" -A Win32  # 32-bit build
   ```

3. **Build the project**:
   ```bash
   cmake --build . --config Release
   ```
   
   Or open the generated `.sln` file in Visual Studio and build from the IDE.

4. **Find the executable**:
   - The built executable will be in `build/Release/FMJ_GUI.exe`

### Option 2: Using MinGW

1. **Clone the repository**:
   ```bash
   git clone https://github.com/anchormehra/Jiggler_Detector.git
   cd Jiggler_Detector
   ```

2. **Generate Makefiles**:
   ```bash
   mkdir build
   cd build
   cmake .. -G "MinGW Makefiles"
   ```

3. **Build the project**:
   ```bash
   cmake --build .
   ```

4. **Find the executable**:
   - The built executable will be in `build/FMJ_GUI.exe`
   - Note: MinGW builds are statically linked for portability

### Option 3: Using CMake GUI

1. Open CMake GUI
2. Set "Where is the source code" to the repository root directory
3. Set "Where to build the binaries" to `repository_root/build`
4. Click "Configure" and select your generator (Visual Studio or MinGW)
5. Click "Generate"
6. Click "Open Project" to open in your IDE, or navigate to the build directory and build using command line

## Usage

1. **Run the application**:
   - Double-click `FMJ_GUI.exe` or run it from the command line
   - The application requires Windows Vista or later

2. **Monitor mouse devices**:
   - The main window displays all detected mouse devices
   - Each device shows variance statistics for X and Y movements
   - Touchpads are automatically identified and marked

3. **Detection alerts**:
   - When a jiggler is detected, a popup alert will appear
   - The main status will show the type of pattern detected
   - Detection types include:
     - Small repetitive movements
     - Large repetitive patterns
     - Continuous movement (15+ seconds)
     - Constant speed anomalies
     - Circular patterns
     - Oscillation patterns
     - External wheel-based jigglers on primary mouse

4. **Close the application**:
   - Click the X button or use Alt+F4 to exit

## Project Structure

```
Jiggler_Detector/
├── CMakeLists.txt          # CMake build configuration
├── main.cpp                # Application entry point, window proc, main detection loop
├── detection_constants.h   # Detection thresholds and tuning constants
├── mouse_device.h          # Core data structures (MouseDevice, DetectionType, etc.)
├── known_devices.h/cpp     # Gaming mice VID/PID lookup table
├── device_ids.h            # Jiggler/legitimate VID database and trust scoring
├── device_utils.h/cpp      # Device utility functions (touchpad detection, HID attributes)
├── detection.h/cpp         # Detection algorithm implementations
├── ui.h/cpp                # UI helpers, popup dialogs, calibration flow
├── globals.h/cpp           # Shared global state (extern declarations and definitions)
├── resource.rc             # Windows resource file
├── app.manifest            # Application manifest for Windows
└── README.md               # This file
```

## Technical Details

### Detection Algorithms

1. **Continuous Movement Detection**
   - Tracks movement duration over 30 seconds
   - Monitors pause patterns (< 2 seconds between movements)

2. **Speed Variance Analysis**
   - Calculates Coefficient of Variation (CV) for movement speeds
   - Flags movements with CV < 15% as suspicious

3. **Circular Pattern Detection**
   - Analyzes cumulative angle changes
   - Detects near-circular patterns (340-360 degrees)

4. **Oscillation Pattern Detection**
   - Counts directional reversals in X and Y axes
   - Requires minimum 10 reversals for detection

5. **Repetitive Movement Analysis**
   - Compares movement pairs for similarity
   - Uses 30% tolerance threshold

6. **External Wheel-Based Jiggler Detection**
   - Detects external wheel/turntable jigglers that physically move the user's primary mouse
   - Uses sliding-window circle fitting (arc pattern detection) to identify repeated arcs with consistent radius
   - The primary mouse uses a long qualifying process to avoid false positives:
     - Requires at least 150 movement samples and 15 seconds of observation before analysis begins
     - Suspicious patterns must be sustained for multiple consecutive analysis cycles (~3 seconds) before flagging
     - Transient spikes from normal use (e.g. sudden direction changes during curves) are filtered out because the score resets when the pattern is not sustained
   - A wheel jiggler produces characteristic arc patterns, constant speed, and continuous movement that collectively exceed the detection threshold over sustained periods

### Dependencies

The application links against the following Windows libraries:
- `comctl32.lib` - Common Controls
- `uxtheme.lib` - UI Themes
- `hid.lib` - Human Interface Device
- `msimg32.lib` - GDI functions (GradientFill)

## Troubleshooting

### Build Issues

**Problem**: CMake version error
```
Solution: Upgrade CMake to version 3.25 or higher
```

**Problem**: Windows SDK not found
```
Solution: Install Windows SDK through Visual Studio Installer or standalone
```

**Problem**: Compiler not found
```
Solution: Ensure Visual Studio or MinGW is properly installed and in PATH
```

**Problem**: Build errors related to `min` and `max` (C2589, C4003)
```
Solution: This is already fixed in the code with NOMINMAX define.
If you still encounter these errors, ensure you're using the latest version of main.cpp.
The NOMINMAX preprocessor definition prevents Windows.h from defining min/max macros
that conflict with std::min and std::max.
```

### Runtime Issues

**Problem**: Application fails to start
```
Solution: Ensure you're running on Windows Vista or later
```

**Problem**: Devices not detected
```
Solution: Check that mouse devices are properly connected and recognized by Windows
```

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## Support

For issues, questions, or suggestions, please visit [detectmyjiggler.com](https://detectmyjiggler.com).

## Acknowledgments

This application uses Windows Raw Input API for low-level mouse monitoring and detection.
