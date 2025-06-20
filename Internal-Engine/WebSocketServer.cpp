#include "WebSocketServer.hpp"
#include "CommandRouter.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <regex>
#include <wincrypt.h>
#include <iostream>

// WebSocket handshake with proper crypto
#pragma comment(lib, "crypt32.lib")

namespace InternalEngine {

// Global instance
WebSocketServer* g_WebSocketServer = nullptr;

// WebSocket magic string for handshake
const std::string WEBSOCKET_MAGIC = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

// WebSocketConnection Implementation
WebSocketConnection::WebSocketConnection(SOCKET socket, const std::string& clientAddr)
    : clientSocket(socket), clientAddress(clientAddr), state(WebSocketState::OPEN) {
}

WebSocketConnection::~WebSocketConnection() {
    Close();
}

bool WebSocketConnection::SendText(const std::string& text) {
    std::vector<uint8_t> payload(text.begin(), text.end());
    return SendFrame(WebSocketOpcode::TEXT, payload);
}

bool WebSocketConnection::SendBinary(const std::vector<uint8_t>& data) {
    return SendFrame(WebSocketOpcode::BINARY, data);
}

bool WebSocketConnection::SendPing() {
    return SendFrame(WebSocketOpcode::PING, {});
}

bool WebSocketConnection::SendPong() {
    return SendFrame(WebSocketOpcode::PONG, {});
}

void WebSocketConnection::Close() {
    if (state != WebSocketState::CLOSED) {
        state = WebSocketState::CLOSED;
        if (clientSocket != INVALID_SOCKET) {
            closesocket(clientSocket);
            clientSocket = INVALID_SOCKET;
        }
    }
}

bool WebSocketConnection::SendFrame(WebSocketOpcode opcode, const std::vector<uint8_t>& payload) {
    if (state != WebSocketState::OPEN) return false;
    
    std::lock_guard<std::mutex> lock(sendMutex);
    
    auto frame = CreateFrame(opcode, payload);
    int sent = send(clientSocket, reinterpret_cast<const char*>(frame.data()), static_cast<int>(frame.size()), 0);
    
    return sent == static_cast<int>(frame.size());
}

std::vector<uint8_t> WebSocketConnection::CreateFrame(WebSocketOpcode opcode, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> frame;
    
    // First byte: FIN=1, opcode
    frame.push_back(0x80 | static_cast<uint8_t>(opcode));
    
    // Payload length encoding
    size_t payloadLen = payload.size();
    if (payloadLen < 126) {
        frame.push_back(static_cast<uint8_t>(payloadLen));
    } else if (payloadLen <= 65535) {
        frame.push_back(126);
        frame.push_back(static_cast<uint8_t>(payloadLen >> 8));
        frame.push_back(static_cast<uint8_t>(payloadLen & 0xFF));
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; i--) {
            frame.push_back(static_cast<uint8_t>((payloadLen >> (i * 8)) & 0xFF));
        }
    }
    
    // Payload
    frame.insert(frame.end(), payload.begin(), payload.end());
    
    return frame;
}

// WebSocketServer Implementation
WebSocketServer::WebSocketServer() 
    : serverSocket(INVALID_SOCKET), running(false), stopping(false) {
    
    // Initialize Winsock
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
}

WebSocketServer::~WebSocketServer() {
    Stop();
    WSACleanup();
}

bool WebSocketServer::Start(int port) {
    if (running) return false;
    
    // Create server socket
    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        return false;
    }
    
    // Allow socket reuse
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
    
    // Bind to port
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<unsigned short>(port));
    
    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        closesocket(serverSocket);
        return false;
    }
    
    // Start listening
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(serverSocket);
        return false;
    }
    
    running = true;
    stopping = false;
    
    // Start accept thread
    acceptThread = std::thread(&WebSocketServer::AcceptLoop, this);
    
    // Start message processor thread for performance
    messageProcessor = std::thread(&WebSocketServer::ProcessMessages, this);
    
    return true;
}

