import { create } from 'zustand'
import { dllConnection, DirectDllConnection, ConnectionStatus as DllConnectionStatus } from '../utils/directDllConnection'
// NamedPipeClient removed - Direct WebSocket communication only

export interface ProcessInfo {
  pid: number
  name: string
  platform?: string
  addressWidth?: number
  processorArchitecture?: number
  pageSize?: number
  workingSetSize: number
  pagefileUsage: number
  peakWorkingSetSize: number
}

export interface Module {
  name: string
  baseAddress: string
  size: number
  path: string
}

export interface Address {
  id: string
  address: string
  value: string
  type: string
  description: string
  frozen: boolean
}

export interface ScanResult {
  address: string
  value: string
}

export type ConnectionStatus = 'connecting' | 'connected' | 'error' | 'disconnected'

interface EngineState {
  // Direct WebSocket connection state (Bridge completely removed)
  isConnected: boolean
  connectionStatus: DllConnectionStatus
  
  // Data state
  processInfo: ProcessInfo | null
  modules: Module[]
  addresses: Address[]
  scanResults: ScanResult[]
  memoryData: string
  disassembly: string
  consoleOutput: string[]
  
  // Polling state
  pollingInterval: number
  pollingTimerId: NodeJS.Timeout | null
  
  // Real-time monitoring
  monitoredValues: Map<string, { type: string; lastValue: string }>
  valueUpdateCallbacks: Map<string, (newValue: string) => void>
  
  // Optimization flags
  isPollingActive: boolean
  pollingCycleCount: number
  
  // Actions (Simplified - WebSocket only)
  connect: () => Promise<void>
  disconnect: () => void
  sendCommand: (command: any) => Promise<any>
  addConsoleOutput: (message: string) => void
  refreshProcessInfo: () => Promise<void>
  refreshModules: () => Promise<void>
  addAddress: (address: Omit<Address, 'id'>) => Promise<void>
  removeAddress: (id: string) => void
  toggleFreeze: (id: string) => void
  updateAddressValue: (id: string, value: string) => void
  scanMemory: (value: string, type: string) => Promise<void>
  readMemory: (address: string, size: number, type?: string) => Promise<number[]>
  writeMemory: (address: string, value: string, type: string) => Promise<boolean>
  validateAddress: (address: string, size?: number) => Promise<any>
  disassembleMemory: (address: string, size: number) => Promise<void>
  executeCommand: (command: string) => Promise<string>
  startPolling: () => void
  stopPolling: () => void
  updatePollingInterval: (interval: number) => void
  readValue: (address: string, type: string) => Promise<string | null>
  
  // Real-time monitoring actions
  startMonitoring: (address: string, type: string, callback?: (newValue: string) => void) => void
  stopMonitoring: (address: string) => void
  updateMonitoredValue: (address: string, newValue: string) => void
}

