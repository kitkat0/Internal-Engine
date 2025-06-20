// Ultra-high performance Web Worker for background memory monitoring
// This offloads memory value updates from the main thread for maximum UI responsiveness

interface MonitoringRequest {
  type: 'start_monitoring' | 'stop_monitoring' | 'update_value' | 'batch_update'
  address?: string
  valueType?: string
  value?: string
  addresses?: string[]
  values?: { address: string; value: string }[]
}

interface MonitoringResponse {
  type: 'value_updated' | 'batch_updated' | 'monitoring_started' | 'monitoring_stopped'
  address?: string
  value?: string
  updates?: { address: string; value: string }[]
}

class MemoryWorker {
  private monitoredAddresses = new Map<string, { type: string; lastValue: string; lastUpdate: number }>()
  private updateQueue: { address: string; value: string }[] = []
  private batchProcessingInterval: number = 0
  
  constructor() {
    this.startBatchProcessor()
  }
  
  private startBatchProcessor() {
    // Ultra-fast batch processing for maximum performance
    this.batchProcessingInterval = setInterval(() => {
      if (this.updateQueue.length > 0) {
        const updates = [...this.updateQueue]
        this.updateQueue = []
        
        this.postMessage({
          type: 'batch_updated',
          updates
        })
      }
    }, 16) // 60fps batch processing
  }
  
  handleMessage(event: MessageEvent<MonitoringRequest>) {
    const { type, address, valueType, value, addresses, values } = event.data
    
    switch (type) {
      case 'start_monitoring':
        if (address && valueType) {
          this.monitoredAddresses.set(address, {
            type: valueType,
            lastValue: '',
            lastUpdate: 0
          })
          
          this.postMessage({
            type: 'monitoring_started',
            address
          })
        }
        break
        
      case 'stop_monitoring':
        if (address) {
          this.monitoredAddresses.delete(address)
          
          this.postMessage({
            type: 'monitoring_stopped',
            address
          })
        }
        break
        
      case 'update_value':
        if (address && value !== undefined) {
          const monitored = this.monitoredAddresses.get(address)
          if (monitored) {
            const now = performance.now()
            
            // Only process if value changed and enough time passed
            if (monitored.lastValue !== value && (now - monitored.lastUpdate) >= 16) {
              monitored.lastValue = value
              monitored.lastUpdate = now
              
              // Queue for batch processing
              this.updateQueue.push({ address, value })
            }
          }
        }
        break
        
      case 'batch_update':
        if (values) {
          const now = performance.now()
          const processedUpdates: { address: string; value: string }[] = []
          
          for (const update of values) {
            const monitored = this.monitoredAddresses.get(update.address)
            if (monitored && monitored.lastValue !== update.value && (now - monitored.lastUpdate) >= 16) {
              monitored.lastValue = update.value
              monitored.lastUpdate = now
              processedUpdates.push(update)
            }
          }
          
          if (processedUpdates.length > 0) {
            this.postMessage({
              type: 'batch_updated',
              updates: processedUpdates
            })
          }
        }
        break
    }
  }
  
  private postMessage(message: MonitoringResponse) {
    self.postMessage(message)
  }
}

// Initialize worker
const worker = new MemoryWorker()

// Handle messages from main thread
self.addEventListener('message', (event) => {
  worker.handleMessage(event)
})

// Export for TypeScript
export {}