void WebSocketServer::Stop() {
    if (!running) return;
    
    stopping = true;
    running = false;
    
    // Close server socket
    if (serverSocket != INVALID_SOCKET) {
        closesocket(serverSocket);
        serverSocket = INVALID_SOCKET;
    }
    
    // Wait for threads to finish
    if (acceptThread.joinable()) {
        acceptThread.join();
    }
    
    if (messageProcessor.joinable()) {
        messageProcessor.join();
    }
    
    for (auto& worker : workerThreads) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workerThreads.clear();
    
    // Close all connections
    {
        std::lock_guard<std::mutex> lock(connectionsMutex);
        connections.clear();
    }
}

void WebSocketServer::AcceptLoop() {
    while (running && !stopping) {
        sockaddr_in clientAddr;
        int clientAddrLen = sizeof(clientAddr);
        
        SOCKET clientSocket = accept(serverSocket, reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrLen);
        
        if (clientSocket == INVALID_SOCKET) {
            if (running) {
                // Accept error, but continue if server is still running
                Sleep(10);
            }
            continue;
        }
        
        // Get client address string (Windows compatible)
        std::string clientAddrString = std::string(inet_ntoa(clientAddr.sin_addr)) + ":" + std::to_string(ntohs(clientAddr.sin_port));
        
        // Process client in separate thread for high performance
        workerThreads.emplace_back(&WebSocketServer::ProcessClient, this, clientSocket, clientAddrString);
    }
}

void WebSocketServer::ProcessClient(SOCKET clientSocket, const std::string& clientAddr) {
    // Perform WebSocket handshake
    if (!PerformHandshake(clientSocket)) {
        closesocket(clientSocket);
        return;
    }
    
    // Create connection object
    auto connection = std::make_unique<WebSocketConnection>(clientSocket, clientAddr);
    WebSocketConnection* connPtr = connection.get();
    
    // Add to connections list
    {
        std::lock_guard<std::mutex> lock(connectionsMutex);
        connections.push_back(std::move(connection));
    }
    
    // Notify connection handler
    if (connectionHandler) {
        connectionHandler(connPtr, true);
    }
    
    // Process messages from this client
    while (running && connPtr->IsConnected()) {
        try {
            WebSocketFrame frame = ReadFrame(clientSocket);
            
            if (frame.opcode == WebSocketOpcode::TEXT && messageHandler) {
                std::string message(frame.payload.begin(), frame.payload.end());
                
                // Process command and send response
                std::string response = messageHandler(message, connPtr);
                if (!response.empty()) {
                    connPtr->SendText(response);
                }
            } else if (frame.opcode == WebSocketOpcode::PING) {
                connPtr->SendPong();
            } else if (frame.opcode == WebSocketOpcode::CLOSE) {
                break;
            }
        } catch (...) {
            // Connection error, break the loop
            break;
        }
    }
    
    // Notify disconnection
    if (connectionHandler) {
        connectionHandler(connPtr, false);
    }
    
    // Remove from connections list
    RemoveConnection(connPtr);
}