export const useEngineStore = create<EngineState>((set, get) => ({
  // Initial state - Direct WebSocket only
  isConnected: false,
  connectionStatus: 'disconnected',
  processInfo: null,
  modules: [],
  addresses: [],
  scanResults: [],
  memoryData: '',
  disassembly: '',
  consoleOutput: [],
  pollingInterval: 100, // Very fast polling for real-time updates (100ms)
  pollingTimerId: null,
  
  // Real-time monitoring
  monitoredValues: new Map(),
  valueUpdateCallbacks: new Map(),
  
  // Optimization flags
  isPollingActive: false,
  pollingCycleCount: 0,
  
  connect: async () => {
    console.log('🚀 Connecting directly to DLL WebSocket server...')
    console.log('🔥 Bridge-free architecture: No more intermediate servers!')
    
    // Test if DLL WebSocket server is reachable
    console.log('🔍 Testing DLL WebSocket server availability...')
    const isReachable = await DirectDllConnection.testConnection()
    
    if (!isReachable) {
      console.warn('⚠️ DLL WebSocket server not reachable on port 8765')
      console.log('💡 Make sure the DLL is injected and WebSocket server is running')
      throw new Error('DLL WebSocket server not reachable. Ensure DLL is injected and running.')
    }
    
    console.log('✅ DLL WebSocket server is reachable, connecting...')
    
    set({ 
      connectionStatus: 'connecting',
      isConnected: false 
    })

    try {
      // Set up event handlers
      dllConnection.onStatusChange = (status: DllConnectionStatus) => {
        console.log(`📡 DLL connection status: ${status}`)
        set({ 
          connectionStatus: status,
          isConnected: status === 'connected'
        })
      }
      
      // Set up real-time update handler for maximum performance
      dllConnection.onRealTimeUpdate = (data) => {
        console.log('⚡ Real-time update from DLL:', data)
        
        if (data.type === 'memory_update') {
          // Update scan results in real-time
          const { updateMonitoredValue } = get()
          updateMonitoredValue(data.address, data.value)
        } else if (data.type === 'scan_results') {
          // Handle bulk scan result updates
          console.log('📊 Bulk scan results update received')
        }
      }

      // Connect to DLL WebSocket server
      await dllConnection.connect()
      
      console.log('✅ Direct DLL connection established!')
      console.log('🔥 Bridge eliminated - Direct WebSocket communication active!')
      
      // Initial data fetch
      await get().refreshProcessInfo()
      await get().refreshModules()
      get().startPolling()
      
    } catch (error) {
      console.error('❌ Direct DLL connection failed:', error)
      console.log('🔧 Troubleshooting steps:')
      console.log('  1. Make sure the target process is injected with Internal-Engine.dll')
      console.log('  2. Check the DLL console for WebSocket server startup messages')
      console.log('  3. Verify port 8765 is not blocked by firewall')
      console.log('  4. Try refreshing the page and reconnecting')
      
      set({ 
        connectionStatus: 'error',
        isConnected: false 
      })
      throw error
    }
  },
  
  disconnect: () => {
    console.log('🔌 Disconnecting from DLL WebSocket server...')
    
    // Disconnect from DLL directly
    dllConnection.disconnect()
    
    get().stopPolling()
    
    set({ 
      isConnected: false,
      connectionStatus: 'disconnected',
      processInfo: null,
      modules: [],
      addresses: [],
      scanResults: [],
      memoryData: '',
      disassembly: '',
      consoleOutput: []
    })
    
    console.log('🔥 Direct WebSocket connection closed!')
  },

  sendCommand: async (command: any) => {
    const { isConnected } = get()
    
    if (!isConnected) {
      throw new Error('Not connected to DLL WebSocket server')
    }
    
    try {
      const startTime = Date.now()
      const response = await dllConnection.sendCommand(command)
      const latency = Date.now() - startTime
      
      console.log(`⚡ Direct DLL command completed in ${latency}ms:`, command.command)
      return response
    } catch (error) {
      console.error('❌ Direct DLL command failed:', error)
      throw error
    }
  },
  
  addConsoleOutput: (message: string) => {
    set(state => ({
      consoleOutput: [...state.consoleOutput, `[${new Date().toLocaleTimeString()}] ${message}`]
    }))
  },
  
  refreshProcessInfo: async () => {
    const { isConnected } = get()
    if (!isConnected) return

    try {
      const response = await dllConnection.sendCommand({ command: 'process.info' })
      if (response.success) {
        set({ processInfo: response.data })
        
        // Add console output with connection status
        const status = get().connectionStatus
        if (status === 'connected') {
          get().addConsoleOutput(`✅ Connected to process: ${response.data.name} (PID: ${response.data.pid})`)
        }
      } else {
        get().addConsoleOutput(`❌ Failed to get process info: ${response.error}`)
      }
    } catch (error) {
      get().addConsoleOutput(`❌ Error getting process info: ${error}`)
    }
  },
  
  refreshModules: async () => {
    const { isConnected } = get()
    if (!isConnected) return

    try {
      const response = await dllConnection.sendCommand({ command: 'module.list' })
      if (response.success) {
        set({ modules: response.data })
        get().addConsoleOutput(`📋 Loaded ${response.data.length} modules`)
      } else {
        get().addConsoleOutput(`❌ Failed to get modules: ${response.error}`)
      }
    } catch (error) {
      get().addConsoleOutput(`❌ Error getting modules: ${error}`)
    }
  },
  
  addAddress: async (address: Omit<Address, 'id'>) => {
    const newAddress: Address = {
      ...address,
      id: Date.now().toString(),
    }
    
    // Try to read the initial value
    const { readValue, isConnected } = get()
    if (isConnected && address.value === '???') {
      try {
        const initialValue = await readValue(address.address, address.type)
        if (initialValue !== null) {
          newAddress.value = initialValue
        }
      } catch (error) {
        console.error(`Failed to read initial value for ${address.address}:`, error)
      }
    }
    
    set(state => ({
      addresses: [...state.addresses, newAddress]
    }))
    
    get().addConsoleOutput(`➕ Added address: ${address.address} (${address.description})`)
  },
  
  removeAddress: (id: string) => {
    const address = get().addresses.find(addr => addr.id === id)
    set(state => ({
      addresses: state.addresses.filter(addr => addr.id !== id)
    }))
    
    if (address) {
      get().addConsoleOutput(`➖ Removed address: ${address.address}`)
    }
  },
  
  toggleFreeze: (id: string) => {
    set(state => ({
      addresses: state.addresses.map(addr =>
        addr.id === id ? { ...addr, frozen: !addr.frozen } : addr
      )
    }))
    
    const address = get().addresses.find(addr => addr.id === id)
    if (address) {
      get().addConsoleOutput(`${address.frozen ? '🧊' : '🔥'} ${address.frozen ? 'Froze' : 'Unfroze'} address: ${address.address}`)
    }
  },
  
  updateAddressValue: (id: string, value: string) => {
    set(state => ({
      addresses: state.addresses.map(addr =>
        addr.id === id ? { ...addr, value } : addr
      )
    }))
  },
  
  scanMemory: async (value: string, type: string) => {
    const { client } = get()
    if (!client) return

    get().addConsoleOutput(`🔍 Scanning for ${type} value: ${value}`)
    
    try {
      const response = await client.sendCommand('memory.scan', { value, type })
      if (response.success) {
        set({ scanResults: response.data })
        get().addConsoleOutput(`✅ Scan complete: ${response.data.length} results found`)
      } else {
        get().addConsoleOutput(`❌ Scan failed: ${response.error}`)
      }
    } catch (error) {
      get().addConsoleOutput(`❌ Scan error: ${error}`)
    }
  },
  
  readMemory: async (address: string, size: number, type?: string) => {
    const { isConnected } = get();
    if (!isConnected) return [];

    get().addConsoleOutput(`📖 Reading memory at ${address}, size: ${size}${type ? ` (${type})` : ''}`);
    
    try {
      const response = await dllConnection.sendCommand({ 
        command: 'memory.read',
        address, 
        size, 
        type: type || 'bytes' 
      });
      
      if (response.success) {
        // The DLL returns data as an array of bytes
        const memoryBytes = response.data;
        if (Array.isArray(memoryBytes)) {
          // Convert to hex string for display
          const hexString = memoryBytes.map(byte => 
            byte.toString(16).padStart(2, '0')
          ).join(' ');
          
          set({ memoryData: hexString });
          get().addConsoleOutput(`✅ Memory read successful: ${memoryBytes.length} bytes`);
          
          // Return the raw byte array for MemoryViewer
          return memoryBytes;
        } else {
          get().addConsoleOutput(`❌ Invalid memory data format`);
          return [];
        }
      } else {
        get().addConsoleOutput(`❌ Memory read failed: ${response.error}`);
        return [];
      }
    } catch (error) {
      get().addConsoleOutput(`❌ Memory read error: ${error}`);
      return [];
    }
  },
  
  writeMemory: async (address: string, value: string, type: string) => {
    const { isConnected } = get()
    if (!isConnected) return false

    try {
      const response = await dllConnection.sendCommand({ command: 'memory.write', address, value, type })
      if (response.success) {
        get().addConsoleOutput(`✅ Memory write successful at ${address}`)
        return true
      } else {
        get().addConsoleOutput(`❌ Memory write failed at ${address}: ${response.error}`)
        return false
      }
    } catch (error) {
      get().addConsoleOutput(`❌ Memory write error: ${error}`)
      return false
    }
  },
  
  validateAddress: async (address: string, size?: number) => {
    const { isConnected } = get();
    if (!isConnected) {
      return { valid: false, readable: false, writable: false, error: 'Not connected' };
    }

    try {
      const response = await dllConnection.sendCommand({ 
        command: 'memory.validate',
        address, 
        size: size || 1 
      });
      
      if (response.success) {
        return response.data;
      } else {
        get().addConsoleOutput(`❌ Address validation failed: ${response.error}`);
        return { valid: false, readable: false, writable: false, error: response.error };
      }
    } catch (error) {
      get().addConsoleOutput(`❌ Address validation error: ${error}`);
      return { valid: false, readable: false, writable: false, error: error };
    }
  },
  
  disassembleMemory: async (address: string, size: number) => {
    const { isConnected } = get()
    if (!isConnected) return

    get().addConsoleOutput(`🔧 Disassembling at ${address}, size: ${size}`)
    
    try {
      const response = await dllConnection.sendCommand({ command: 'memory.disassemble', address, size })
      if (response.success) {
        set({ disassembly: response.data })
        get().addConsoleOutput(`✅ Disassembly complete`)
      } else {
        get().addConsoleOutput(`❌ Disassembly failed: ${response.error}`)
      }
    } catch (error) {
      get().addConsoleOutput(`❌ Disassembly error: ${error}`)
    }
  },
  
  executeCommand: async (command: string) => {
    const { isConnected } = get()
    if (!isConnected) return 'No connection'

    get().addConsoleOutput(`> ${command}`)
    
    try {
      const response = await dllConnection.sendCommand({ command: 'console.execute', command })
      const result = response.success ? response.data : `Error: ${response.error}`
      get().addConsoleOutput(result)
      return result
    } catch (error) {
      const errorMsg = `Error: ${error}`
      get().addConsoleOutput(errorMsg)
      return errorMsg
    }
  },

  startPolling: () => {
    const { pollingTimerId, pollingInterval, addresses, isConnected, readValue, writeMemory, updateAddressValue } = get();
    if (pollingTimerId) {
      clearInterval(pollingTimerId);
    }

    if (!isConnected) return;

    const timerId = setInterval(async () => {
      const state = get();
      const { addresses, isConnected, monitoredValues, updateMonitoredValue, pollingCycleCount } = state;
      
      if (!isConnected || state.isPollingActive) return;
      
      // Set polling active flag to prevent overlapping
      set({ isPollingActive: true, pollingCycleCount: pollingCycleCount + 1 });

      try {
        const readPromises: Promise<void>[] = [];
        const writePromises: Promise<void>[] = [];
        
        // Poll address list values (high priority)
        for (const address of addresses) {
          if (address.frozen) {
            writePromises.push(
              writeMemory(address.address, address.value, address.type)
                .catch(() => {})
            );
          } else {
            readPromises.push(
              readValue(address.address, address.type)
                .then(currentValue => {
                  if (currentValue !== null && currentValue !== address.value) {
                    updateAddressValue(address.id, currentValue);
                  }
                })
                .catch(() => {})
            );
          }
        }
        
        // Poll monitored values aggressively for real-time updates
        if (monitoredValues.size > 0) {
          const monitoredArray = Array.from(monitoredValues.entries());
          // Use much larger batch size for unlimited real-time monitoring
          const batchSize = Math.min(200, monitoredArray.length); // Much larger batch
          const startIndex = (pollingCycleCount * batchSize) % monitoredArray.length;
          const currentBatch = monitoredArray.slice(startIndex, startIndex + batchSize);
          
          for (const [monitoredAddress, monitoredData] of currentBatch) {
            readPromises.push(
              readValue(monitoredAddress, monitoredData.type)
                .then(currentValue => {
                  if (currentValue !== null) {
                    updateMonitoredValue(monitoredAddress, currentValue);
                  }
                })
                .catch(() => {})
            );
          }
        }
        
        // Execute operations with timeout
        await Promise.race([
          Promise.all([...readPromises, ...writePromises]),
          new Promise(resolve => setTimeout(resolve, pollingInterval * 0.8)) // 80% of interval as timeout
        ]);
      } finally {
        set({ isPollingActive: false });
      }
    }, pollingInterval);

    set({ pollingTimerId: timerId });
  },

  stopPolling: () => {
    const { pollingTimerId } = get();
    if (pollingTimerId) {
      clearInterval(pollingTimerId);
    }
    set({ pollingTimerId: null });
  },

  updatePollingInterval: (interval: number) => {
    set({ pollingInterval: interval });
    const { startPolling } = get();
    startPolling(); // Restart polling with the new interval
  },

  readValue: async (address: string, type: string) => {
    const { isConnected } = get();
    if (!isConnected) return null;

    try {
        // Map UI type names to DLL type names
        const typeMap: { [key: string]: string } = {
            'int32': 'int',
            'int64': 'int',  // DLL doesn't support int64 yet, use int
            'float': 'float',
            'double': 'double',
            'byte': 'int',   // Read as int for single byte
            'string': 'string',
            'bytes': 'bytes'
        };
        
        const dllType = typeMap[type] || 'int';
        const sizeMap: { [key: string]: number } = {
            'int32': 4, 'int64': 8, 'float': 4, 'double': 8, 'byte': 1, 'string': 256, 'bytes': 256
        };
        const size = sizeMap[type] || 4;

        const response = await dllConnection.sendCommand({ command: 'memory.read_value', address, type: dllType });
        if (response.success) {
            // Parse based on type
            if (dllType === 'bytes') {
                // Convert byte array to hex string
                const bytes = response.data as number[];
                return bytes.map(b => b.toString(16).padStart(2, '0')).join(' ');
            }
            return response.data.toString();
        }
        return null;
    } catch (error) {
        console.error(`Error reading value at ${address}:`, error);
        return null;
    }
  },

  // Real-time monitoring functions
  startMonitoring: (address: string, type: string, callback?: (newValue: string) => void) => {
    const { monitoredValues, valueUpdateCallbacks } = get();
    
    // Add to monitoring list
    monitoredValues.set(address, { type, lastValue: '' });
    
    // Add callback if provided
    if (callback) {
      valueUpdateCallbacks.set(address, callback);
    }
    
    set({ monitoredValues, valueUpdateCallbacks });
  },

  stopMonitoring: (address: string) => {
    const { monitoredValues, valueUpdateCallbacks } = get();
    
    monitoredValues.delete(address);
    valueUpdateCallbacks.delete(address);
    
    set({ monitoredValues, valueUpdateCallbacks });
  },

  updateMonitoredValue: (address: string, newValue: string) => {
    const { monitoredValues, valueUpdateCallbacks } = get();
    
    const monitored = monitoredValues.get(address);
    if (monitored && monitored.lastValue !== newValue) {
      // Update the stored value
      monitoredValues.set(address, { ...monitored, lastValue: newValue });
      
      // Call the callback if it exists
      const callback = valueUpdateCallbacks.get(address);
      if (callback) {
        callback(newValue);
      }
      
      set({ monitoredValues });
    }
  },
}))