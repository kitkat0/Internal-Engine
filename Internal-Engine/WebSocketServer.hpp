#pragma once
#include <windows.h>

#include <string>
#include <thread>
#include <vector>
#include <functional>
#include <atomic>
#include <queue>
#include <mutex>
#include <unordered_map>

#include <winsock.h>
#pragma comment(lib, "ws2_32.lib")

namespace InternalEngine {

// WebSocket frame types
enum class WebSocketOpcode {
    CONTINUATION = 0x0,
    TEXT = 0x1,
    BINARY = 0x2,
    CLOSE = 0x8,
    PING = 0x9,
    PONG = 0xA
};

// WebSocket connection state
enum class WebSocketState {
    CONNECTING,
    OPEN,
    CLOSING,
    CLOSED
};

// WebSocket frame structure
struct WebSocketFrame {
    WebSocketOpcode opcode;
    bool masked;
    uint64_t payloadLength;
    std::vector<uint8_t> payload;
};

// WebSocket connection
class WebSocketConnection {
public:
    WebSocketConnection(SOCKET socket, const std::string& clientAddr);
    ~WebSocketConnection();
    
    bool SendText(const std::string& text);
    bool SendBinary(const std::vector<uint8_t>& data);
    bool SendPing();
    bool SendPong();
    void Close();
    
    bool IsConnected() const { return state == WebSocketState::OPEN; }
    std::string GetClientAddress() const { return clientAddress; }
    
private:
    SOCKET clientSocket;
    std::string clientAddress;
    WebSocketState state;
    std::mutex sendMutex;
    
    bool SendFrame(WebSocketOpcode opcode, const std::vector<uint8_t>& payload);
    std::vector<uint8_t> CreateFrame(WebSocketOpcode opcode, const std::vector<uint8_t>& payload);
};

// High-performance WebSocket server for DLL
class WebSocketServer {
public:
    using MessageHandler = std::function<std::string(const std::string& message, WebSocketConnection* conn)>;
    using ConnectionHandler = std::function<void(WebSocketConnection* conn, bool connected)>;
    
    WebSocketServer();
    ~WebSocketServer();
    
    // Server control
    bool Start(int port = 8765);
    void Stop();
    bool IsRunning() const { return running; }
    
    // Event handlers
    void SetMessageHandler(MessageHandler handler) { messageHandler = handler; }
    void SetConnectionHandler(ConnectionHandler handler) { connectionHandler = handler; }
    
    // Broadcasting
    void BroadcastText(const std::string& text);
    void BroadcastBinary(const std::vector<uint8_t>& data);
    
    // Connection management
    size_t GetConnectionCount() const;
    std::vector<WebSocketConnection*> GetConnections() const;
    
    // Real-time data streaming for scan results
    void StreamScanResults(const std::string& jsonData);
    void StreamMemoryUpdate(const std::string& address, const std::string& newValue);
    
private:
    SOCKET serverSocket;
    std::atomic<bool> running;
    std::atomic<bool> stopping;
    
    std::thread acceptThread;
    std::vector<std::thread> workerThreads;
    
    std::vector<std::unique_ptr<WebSocketConnection>> connections;
    mutable std::mutex connectionsMutex;
    
    MessageHandler messageHandler;
    ConnectionHandler connectionHandler;
    
    // Performance optimization
    std::queue<std::string> messageQueue;
    std::mutex queueMutex;
    std::thread messageProcessor;
    
    void AcceptLoop();
    void ProcessClient(SOCKET clientSocket, const std::string& clientAddr);
    void ProcessMessages();
    
    bool PerformHandshake(SOCKET clientSocket);
    WebSocketFrame ReadFrame(SOCKET clientSocket);
    std::string GenerateWebSocketKey(const std::string& clientKey);
    
    void CleanupConnections();
    void RemoveConnection(WebSocketConnection* conn);
};

// Global WebSocket server instance
extern WebSocketServer* g_WebSocketServer;

} // namespace InternalEngine