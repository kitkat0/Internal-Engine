#pragma once
#include <Windows.h>
#include <string>
#include <functional>
#include <thread>
#include <memory>
#include <mutex>
#include <queue>
#include <atomic>
#include <vector>
#include <sstream>

namespace InternalEngine {

// Message handler function type
using MessageHandler = std::function<std::string(const std::string& message)>;

class IpcServer {
public:
    IpcServer(uint16_t port = 8767); // Changed from 8765 to avoid WebSocket conflict
    ~IpcServer();

    bool Start();
    void Stop();
    
    void SetMessageHandler(MessageHandler handler);
    void SendMessage(const std::string& message);
    void BroadcastMessage(const std::string& message);
    
    size_t GetClientCount() const;
    std::vector<std::string> GetConnectedClients() const;

private:
    uint16_t port;
    std::atomic<bool> running;
    std::unique_ptr<std::thread> serverThread;
    
    SOCKET serverSocket;
    
    MessageHandler messageHandler;
    std::mutex queueMutex;
    std::queue<std::string> messageQueue;
    
    void ServerLoop();
    void HandleClient(SOCKET clientSocket);
    void ProcessMessages();
    std::string HandleMessage(const std::string& message);
    
    bool InitializeWinsock();
    void CleanupWinsock();
    std::string ParseHttpBody(const std::string& request);
    std::string CreateHttpResponse(const std::string& body, const std::string& contentType = "application/json");
};

// Global IPC server instance
extern IpcServer* g_IpcServer;

} // namespace InternalEngine 