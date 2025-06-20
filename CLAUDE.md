# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

### C++ DLL (Internal-Engine)
```bash
# Build using Visual Studio 2022
# Open Internal-Engine.sln in Visual Studio
# Build configuration: Release | x64
# Output: x64/Release/Internal-Engine.dll
```

### Web UI
```bash
cd WebUI
npm install          # Install dependencies
npm run dev          # Development server (http://localhost:3000)
npm run build        # Production build
npm run preview      # Preview production build
```


## Architecture Overview

This is a Windows memory manipulation framework with two main components:

1. **C++ DLL** (`Internal-Engine/`) - Injected into target processes
   - **WebSocketServer** (port 8765): Direct communication with Web UI
   - **MemoryEngine**: Core memory operations (read/write/scan/pattern matching)
   - **HookManager**: Function hooking via custom DetoursLite implementation
   - **CommandRouter**: JSON command processing and routing
   - **BinaryProtocol**: High-performance binary protocol for real-time updates

2. **Web UI** (`WebUI/`) - React/TypeScript interface
   - Direct WebSocket connection to DLL (port 8765)
   - Zustand state management with real-time polling
   - Window-based UI system with draggable/resizable components
   - Virtualized scanning results for unlimited items

## Communication Flow

```
Web UI (React) ←→ WebSocket (8765) ←→ DLL WebSocketServer
```

The DLL creates a WebSocket server on port 8765 for direct communication with the Web UI.

## Critical Code Paths

### Memory Operations
- **Scanning**: `MemoryEngine::FirstScan()` → `CommandRouter::HandleMemoryScan()`
- **Reading**: `MemoryEngine::SafeRead<T>()` → Protected with SEH
- **Writing**: `MemoryEngine::SafeWriteBytes()` → Validates addresses first

### Real-time Monitoring
- `engineStore.ts`: 50ms polling interval for ultra-fast address monitoring
- `Scanner.tsx`: Viewport-based monitoring (only visible addresses)
- Direct WebSocket messages for real-time value updates
- Web Workers for background processing (if available)

### Connection Management
- `directDllConnection.ts`: WebSocket client for DLL communication
- Automatic reconnection with exponential backoff
- Connection status tracking in Zustand store

## Development Patterns

### Adding New Commands
1. Add handler in `CommandRouter::RegisterBuiltinCommands()`
2. Implement in appropriate engine component
3. Add TypeScript interface in `engineStore.ts`
4. Update UI component to use new command

### Memory Safety
- All memory operations wrapped in SEH handlers
- Address validation before read/write
- Protection checks (`IsAddressReadable`, `IsAddressWritable`)

### Performance Considerations
- Viewport-based monitoring in Scanner (only visible results)
- Batch operations for multiple address updates
- 200ms debounce for monitoring updates
- Module info caching in DLL (5-second validity)

## Current State Notes

- **Direct Architecture**: WebSocket communication between DLL and Web UI
- **Performance Optimized**: Ultra-fast scanning with unlimited results support
- **Real-time Updates**: 50ms polling with viewport-based monitoring
- **Scanner Component**: React virtualization for handling massive result sets
- **Binary Protocol**: Available for high-performance scenarios