bool WebSocketServer::PerformHandshake(SOCKET clientSocket) {
    // Read HTTP request
    char buffer[4096];
    int received = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (received <= 0) {
        OutputDebugStringA("[WebSocket] Failed to receive handshake request\n");
        return false;
    }
    
    buffer[received] = '\0';
    std::string request(buffer);
    
    // Debug: Log the request
    std::cout << "[WebSocket] Handshake request received" << std::endl;
    OutputDebugStringA(("[WebSocket] Received request:\n" + request + "\n").c_str());
    
    // Validate WebSocket request headers
    if (request.find("GET ") != 0) {
        OutputDebugStringA("[WebSocket] Not a GET request\n");
        return false;
    }
    
    if (request.find("Upgrade: websocket") == std::string::npos && 
        request.find("upgrade: websocket") == std::string::npos) {
        OutputDebugStringA("[WebSocket] Missing Upgrade: websocket header\n");
        return false;
    }
    
    if (request.find("Connection: Upgrade") == std::string::npos && 
        request.find("connection: upgrade") == std::string::npos) {
        OutputDebugStringA("[WebSocket] Missing Connection: Upgrade header\n");
        return false;
    }
    
    // Parse WebSocket key - improved parsing
    std::string clientKey;
    size_t keyPos = request.find("Sec-WebSocket-Key:");
    if (keyPos == std::string::npos) {
        keyPos = request.find("sec-websocket-key:");
    }
    
    if (keyPos != std::string::npos) {
        size_t valueStart = keyPos + 18; // Length of "Sec-WebSocket-Key:"
        while (valueStart < request.length() && (request[valueStart] == ' ' || request[valueStart] == '\t')) {
            valueStart++;
        }
        size_t valueEnd = request.find('\r', valueStart);
        if (valueEnd == std::string::npos) {
            valueEnd = request.find('\n', valueStart);
        }
        if (valueEnd != std::string::npos) {
            clientKey = request.substr(valueStart, valueEnd - valueStart);
            // Trim trailing whitespace
            while (!clientKey.empty() && (clientKey.back() == ' ' || clientKey.back() == '\t')) {
                clientKey.pop_back();
            }
        }
    }
    
    if (clientKey.empty()) {
        OutputDebugStringA("[WebSocket] No Sec-WebSocket-Key found in request\n");
        return false;
    }
    
    // Generate accept key
    std::string acceptKey = GenerateWebSocketKey(clientKey);
    
    // Debug: Log the keys and intermediate values
    OutputDebugStringA(("[WebSocket] Client key: '" + clientKey + "'\n").c_str());
    std::string combined = clientKey + WEBSOCKET_MAGIC;
    OutputDebugStringA(("[WebSocket] Combined string: '" + combined + "'\n").c_str());
    OutputDebugStringA(("[WebSocket] Accept key: '" + acceptKey + "'\n").c_str());
    
    // Send handshake response
    std::string response = 
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + acceptKey + "\r\n"
        "\r\n";
    
    // Debug: Log the response
    OutputDebugStringA(("[WebSocket] Sending response:\n" + response + "\n").c_str());
    
    int sent = send(clientSocket, response.c_str(), static_cast<int>(response.length()), 0);
    
    bool success = sent == static_cast<int>(response.length());
    OutputDebugStringA(success ? "[WebSocket] Handshake successful\n" : "[WebSocket] Handshake failed\n");
    
    return success;
}

// Proper SHA1 implementation for WebSocket handshake
std::string SimpleSHA1(const std::string& input) {
    // Use Windows CryptoAPI for proper SHA1
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    std::string result;
    
    if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        if (CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash)) {
            if (CryptHashData(hHash, (BYTE*)input.c_str(), (DWORD)input.length(), 0)) {
                DWORD hashSize = 20; // SHA1 is 20 bytes
                BYTE hashData[20];
                if (CryptGetHashParam(hHash, HP_HASHVAL, hashData, &hashSize, 0)) {
                    // Return binary data directly (not hex string)
                    result = std::string(reinterpret_cast<char*>(hashData), 20);
                }
            }
            CryptDestroyHash(hHash);
        }
        CryptReleaseContext(hProv, 0);
    }
    
    return result;
}

