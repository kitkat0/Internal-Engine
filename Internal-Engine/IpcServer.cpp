#include "IpcServer.hpp"
#include "CommandRouter.hpp"
#include <iostream>
#include <sstream>

#include <winsock.h>
#pragma comment(lib, "ws2_32.lib")

namespace InternalEngine {

// Global instances
IpcServer* g_IpcServer = nullptr;
// g_CommandRouter는 CommandRouter.cpp에서 정의됨

// Console logging helper for IpcServer
void LogIpcToConsole(const std::string& message) {
    std::cout << "[IpcServer] " << message << std::endl;
    OutputDebugStringA(("[IpcServer] " + message + "\n").c_str());
}

// IpcServer implementation using HTTP
IpcServer::IpcServer(uint16_t port) 
    : port(port), running(false), serverSocket(INVALID_SOCKET) {
    LogIpcToConsole("IpcServer constructor called (HTTP mode on port " + std::to_string(port) + ")");
}

IpcServer::~IpcServer() {
    LogIpcToConsole("IpcServer destructor called");
    Stop();
}

bool IpcServer::Start() {
    if (running) {
        LogIpcToConsole("Server already running");
        return false;
    }
    
    LogIpcToConsole("Starting HTTP IPC server on port " + std::to_string(port) + "...");
    
    if (!InitializeWinsock()) {
        LogIpcToConsole("Failed to initialize Winsock");
        return false;
    }
    
    running = true;
    serverThread = std::make_unique<std::thread>(&IpcServer::ServerLoop, this);
    LogIpcToConsole("Server thread started");
    
    return true;
}

void IpcServer::Stop() {
    if (!running) return;
    
    LogIpcToConsole("Stopping HTTP IPC server...");
    running = false;
    
    // 서버 소켓 닫기
    if (serverSocket != INVALID_SOCKET) {
        closesocket(serverSocket);
        serverSocket = INVALID_SOCKET;
    }
    
    if (serverThread && serverThread->joinable()) {
        LogIpcToConsole("Waiting for server thread to finish...");
        serverThread->join();
        LogIpcToConsole("Server thread finished");
    }
    
    CleanupWinsock();
}

bool IpcServer::InitializeWinsock() {
    LogIpcToConsole("Initializing Winsock...");
    
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        LogIpcToConsole("WSAStartup failed: " + std::to_string(result));
        return false;
    }
    
    // 서버 소켓 생성
    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        LogIpcToConsole("Socket creation failed: " + std::to_string(WSAGetLastError()));
        WSACleanup();
        return false;
    }
    
    // 주소 재사용 옵션 설정
    int optval = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval));
    
    // 주소 바인딩
    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverAddr.sin_port = htons(port);
    
    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        LogIpcToConsole("Bind failed: " + std::to_string(WSAGetLastError()));
        closesocket(serverSocket);
        serverSocket = INVALID_SOCKET;
        WSACleanup();
        return false;
    }
    
    // 리스닝 시작
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        LogIpcToConsole("Listen failed: " + std::to_string(WSAGetLastError()));
        closesocket(serverSocket);
        serverSocket = INVALID_SOCKET;
        WSACleanup();
        return false;
    }
    
    LogIpcToConsole("HTTP server listening on http://127.0.0.1:" + std::to_string(port));
    return true;
}

void IpcServer::CleanupWinsock() {
    LogIpcToConsole("Cleaning up Winsock...");
    
    if (serverSocket != INVALID_SOCKET) {
        closesocket(serverSocket);
        serverSocket = INVALID_SOCKET;
    }
    
    WSACleanup();
    LogIpcToConsole("Winsock cleanup complete");
}

void IpcServer::SetMessageHandler(MessageHandler handler) {
    LogIpcToConsole("Message handler set");
    messageHandler = handler;
}

void IpcServer::SendMessage(const std::string& message) {
    LogIpcToConsole("Queuing message: " + message);
    std::lock_guard<std::mutex> lock(queueMutex);
    messageQueue.push(message);
}

void IpcServer::BroadcastMessage(const std::string& message) {
    LogIpcToConsole("Broadcasting message: " + message);
    SendMessage(message);
}

size_t IpcServer::GetClientCount() const {
    return running ? 1 : 0;
}

std::vector<std::string> IpcServer::GetConnectedClients() const {
    std::vector<std::string> clients;
    if (running) {
        clients.push_back("HttpClient");
    }
    return clients;
}

