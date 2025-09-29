# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

MCUViewer is a C++20 GUI debug tool for microcontrollers with two main modules:
1. **Variable Viewer** - Real-time variable visualization using debug interface (SWDIO/SWCLK/GND)
2. **Trace Viewer** - SWO trace output visualization (SWDIO/SWCLK/SWO/GND)

The project supports STLink and JLink programmers and is built using CMake with ImGui for the interface.

## Development Commands

### Building
```bash
# Debug build (default)
mkdir build
cd build
cmake ..
make -j8

# Production build
cmake -DPRODUCTION=TRUE ..
make -j8

# Cross-compile for Windows (from Linux)
cmake -DPLATFORM=WIN -DPRODUCTION=TRUE ..
make -j8
```

### Testing
```bash
# Run all tests
./launch/run_tests.sh

# Or manually:
rm -rf build_test
mkdir build_test
cd build_test
cmake .. -DMAKE_TESTS=1
make -j
./test/MCUViewer_test
```

### Code Formatting
The project uses clang-format with Allman brace style. Configuration is in `.clang-format`.

## Architecture Overview

### Core Modules
- **src/Gui/** - ImGui-based user interface components
  - `Gui.cpp` - Main GUI controller
  - `GuiPlots.cpp` - Variable plotting interface
  - `GuiSwoPlots.cpp` - SWO trace plotting interface
  - `GuiAcqusition.cpp` - Data acquisition settings
- **src/MemoryReader/** - Debug probe interfaces
  - `StlinkDebugProbe.cpp` - STLink probe implementation
  - `JlinkDebugProbe.cpp` - JLink probe implementation
- **src/TraceReader/** - SWO trace data handling
  - `TraceReader.cpp` - Core trace processing
  - `StlinkTraceProbe.cpp` - STLink trace implementation
  - `JlinkTraceProbe.cpp` - JLink trace implementation
- **src/DataHandler/** - Data processing and management
  - `ViewerDataHandler.cpp` - Variable viewer data
  - `TraceDataHandler.cpp` - Trace viewer data
- **src/Variable/** - Variable management and parsing
- **src/Plot/** - Plotting functionality using ImPlot
- **src/GdbParser/** - ELF file parsing for variable addresses

### Key Dependencies
- **ImGui/ImPlot** - GUI and plotting (third_party/)
- **stlink** - STLink probe library (third_party/stlink/)
- **JLink** - SEGGER JLink library (third_party/jlink/)
- **spdlog** - Logging (fetched via CMake)
- **libusb** - USB communication
- **GLFW** - Window management

### Data Flow
1. ELF file parsing extracts variable addresses and types
2. Debug probes read memory via SWDIO interface
3. Variable data flows through DataHandler to GUI plots
4. SWO trace data flows through TraceReader to trace plots
5. Configuration managed via ConfigHandler (mINI format)

### Platform-Specific Notes
- **Linux**: Requires libusb-1.0-dev, libglfw3-dev, libgtk-3-dev
- **Windows**: Uses MSYS2/MinGW toolchain
- **macOS**: Requires OpenGL and GLUT frameworks
- Post-build: Copy `third_party/stlink/chips` to binary directory for STLink support

### Testing
- Uses Google Test framework
- Tests cover core components: RingBuffer, ScrollingBuffer, TraceReader, Statistics, GdbParser, Variable
- Test ELF file included in `test/testFiles/`

### Build System Notes
- Git version automatically embedded via `launch/addGitVersion.py`
- Supports cross-compilation for Windows from Linux
- CPack integration for package generation (.deb, .rpm, .exe)
- Clang-format integration with Allman style, 4-space tabs