// Proper Base64 encoding
std::string SimpleBase64Encode(const std::string& input) {
    const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    
    int val = 0, valb = -6;
    for (unsigned char c : input) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            result.push_back(chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) {
        result.push_back(chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    while (result.size() % 4) {
        result.push_back('=');
    }
    return result;
}

std::string WebSocketServer::GenerateWebSocketKey(const std::string& clientKey) {
    // Proper WebSocket key generation according to RFC 6455
    std::string combined = clientKey + WEBSOCKET_MAGIC;
    
    // Generate SHA1 hash (returns binary data directly)
    std::string binaryHash = SimpleSHA1(combined);
    
    // Encode in Base64
    return SimpleBase64Encode(binaryHash);
}

WebSocketFrame WebSocketServer::ReadFrame(SOCKET clientSocket) {
    WebSocketFrame frame;
    
    // Read first two bytes
    uint8_t header[2];
    if (recv(clientSocket, reinterpret_cast<char*>(header), 2, 0) != 2) {
        throw std::runtime_error("Failed to read frame header");
    }
    
    frame.opcode = static_cast<WebSocketOpcode>(header[0] & 0x0F);
    frame.masked = (header[1] & 0x80) != 0;
    
    // Read payload length
    uint64_t payloadLen = header[1] & 0x7F;
    if (payloadLen == 126) {
        uint8_t extLen[2];
        if (recv(clientSocket, reinterpret_cast<char*>(extLen), 2, 0) != 2) {
            throw std::runtime_error("Failed to read extended length");
        }
        payloadLen = (extLen[0] << 8) | extLen[1];
    } else if (payloadLen == 127) {
        uint8_t extLen[8];
        if (recv(clientSocket, reinterpret_cast<char*>(extLen), 8, 0) != 8) {
            throw std::runtime_error("Failed to read extended length");
        }
        payloadLen = 0;
        for (int i = 0; i < 8; i++) {
            payloadLen = (payloadLen << 8) | extLen[i];
        }
    }
    
    frame.payloadLength = payloadLen;
    
    // Read mask if present
    uint8_t mask[4] = {0};
    if (frame.masked) {
        if (recv(clientSocket, reinterpret_cast<char*>(mask), 4, 0) != 4) {
            throw std::runtime_error("Failed to read mask");
        }
    }
    
    // Read payload
    frame.payload.resize(static_cast<size_t>(payloadLen));
    if (payloadLen > 0) {
        int totalReceived = 0;
        while (totalReceived < static_cast<int>(payloadLen)) {
            int received = recv(clientSocket, 
                              reinterpret_cast<char*>(frame.payload.data()) + totalReceived, 
                              static_cast<int>(payloadLen) - totalReceived, 0);
            if (received <= 0) {
                throw std::runtime_error("Failed to read payload");
            }
            totalReceived += received;
        }
        
        // Unmask payload if needed
        if (frame.masked) {
            for (size_t i = 0; i < frame.payload.size(); i++) {
                frame.payload[i] ^= mask[i % 4];
            }
        }
    }
    
    return frame;
}

void WebSocketServer::BroadcastText(const std::string& text) {
    std::lock_guard<std::mutex> lock(connectionsMutex);
    for (auto& conn : connections) {
        if (conn->IsConnected()) {
            conn->SendText(text);
        }
    }
}

void WebSocketServer::BroadcastBinary(const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(connectionsMutex);
    for (auto& conn : connections) {
        if (conn->IsConnected()) {
            conn->SendBinary(data);
        }
    }
}

void WebSocketServer::StreamScanResults(const std::string& jsonData) {
    // High-performance streaming for scan results
    std::string message = R"({"type":"scan_results","data":)" + jsonData + "}";
    BroadcastText(message);
}

void WebSocketServer::StreamMemoryUpdate(const std::string& address, const std::string& newValue) {
    // Real-time memory value updates
    std::string message = R"({"type":"memory_update","address":")" + address + 
                         R"(","value":")" + newValue + R"("})";
    BroadcastText(message);
}

size_t WebSocketServer::GetConnectionCount() const {
    std::lock_guard<std::mutex> lock(connectionsMutex);
    return connections.size();
}

std::vector<WebSocketConnection*> WebSocketServer::GetConnections() const {
    std::lock_guard<std::mutex> lock(connectionsMutex);
    std::vector<WebSocketConnection*> result;
    for (const auto& conn : connections) {
        result.push_back(conn.get());
    }
    return result;
}

void WebSocketServer::ProcessMessages() {
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        
        std::lock_guard<std::mutex> lock(queueMutex);
        while (!messageQueue.empty()) {
            std::string message = messageQueue.front();
            messageQueue.pop();
            BroadcastText(message);
        }
    }
}

void WebSocketServer::RemoveConnection(WebSocketConnection* conn) {
    std::lock_guard<std::mutex> lock(connectionsMutex);
    connections.erase(
        std::remove_if(connections.begin(), connections.end(),
                      [conn](const std::unique_ptr<WebSocketConnection>& c) {
                          return c.get() == conn;
                      }),
        connections.end());
}

} // namespace InternalEngine