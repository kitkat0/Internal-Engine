// Since browsers can't directly access Named Pipes, we'll use a WebSocket bridge
// The bridge server connects to the Named Pipe and forwards messages

export type ConnectionStatus = 'connecting' | 'connected' | 'error' | 'disconnected';

interface CommandResponse {
  id: string;
  success: boolean;
  data?: any;
  error?: string;
}

export class NamedPipeClient {
  private ws: WebSocket | null = null;
  private connected = false;
  private connectionStatus: ConnectionStatus = 'disconnected';
  private pendingRequests = new Map<string, { resolve: Function; reject: Function }>();
  private requestId = 0;
  private reconnectTimer: NodeJS.Timeout | null = null;
  private reconnectAttempts = 0;
  private maxReconnectAttempts = 10;
  private reconnectDelay = 2000;
  private isReconnecting = false;
  
  // Callback for connection status changes
  public onConnectionStatusChange?: (status: ConnectionStatus) => void;

  private updateConnectionStatus(status: ConnectionStatus) {
    if (this.connectionStatus !== status) {
      this.connectionStatus = status;
      console.log(`Connection status changed to: ${status}`);
      if (this.onConnectionStatusChange) {
        this.onConnectionStatusChange(status);
      }
    }
  }

  async connect(): Promise<void> {
    return new Promise((resolve, reject) => {
      this.updateConnectionStatus('connecting');
      this.isReconnecting = false;
      
      try {
        this.ws = new WebSocket('ws://localhost:9877');
        
        let connectTimeout = setTimeout(() => {
          if (!this.connected) {
            this.ws?.close();
            reject(new Error('Connection timeout'));
          }
        }, 5000);
        
        this.ws.onopen = () => {
          console.log('Connected to WebSocket bridge');
          clearTimeout(connectTimeout);
          this.reconnectAttempts = 0;
          // Don't set connected=true yet, wait for bridge status
        };
        
        this.ws.onmessage = (event) => {
          try {
            const message = JSON.parse(event.data);
            
            // Handle connection status messages
            if (message.type) {
              switch (message.type) {
                case 'connected':
                  console.log('Bridge connected to DLL');
                  this.connected = true;
                  this.updateConnectionStatus('connected');
                  clearTimeout(connectTimeout);
                  resolve();
                  break;
                  
                case 'error':
                  console.error('Bridge connection error:', message.message);
                  this.connected = false;
                  this.updateConnectionStatus('error');
                  // Don't reject immediately, keep trying to reconnect
                  break;
                  
                case 'disconnected':
                  console.log('Bridge disconnected from DLL');
                  this.connected = false;
                  this.updateConnectionStatus('error');
                  break;
              }
              return;
            }
            
            // Handle command responses
            if (message.id && this.pendingRequests.has(message.id)) {
              const { resolve: resolveRequest } = this.pendingRequests.get(message.id)!;
              this.pendingRequests.delete(message.id);
              resolveRequest(message);
            }
          } catch (error) {
            console.error('Error parsing WebSocket message:', error);
          }
        };
        
        this.ws.onclose = (event) => {
          console.log('WebSocket connection closed', event.code, event.reason);
          this.connected = false;
          clearTimeout(connectTimeout);
          
          // Reject all pending requests
          this.pendingRequests.forEach(({ reject }) => {
            reject(new Error('Connection closed'));
          });
          this.pendingRequests.clear();
          
          // Only attempt reconnect if we were previously connected
          if (this.connectionStatus === 'connected' && !this.isReconnecting) {
            this.updateConnectionStatus('disconnected');
            this.attemptReconnect();
          } else {
            this.updateConnectionStatus('disconnected');
          }
        };
        
        this.ws.onerror = (error) => {
          console.error('WebSocket error:', error);
          clearTimeout(connectTimeout);
          this.updateConnectionStatus('error');
          if (!this.connected && !this.isReconnecting) {
            reject(new Error('WebSocket connection failed'));
          }
        };
        
      } catch (error) {
        this.updateConnectionStatus('error');
        reject(error);
      }
    });
  }

  disconnect(): void {
    this.isReconnecting = false;
    if (this.reconnectTimer) {
      clearTimeout(this.reconnectTimer);
      this.reconnectTimer = null;
    }
    if (this.ws) {
      this.ws.close();
      this.ws = null;
    }
    this.connected = false;
    this.updateConnectionStatus('disconnected');
  }
  
  private attemptReconnect(): void {
    if (this.isReconnecting || this.reconnectAttempts >= this.maxReconnectAttempts) {
      return;
    }
    
    this.isReconnecting = true;
    this.reconnectAttempts++;
    
    console.log(`Attempting to reconnect (${this.reconnectAttempts}/${this.maxReconnectAttempts})...`);
    
    this.reconnectTimer = setTimeout(async () => {
      try {
        await this.connect();
        console.log('Reconnection successful');
      } catch (error) {
        console.error('Reconnection failed:', error);
        if (this.reconnectAttempts < this.maxReconnectAttempts) {
          this.attemptReconnect();
        } else {
          console.error('Max reconnection attempts reached');
          this.updateConnectionStatus('error');
        }
      }
    }, this.reconnectDelay);
  }

  async sendCommand(command: string, params: any = {}): Promise<CommandResponse> {
    if (!this.connected || !this.ws || this.ws.readyState !== WebSocket.OPEN) {
      throw new Error('Not connected to DLL via bridge');
    }

    return new Promise((resolve, reject) => {
      const id = (++this.requestId).toString();
      const message = { id, command, ...params };
      
      // Store the promise resolvers
      this.pendingRequests.set(id, { resolve, reject });
      
      // Set timeout
      setTimeout(() => {
        if (this.pendingRequests.has(id)) {
          this.pendingRequests.delete(id);
          reject(new Error('Request timeout'));
        }
      }, 10000); // 10 second timeout
      
      // Send the message
      try {
        this.ws!.send(JSON.stringify(message));
      } catch (error) {
        this.pendingRequests.delete(id);
        reject(error);
      }
    });
  }

  isConnected(): boolean {
    return this.connected;
  }
  
  getConnectionStatus(): ConnectionStatus {
    return this.connectionStatus;
  }
}

// Helper function to format bytes as hex
export function bytesToHex(bytes: number[]): string {
  return bytes.map(b => b.toString(16).padStart(2, '0').toUpperCase()).join(' ')
}

// Helper function to parse hex string to bytes
export function hexToBytes(hex: string): number[] {
  const cleanHex = hex.replace(/\s/g, '')
  const bytes = []
  for (let i = 0; i < cleanHex.length; i += 2) {
    bytes.push(parseInt(cleanHex.substr(i, 2), 16))
  }
  return bytes
}