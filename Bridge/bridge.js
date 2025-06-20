const WebSocket = require('ws');
const http = require('http');

// Configuration
const WEBSOCKET_PORT = 9877;
const DLL_HTTP_PORT = 8767; // Updated to match new IPC server port
const DLL_HTTP_URL = `http://127.0.0.1:${DLL_HTTP_PORT}`;

// Connection state
let connected = false;
let connectedClients = new Set();
let httpAgent = null; // Keep-Alive 연결을 위한 Agent

// Create WebSocket server
const wss = new WebSocket.Server({ port: WEBSOCKET_PORT });

console.log(`WebSocket bridge listening on port ${WEBSOCKET_PORT}`);
console.log(`Will connect to DLL HTTP server at ${DLL_HTTP_URL}`);

// Keep-Alive Agent 생성
const createHttpAgent = () => {
    if (httpAgent) {
        httpAgent.destroy();
    }
    
    httpAgent = new http.Agent({
        keepAlive: true,
        keepAliveMsecs: 30000, // 30초 동안 연결 유지
        maxSockets: 5,
        maxFreeSockets: 2,
        timeout: 10000
    });
    
    console.log('Created new HTTP Agent with Keep-Alive');
};

// 초기 Agent 생성
createHttpAgent();

// DLL에 명령 전송
const sendToDll = async (command) => {
    try {
        const postData = JSON.stringify(command);
        
        const options = {
            hostname: '127.0.0.1',
            port: DLL_HTTP_PORT,
            path: '/',
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'Content-Length': Buffer.byteLength(postData),
                'Connection': 'keep-alive'
            },
            agent: httpAgent // Keep-Alive Agent 사용
        };

        return new Promise((resolve, reject) => {
            const req = http.request(options, (res) => {
                let data = '';
                
                res.on('data', (chunk) => {
                    data += chunk;
                });
                
                res.on('end', () => {
                    try {
                        const response = JSON.parse(data);
                        // 성공적인 응답 시 연결 상태 확인
                        if (!connected) {
                            connected = true;
                            console.log('✅ Connection established to DLL HTTP server');
                            broadcastToClients({ type: 'connected' });
                        }
                        resolve(response);
                    } catch (parseError) {
                        reject(new Error('Failed to parse response: ' + parseError.message));
                    }
                });
            });

            req.on('error', (error) => {
                // 연결 오류 시 상태 업데이트
                if (connected) {
                    connected = false;
                    console.log('❌ Connection lost to DLL HTTP server');
                    broadcastToClients({ type: 'disconnected' });
                    
                    // Agent 재생성 (연결 풀 리셋)
                    createHttpAgent();
                }
                reject(error);
            });

            req.on('timeout', () => {
                req.destroy();
                reject(new Error('Request timeout'));
            });

            req.setTimeout(10000);
            req.write(postData);
            req.end();
        });
    } catch (error) {
        throw error;
    }
};

// 연결 상태 확인 (매우 가끔만 실행)
const checkConnection = async () => {
    // 이미 연결된 상태라면 확인하지 않음
    if (connected) {
        return;
    }
    
    console.log('🔍 Checking DLL connection status...');
    
    try {
        const options = {
            hostname: '127.0.0.1',
            port: DLL_HTTP_PORT,
            path: '/',
            method: 'GET',
            timeout: 3000,
            agent: httpAgent
        };

        const req = http.request(options, (res) => {
            if (res.statusCode === 200) {
                if (!connected) {
                    connected = true;
                    console.log('✅ DLL HTTP server detected');
                    broadcastToClients({ type: 'connected' });
                }
            }
        });

        req.on('error', () => {
            if (connected) {
                connected = false;
                console.log('❌ DLL HTTP server not responding');
                broadcastToClients({ type: 'disconnected' });
            }
        });

        req.setTimeout(3000);
        req.end();
    } catch (error) {
        if (connected) {
            connected = false;
            broadcastToClients({ type: 'disconnected' });
        }
    }
};

const broadcastToClients = (message) => {
    const messageStr = JSON.stringify(message);
    connectedClients.forEach(client => {
        if (client.readyState === WebSocket.OPEN) {
            client.send(messageStr);
        }
    });
};

// 연결 확인을 60초마다만 실행 (매우 가끔)
setInterval(checkConnection, 60000);

// 초기 연결 확인
setTimeout(checkConnection, 1000); // 1초 후에 확인

wss.on('connection', (ws) => {
    console.log('🔌 WebSocket client connected');
    connectedClients.add(ws);
    
    // 현재 연결 상태 전송
    if (connected) {
        ws.send(JSON.stringify({ type: 'connected' }));
    } else {
        ws.send(JSON.stringify({ type: 'connecting' }));
        // 연결되지 않은 상태에서 새 클라이언트가 연결되면 즉시 확인
        checkConnection();
    }

    ws.on('message', async (message) => {
        try {
            const data = JSON.parse(message.toString());
            
            // 실제 명령 실행 시에만 로그 출력
            console.log('📤 Sending command to DLL:', data.command);
            
            try {
                const response = await sendToDll(data);
                console.log('📥 Received response from DLL');
                ws.send(JSON.stringify(response));
            } catch (error) {
                console.error('❌ HTTP communication error:', error.message);
                ws.send(JSON.stringify({
                    id: data.id,
                    success: false,
                    error: error.message
                }));
                
                // 연결 오류 시 상태 업데이트
                if (error.code === 'ECONNREFUSED' || error.code === 'ENOTFOUND') {
                    connected = false;
                    broadcastToClients({ type: 'disconnected' });
                }
            }
            
        } catch (error) {
            console.error('Error processing WebSocket message:', error);
            ws.send(JSON.stringify({
                success: false,
                error: 'Failed to process message: ' + error.message
            }));
        }
    });
    
    ws.on('close', () => {
        console.log('🔌 WebSocket client disconnected');
        connectedClients.delete(ws);
    });
    
    ws.on('error', (error) => {
        console.error('WebSocket error:', error);
        connectedClients.delete(ws);
    });
});

wss.on('error', (error) => {
    console.error('WebSocket server error:', error);
});

console.log('🚀 Bridge ready with optimized HTTP communication!');
console.log('Benefits:');
console.log('  ✅ Keep-Alive connections (no frequent connect/disconnect)');
console.log('  ✅ Minimal connection checking');
console.log('  ✅ Efficient connection pooling');
console.log('  ✅ Reduced DLL log spam');

// 정리 함수
process.on('SIGINT', () => {
    console.log('Shutting down bridge...');
    if (httpAgent) {
        httpAgent.destroy();
    }
    process.exit(0);
});