void IpcServer::ServerLoop() {
    LogIpcToConsole("=== HTTP Server Loop Started ===");
    
    while (running) {
        // 클라이언트 연결 대기 (논블로킹으로 설정)
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(serverSocket, &readfds);
        
        timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000; // 100ms
        
        int selectResult = select(0, &readfds, nullptr, nullptr, &timeout);
        
        if (selectResult > 0 && FD_ISSET(serverSocket, &readfds)) {
            // 새로운 클라이언트 연결
            sockaddr_in clientAddr;
            int clientAddrLen = sizeof(clientAddr);
            
            SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientAddrLen);
            if (clientSocket != INVALID_SOCKET) {
                // 연결 로그는 POST 요청 시에만 출력하도록 HandleClient에서 처리
                
                // 클라이언트 처리를 별도 스레드에서 실행
                std::thread clientThread(&IpcServer::HandleClient, this, clientSocket);
                clientThread.detach();
            }
        } else if (selectResult == SOCKET_ERROR) {
            if (running) {
                LogIpcToConsole("Select error: " + std::to_string(WSAGetLastError()));
            }
            break;
        }
        
        // 큐에 있는 메시지 처리
        ProcessMessages();
    }
    
    LogIpcToConsole("=== HTTP Server Loop Ended ===");
}

void IpcServer::HandleClient(SOCKET clientSocket) {
    char buffer[8192];
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytesReceived > 0) {
        buffer[bytesReceived] = '\0';
        std::string request(buffer);
        
        // HTTP 메서드 확인
        if (request.find("POST") == 0) {
            // POST 요청 처리 - 실제 명령
            LogIpcToConsole("📤 Processing command request (" + std::to_string(bytesReceived) + " bytes)");
            
            std::string body = ParseHttpBody(request);
            if (!body.empty()) {
                LogIpcToConsole("Request: " + body);
                
                // CommandRouter 사용
                if (!g_CommandRouter) {
                    g_CommandRouter = new CommandRouter();
                }
                
                std::string jsonResponse = g_CommandRouter->ExecuteCommand(body);
                std::string httpResponse = CreateHttpResponse(jsonResponse, "application/json");
                
                int bytesSent = send(clientSocket, httpResponse.c_str(), static_cast<int>(httpResponse.length()), 0);
                if (bytesSent > 0) {
                    LogIpcToConsole("📥 Response sent (" + std::to_string(bytesSent) + " bytes)");
                } else {
                    LogIpcToConsole("❌ Failed to send response");
                }
            } else {
                LogIpcToConsole("❌ Empty request body in POST request");
                std::string errorResponse = CreateHttpResponse("{\"success\":false,\"error\":\"Empty request body\"}", "application/json");
                send(clientSocket, errorResponse.c_str(), static_cast<int>(errorResponse.length()), 0);
            }
        } else if (request.find("GET") == 0) {
            // GET 요청 - 상태 확인 (로그 없음, Keep-Alive 체크)
            std::string statusResponse = CreateHttpResponse("{\"status\":\"running\",\"message\":\"Internal Engine IPC Server\"}", "application/json");
            send(clientSocket, statusResponse.c_str(), static_cast<int>(statusResponse.length()), 0);
        } else {
            // 지원하지 않는 메서드
            LogIpcToConsole("❌ Unsupported HTTP method");
            std::string errorResponse = CreateHttpResponse("{\"success\":false,\"error\":\"Method not allowed\"}", "application/json");
            send(clientSocket, errorResponse.c_str(), static_cast<int>(errorResponse.length()), 0);
        }
    } else if (bytesReceived == 0) {
        // 클라이언트가 연결을 정상적으로 종료
    } else {
        // 에러 발생
        LogIpcToConsole("❌ Error receiving data from client");
    }
    
    // 연결 종료 (로그 없음)
    closesocket(clientSocket);
}

std::string IpcServer::ParseHttpBody(const std::string& request) {
    // HTTP 헤더와 바디 구분
    size_t headerEnd = request.find("\r\n\r\n");
    if (headerEnd != std::string::npos) {
        return request.substr(headerEnd + 4);
    }
    return "";
}

std::string IpcServer::CreateHttpResponse(const std::string& body, const std::string& contentType) {
    std::stringstream response;
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: " << contentType << "\r\n";
    response << "Content-Length: " << body.length() << "\r\n";
    response << "Access-Control-Allow-Origin: *\r\n";
    response << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    response << "Access-Control-Allow-Headers: Content-Type\r\n";
    response << "Connection: close\r\n";
    response << "\r\n";
    response << body;
    
    return response.str();
}

void IpcServer::ProcessMessages() {
    std::lock_guard<std::mutex> lock(queueMutex);
    
    while (!messageQueue.empty()) {
        std::string message = messageQueue.front();
        messageQueue.pop();
        
        LogIpcToConsole("Processing queued message: " + message);
        // 필요시 추가 처리
    }
}

std::string IpcServer::HandleMessage(const std::string& message) {
    if (messageHandler) {
        LogIpcToConsole("Handling message with handler: YES");
        return messageHandler(message);
    } else {
        LogIpcToConsole("Handling message with handler: NO");
        return "{\"error\": \"No message handler set\"}";
    }
}

} // namespace InternalEngine 