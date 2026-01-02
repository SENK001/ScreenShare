# ScreenShare - Screen Sharing Application

[中文](README_zh.md) | English

## Introduction

A real-time screen sharing application for Windows using UDP multicast technology for screen capture and transmission.

## Features

- ✨ **Real-time Screen Capture**: High-performance screen capture using DXGI Desktop Duplication API or GDI+
- 🌐 **UDP Multicast Transmission**: One-to-many screen sharing via UDP multicast protocol
- 🎨 **Modern UI**: Tab-based interface based on Win32 API with DPI scaling support
- 📦 **Automatic Fragment Transmission**: Large data packets are automatically fragmented for reliable transmission
- 🔄 **Dual Role Support**: Supports both screen sending and receiving functions
- 🖼️ **JPEG Compression**: Images compressed using JPEG format to reduce bandwidth usage

## System Requirements

- **Operating System**: Windows 10 or higher
- **Compiler**: Visual Studio 2019 or higher (supporting C++17)
- **CMake**: 3.15 or higher
- **Dependencies**: 
  - GDI+ (Windows SDK)
  - Direct3D 11 (Windows SDK)
  - DXGI 1.2 (Windows SDK)
  - Winsock2
  - IP Helper API

## Build Instructions

### Building with CMake

1. Clone or download the project locally
2. Open PowerShell or Command Prompt and navigate to the project directory
3. Create a build directory and generate project files:

```bash
# Create build directory
mkdir build
cd build

# Generate Visual Studio project files
cmake ..

# Build project (Debug mode)
cmake --build . --config Debug

# Or build Release mode
cmake --build . --config Release
```

4. The generated executable files are located in `build/bin/Debug/` or `build/bin/Release/` directory

### Building with Visual Studio

1. Use CMake to generate Visual Studio solution file
2. Open `build/ScreenShare.sln`
3. Select build configuration (Debug or Release)
4. Press `F5` or click "Build Solution"

## Usage Instructions

### Sender Configuration

1. Launch the program and switch to the "Send" tab
2. Configure parameters:
   - **Network Adapter**: Select the network interface to use
   - **Multicast Address**: Set multicast group address (default: 239.255.0.1)
   - **Port**: Set UDP port (default: 9277)
3. Click "Start Sending" button to begin sharing screen
4. Click "Stop Sending" button to stop sharing

### Receiver Configuration

1. Launch the program (can be on the same or different machine)
2. Switch to the "Receive" tab
3. Configure parameters:
   - **Network Adapter**: Select network interface for receiving data
   - **Multicast Address**: Enter the same multicast address as sender
   - **Port**: Enter the same port number as sender
4. Click "Start Receiving" button
5. Received screen will be displayed in a new popup window
6. Receiving window supports double-click to toggle always-on-top display

## Technical Architecture

### Core Components

- **ScreenSender**: Screen capture and sending module
  - DXGI Desktop Duplication API for screen capture (preferred)
  - GDI+ as fallback solution
  - JPEG image compression
  - UDP multicast data transmission
  - Automatic fragmentation handling

- **ScreenReceiver**: Data receiving and display module
  - UDP multicast data receiving
  - Fragment reassembly
  - JPEG image decoding
  - Real-time image display

### Data Transfer Protocol

Uses a custom fragmentation protocol where each packet contains:
- Protocol magic number (2 bytes, fixed as 0x5353 to quickly identify ScreenShare data)
- Frame ID (4 bytes)
- Total frame size (4 bytes)
- Total number of fragments (2 bytes)
- Current fragment index (2 bytes)
- Fragment data size (2 bytes)
- Actual data (up to 1400 bytes)

Layout uses compact packing (16-byte header) with at least 2-byte alignment, with byte order conversion (hton*/ntoh*) to ensure cross-platform consistency. The receiver first validates the magic number, and abnormal packets are discarded directly.

### Project Structure

```
ScreenShare/
├── CMakeLists.txt              # CMake build configuration
├── ScreenShare.rc              # Resource file
├── ScreenShare.manifest        # Windows application manifest
├── README.md                   # English documentation
├── README_zh.md                # Chinese documentation
├── include/                    # Header files directory
│   ├── common.h                # Common definitions and utility functions
│   ├── receiver.h              # Receiver class declaration
│   ├── resource.h              # Resource definitions
│   └── sender.h                # Sender class declaration
├── src/                        # Source code directory
│   ├── main.cpp                # Main program and UI logic
│   ├── common.cpp              # Common function implementation
│   ├── receiver.cpp            # Receiver implementation
│   └── sender.cpp              # Sender implementation
├── res/                        # Resource files directory
│   ├── app.ico                 # Application icon
│   └── [other resource files]
├── tools/                      # Tool scripts directory
│   └── convert_to_ico.ps1      # PowerShell script to convert images to ICO
└── build/                      # Build output directory (generated during build)
```

## Network Configuration Notes

### Multicast Address Range

- Recommended to use private multicast address range: `239.255.0.0 - 239.255.255.255`
- Default address: `239.255.0.1`

### Port Selection

- Can use any unused UDP port
- Default port: `9277`
- Ensure firewall allows communication on the corresponding port

### Network Adapter

- The program automatically enumerates all available network adapters in the system
- Sender and receiver should use adapters in the same network
- If using multiple network interfaces, ensure the correct adapter is selected

## Troubleshooting

### Unable to receive screen

1. Check if sender and receiver are on the same network
2. Confirm multicast address and port configuration match
3. Check firewall settings to ensure UDP communication is allowed
4. Verify router supports multicast forwarding

### Screen delay or stuttering

1. Check if network bandwidth is sufficient
2. Try reducing the screen resolution on the sender side
3. Ensure CPU and GPU resources are sufficient
4. Reduce the number of simultaneous receiving clients

### Compilation errors

1. Ensure Windows SDK is installed
2. Verify Visual Studio version supports C++17
3. Check CMake version meets requirements (≥3.15)

## Version History

### v1.0.0 (2025-08-28)
- Initial version release
- Basic screen capture and transmission functionality
- DXGI and GDI+ dual-mode support
- UDP multicast transmission

### v1.1.0 (2026-01-03)
- Merged sender and receiver into single application
- Tab-based user interface
- Optimized fallback mechanism
- Optimized receiving window to display sender IP and FPS information in title
- Optimized fragmentation protocol with magic number for quick packet identification

## License

This project is for learning and research purposes only.

## Contributing

Issues and suggestions are welcome.

## Contact

For questions or suggestions, please contact via GitHub Issues.