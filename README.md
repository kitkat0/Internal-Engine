# Internal Engine - Advanced Memory Analysis Tool

Internal Engine is a powerful internal memory manipulation and debugging tool that operates from within the target process via DLL injection. It features a modern web-based UI with advanced stealth capabilities and real-time performance optimization.

![image](https://github.com/user-attachments/assets/de1c455f-1e82-45cd-8932-3b03647f2ab9)

## 🚀 Features

- **Internal Operation**: Runs inside the target process for maximum access and minimal detection
- **Direct WebSocket Communication**: Bridge-free architecture for ultra-low latency
- **Web-Based UI**: Modern, responsive React interface accessible from any browser
- **Real-time Memory Operations**: Read, write, scan, and pattern matching with live monitoring
- **Advanced Hooking**: Custom Detours Lite implementation for x86/x64 function hooking
- **High-Performance Scanning**: Virtualized results display supporting unlimited scan results
- **Stealth Design**: Local-only communication with no network exposure

## 🏗️ Architecture

```
┌─────────────────┐     Direct WebSocket     ┌─────────────┐
│  Target Process │ ◄─────────────────────► │   Web UI    │
│  (Internal DLL) │        Port 8765        │   (React)   │
└─────────────────┘                         └─────────────┘
```

### Core Components

1. **Internal Engine DLL** - C++ core injected into target processes
2. **Web UI** - Modern React/TypeScript interface with real-time updates

## 🛠️ Components

### Internal Engine DLL (`Internal-Engine/`)
- **WebSocketServer**: Direct communication with Web UI (port 8765)
- **MemoryEngine**: Core memory manipulation functions with safety checks
- **DetoursLite**: Lightweight x86/x64 hooking implementation
- **HookManager**: High-level hook management and lifecycle
- **CommandRouter**: JSON command processing and routing
- **BinaryProtocol**: High-performance binary protocol for real-time updates

### Web UI (`WebUI/`)
- React 18 + TypeScript application
- Zustand state management with real-time polling
- Desktop-like window management system
- Components:
  - **ProcessInfo**: Display process information and metrics
  - **ModuleList**: Browse loaded modules
  - **MemoryViewer**: Hex editor for memory inspection
  - **AddressList**: Manage and monitor specific addresses
  - **Scanner**: Advanced memory scanning with unlimited results
  - **Disassembler**: Assembly code viewer
  - **Console**: Command interface and debug output

## 📋 Prerequisites

- Windows 10/11
- Visual Studio 2022 with C++ development tools
- Node.js 16+ and npm
- Windows SDK

## 🔧 Building

### Build Internal Engine DLL
```bash
# Open Internal-Engine.sln in Visual Studio 2022
# Select Release | x64 configuration
# Build Solution (Ctrl+Shift+B)
# Output: x64/Release/Internal-Engine.dll
```

### Build Web UI
```bash
cd WebUI
npm install
npm run build
```

## 🚀 Usage

### 1. Inject the DLL
Inject `Internal-Engine.dll` into your target process using any DLL injector:
- Process Hacker
- Xenos Injector  
- Manual mapping tools
- Or your preferred injection method

### 2. Launch the Web UI
```bash
cd WebUI
npm run dev
# Open http://localhost:3000 in your browser
```

### 3. Connect and Use
The Web UI will automatically connect to the injected DLL via WebSocket on port 8765.

## 🎯 API Commands

The engine supports comprehensive commands via WebSocket:

### Memory Operations
- `memory.read` - Read memory at address
- `memory.write` - Write value to address  
- `memory.scan` - Scan for values with filters
- `memory.validate` - Validate address accessibility
- `memory.regions` - List memory regions

### Pattern Scanning
- `pattern.scan` - Single pattern/AOB scanning
- `pattern.scanall` - Find all pattern matches

### Process Information
- `process.info` - Get process details
- `module.list` - List loaded modules
- `module.info` - Get module information

### Advanced Features
- `hook.install` - Install function hooks
- `hook.remove` - Remove hooks
- `hook.list` - List active hooks
- `memory.allocate` - Allocate memory
- `memory.free` - Free allocated memory

## 🔒 Security Features

- **Local Communication Only**: WebSocket server bound to localhost (127.0.0.1)
- **No Network Exposure**: All communication stays within the local machine
- **Safe Memory Operations**: Protected with SEH (Structured Exception Handling)
- **Address Validation**: Comprehensive checks before read/write operations
- **Internal Operation**: Runs within target process address space

## ⚡ Performance Features

- **Direct WebSocket Communication**: Eliminates middleware overhead
- **Real-time Monitoring**: 50ms polling interval for ultra-fast updates
- **Viewport-based Scanning**: Only monitors visible results for optimal performance
- **Virtualized UI**: Handles unlimited scan results without performance degradation
- **Binary Protocol**: Optional high-performance binary communication
- **Memory Caching**: Smart caching of module information and memory regions

## 🛡️ Development

### Adding New Commands
1. Add handler in `CommandRouter::RegisterBuiltinCommands()`
2. Implement functionality using `MemoryEngine` or `HookManager`
3. Add TypeScript interface in `engineStore.ts`
4. Update UI component to use the new command

### Memory Safety Guidelines
- All memory operations use SEH for exception handling
- Address validation before any memory access
- Protection checks (`IsAddressReadable`, `IsAddressWritable`)
- Graceful error handling and user feedback

### Performance Best Practices
- Use viewport-based monitoring for large datasets
- Implement debouncing for frequent updates
- Cache module information when possible
- Prefer batch operations for multiple addresses

## 📁 Project Structure

```
Internal-Engine/
├── Internal-Engine/          # C++ DLL source code
│   ├── MemoryEngine.cpp/.hpp # Core memory operations
│   ├── WebSocketServer.cpp/.hpp # Direct WebSocket communication
│   ├── CommandRouter.cpp/.hpp # Command processing
│   ├── HookManager.cpp/.hpp  # Function hooking
│   └── dllmain.cpp          # DLL entry point
├── WebUI/                   # React web interface
│   ├── src/components/      # UI components
│   ├── src/store/          # State management
│   └── src/utils/          # Utility functions
├── x64/Release/            # Build output
└── README.md              # This file
```

## 🔧 Configuration

### Communication Ports
- **8765**: DLL WebSocket server (primary communication)
- **3000**: Web UI development server

### Memory Scanning Options
- Value types: int32, int64, float, double, string, bytes
- Scan types: exact, fuzzy, unknown, increased, decreased, changed, unchanged
- Memory filters: writable, executable, copy-on-write
- Address range specification with start/end addresses

## 📜 License

This project is for educational and research purposes only. Use responsibly and only on software you own or have explicit permission to modify.

## ⚠️ Disclaimer

This tool is designed for legitimate reverse engineering, debugging, and educational purposes. Users are responsible for ensuring compliance with applicable laws and software licenses. The developers assume no responsibility for misuse of this software.
