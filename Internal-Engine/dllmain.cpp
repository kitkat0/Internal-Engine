// dllmain.cpp : Defines the entry point for the DLL application.
#include <Windows.h>
#include <thread>
#include <memory>
#include <iostream>

#include "MemoryEngine.hpp"
#include "HookManager.hpp"
#include "CommandRouter.hpp"
#include "WebSocketServer.hpp"
// IPC Server removed - WebSocket-only architecture

using namespace InternalEngine;

// Core thread handle
static std::unique_ptr<std::thread> g_CoreThread;
static bool g_Running = false;

// Console logging helper
void LogToConsole(const std::string& message) {
    std::cout << "[InternalEngine] " << message << std::endl;
    OutputDebugStringA(("[InternalEngine] " + message + "\n").c_str());
}

// Initialize all engine components
void InitializeEngine() {
    LogToConsole("=== Initializing Internal Engine ===");
    
    try {
        // Create global instances
        LogToConsole("Creating HookManager...");
        g_HookManager = new HookManager();
        LogToConsole("HookManager created successfully");
        
        LogToConsole("Creating CommandRouter...");
        g_CommandRouter = new CommandRouter();
        LogToConsole("CommandRouter created successfully");
        
        // === NEW: Direct WebSocket Communication ===
        LogToConsole("Creating WebSocket Server for direct web communication...");
        g_WebSocketServer = new WebSocketServer();
        LogToConsole("WebSocket Server created successfully");
        
        // Set up WebSocket message handler
        LogToConsole("Setting up WebSocket message handler...");
        g_WebSocketServer->SetMessageHandler([](const std::string& message, WebSocketConnection* conn) {
            if (g_CommandRouter) {
                std::string response = g_CommandRouter->ExecuteCommand(message);
                return response;
            }
            return std::string("{\"error\": \"Command router not initialized\"}");
        });
        
        // Set up connection handler for monitoring
        g_WebSocketServer->SetConnectionHandler([](WebSocketConnection* conn, bool connected) {
            if (connected) {
                LogToConsole("New WebSocket client connected: " + conn->GetClientAddress());
            } else {
                LogToConsole("WebSocket client disconnected: " + conn->GetClientAddress());
            }
        });
        
        // Start WebSocket server on port 8765 (direct web communication)
        LogToConsole("Starting WebSocket server for direct web communication...");
        if (g_WebSocketServer->Start(8765)) {
            LogToConsole("✅ WebSocket server started on port 8765!");
            LogToConsole("🚀 Direct DLL-to-Web communication enabled!");
            LogToConsole("📡 Web UI can connect to: ws://localhost:8765");
        } else {
            LogToConsole("❌ ERROR: Failed to start WebSocket server!");
        }
        
        // === IPC SERVER REMOVED: Direct WebSocket communication only ===
        LogToConsole("🔥 Bridge-free architecture: WebSocket-only communication enabled!");
        LogToConsole("📡 All communication goes through WebSocket port 8765");
        
        LogToConsole("=== Internal Engine Initialization Complete ===");
        
    } catch (const std::exception& e) {
        LogToConsole("EXCEPTION during initialization: " + std::string(e.what()));
    } catch (...) {
        LogToConsole("UNKNOWN EXCEPTION during initialization!");
    }
}

// Cleanup all engine components
void CleanupEngine() {
    LogToConsole("=== Cleaning up Internal Engine ===");
    
    // Stop WebSocket server first (primary communication)
    if (g_WebSocketServer) {
        LogToConsole("Stopping WebSocket server...");
        g_WebSocketServer->Stop();
        delete g_WebSocketServer;
        g_WebSocketServer = nullptr;
        LogToConsole("WebSocket server stopped");
    }
    
    // === IPC SERVER REMOVED: WebSocket-only cleanup ===
    LogToConsole("🔥 Bridge-free cleanup: No legacy IPC to stop!");
    
    // Cleanup command router
    if (g_CommandRouter) {
        LogToConsole("Cleaning up CommandRouter...");
        delete g_CommandRouter;
        g_CommandRouter = nullptr;
        LogToConsole("CommandRouter cleaned up");
    }
    
    // Remove all hooks
    if (g_HookManager) {
        LogToConsole("Removing all hooks...");
        g_HookManager->RemoveAllHooks();
        delete g_HookManager;
        g_HookManager = nullptr;
        LogToConsole("HookManager cleaned up");
    }
    
    LogToConsole("=== Internal Engine Cleanup Complete ===");
}

// Core engine thread
void CoreThread() {
    LogToConsole("Core thread started");
    
    // Initialize engine
    InitializeEngine();
    
    // Keep thread alive
    LogToConsole("Entering main loop...");
    while (g_Running) {
        Sleep(1000); // Check every second
    }
    
    LogToConsole("Exiting main loop...");
    
    // Cleanup
    CleanupEngine();
    
    LogToConsole("Core thread ending");
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        // Allocate console first
        AllocConsole();
        
        // Redirect stdout to console
        freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
        freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);
        freopen_s((FILE**)stdin, "CONIN$", "r", stdin);
        
        LogToConsole("=== DLL INJECTED SUCCESSFULLY ===");
        LogToConsole("Process ID: " + std::to_string(GetCurrentProcessId()));
        
        // Disable thread library calls for performance
        DisableThreadLibraryCalls(hModule);
        
        // Start core thread
        g_Running = true;
        LogToConsole("Starting core thread...");
        g_CoreThread = std::make_unique<std::thread>(CoreThread);
        LogToConsole("Core thread started");
        break;
        
    case DLL_PROCESS_DETACH:
        LogToConsole("=== DLL DETACHING ===");
        
        // Signal shutdown
        g_Running = false;
        
        // Wait for core thread to finish
        if (g_CoreThread && g_CoreThread->joinable()) {
            LogToConsole("Waiting for core thread to finish...");
            g_CoreThread->join();
            LogToConsole("Core thread finished");
        }
        
        LogToConsole("=== DLL DETACHED ===");
        break;
        
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }
    
    return TRUE;
}

