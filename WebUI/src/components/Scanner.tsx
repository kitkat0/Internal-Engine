import React from 'react'
import { 
  FiSearch, FiRefreshCw, FiPlus, FiFilter, FiTarget, 
  FiSettings, FiZap, FiLayers, FiCpu, FiDatabase,
  FiCopy, FiEdit, FiTrash2, FiCornerDownRight, FiCheck
} from 'react-icons/fi'
import { useEngineStore } from '../store/engineStore'
import { useWindowManagerStore } from '../store/windowManager'
import { dllConnection } from '../utils/directDllConnection'
import ContextMenu, { useContextMenu, ContextMenuItem } from './ContextMenu'

interface ScanResult {
  address: string
  value: string
  previousValue?: string
  module?: string
  originalValue?: string  // Store the original value from scan
  hasChanged?: boolean    // Track if value has changed since scan
}

export default function Scanner() {
  // Scan configuration
  const [scanValue, setScanValue] = React.useState('')
  const [scanType, setScanType] = React.useState<'exact' | 'fuzzy' | 'unknown' | 'increased' | 'decreased' | 'changed' | 'unchanged'>('exact')
  const [valueType, setValueType] = React.useState<'int32' | 'int64' | 'float' | 'double' | 'string' | 'bytes'>('int32')
  const [scanResults, setScanResults] = React.useState<ScanResult[]>([])
  const [selectedResults, setSelectedResults] = React.useState<Set<string>>(new Set())
  const [isScanning, setIsScanning] = React.useState(false)
  const [scanCount, setScanCount] = React.useState(0)
  
  // Context menu
  const { contextMenu, handleContextMenu, closeContextMenu } = useContextMenu()
  const [editingResult, setEditingResult] = React.useState<{ address: string; value: string } | null>(null)
  
  // Memory scan options
  const processInfo = useEngineStore(state => state.processInfo)
  const is64Bit = processInfo?.addressWidth === 64
  
  const [memoryOptions, setMemoryOptions] = React.useState({
    writable: 'yes' as 'yes' | 'no' | 'any',
    executable: 'any' as 'yes' | 'no' | 'any',
    copyOnWrite: 'any' as 'yes' | 'no' | 'any',
    startAddress: '0x00000000',
    endAddress: is64Bit ? '0x7FFFFFFFFFFF' : '0x7FFFFFFF',
    alignment: 1,
    fastScan: true,
    enableSpeedHack: false
  })
  
  // Advanced options
  const [showAdvanced, setShowAdvanced] = React.useState(false)
  const [luaFormula, setLuaFormula] = React.useState('')
  const [notOperator, setNotOperator] = React.useState(false)
  const [unrandomizer, setUnrandomizer] = React.useState(false)
  
  const { sendCommand, isConnected, addAddress, readValue, modules, startMonitoring, stopMonitoring, writeMemory } = useEngineStore()
  const { openWindow } = useWindowManagerStore()
  
  // Virtualization constants - optimized for performance
  const itemHeight = 32 // Reduced height for more density
  const containerHeight = 600
  const bufferSize = 10 // Larger buffer for smoother scrolling
  
  // Virtualization calculations
  const totalItems = scanResults.length
  const visibleItems = Math.min(Math.ceil(containerHeight / itemHeight) + bufferSize, totalItems)
  
  // Performance monitoring for direct DLL communication
  const [performanceMetrics, setPerformanceMetrics] = React.useState({
    scanTime: 0,
    updateLatency: 0,
    monitoredCount: 0,
    communicationMode: 'real-time' // Real-time direct communication
  })
  
  // Real-time streaming state
  const [isStreaming, setIsStreaming] = React.useState(false)
  const [streamingEnabled, setStreamingEnabled] = React.useState(true)
  
  // Simple initialization
  React.useEffect(() => {
    console.log('🚀 Scanner initialized')
  }, [])
  
  // Virtualization state
  const [visibleRange, setVisibleRange] = React.useState({ start: 0, end: 500 })
  const [scrollTop, setScrollTop] = React.useState(0)
  
  // Column width state
  const [columnWidths, setColumnWidths] = React.useState({
    checkbox: 48,
    address: 200,
    module: 180,
    value: 150,
    previous: 120,
    type: 80
  })
  
  // Column resizing
  const [resizing, setResizing] = React.useState<string | null>(null)
  const [resizeStartX, setResizeStartX] = React.useState(0)
  const [resizeStartWidth, setResizeStartWidth] = React.useState(0)
  
  // Simplified, working scroll handler
  const handleScroll = React.useCallback((e: React.UIEvent<HTMLDivElement>) => {
    const scrollTop = e.currentTarget.scrollTop
    setScrollTop(scrollTop)
    
    const start = Math.floor(scrollTop / itemHeight)
    const end = Math.min(start + visibleItems, totalItems)
    
    if (start !== visibleRange.start || end !== visibleRange.end) {
      setVisibleRange({ start, end })
    }
  }, [itemHeight, visibleItems, totalItems, visibleRange.start, visibleRange.end])
  
  // Get visible scan results
  const visibleResults = React.useMemo(() => {
    const results = scanResults.slice(visibleRange.start, visibleRange.end)
    console.log(`Visible results: ${results.length} (from ${visibleRange.start} to ${visibleRange.end})`)
    return results
  }, [scanResults, visibleRange])
  
  // Debounced monitoring to reduce excessive updates
  const [monitoringAddresses, setMonitoringAddresses] = React.useState<Set<string>>(new Set())
  
  // Debounced visible range monitoring
  React.useEffect(() => {
    if (scanResults.length === 0) return
    
    const timer = setTimeout(() => {
      const currentAddresses = new Set(visibleResults.map(r => r.address))
      
      // Only update if addresses actually changed
      if (!areSetsEqual(monitoringAddresses, currentAddresses)) {
        // Stop monitoring old addresses
        monitoringAddresses.forEach(address => {
          if (!currentAddresses.has(address)) {
            stopMonitoring(address)
          }
        })
        
        // Start monitoring new addresses with simple callback
        visibleResults.forEach(result => {
          if (!monitoringAddresses.has(result.address)) {
            startMonitoring(result.address, valueType, (newValue: string) => {
              // Simple state update
              setScanResults(prev => {
                const index = prev.findIndex(r => r.address === result.address)
                if (index === -1 || prev[index].value === newValue) return prev
                
                const newResults = [...prev]
                newResults[index] = {
                  ...newResults[index],
                  value: newValue,
                  hasChanged: newValue !== newResults[index].originalValue
                }
                return newResults
              })
            })
          }
        })
        
        setMonitoringAddresses(currentAddresses)
        console.log(`👁️ Monitoring: ${currentAddresses.size} addresses`)
      }
    }, 200) // Normal debounce for stability
    
    return () => clearTimeout(timer)
  }, [visibleRange.start, visibleRange.end, scanResults.length]) // Only track essential changes
  
  // Helper function to compare sets
  const areSetsEqual = (set1: Set<string>, set2: Set<string>) => {
    if (set1.size !== set2.size) return false
    for (const item of set1) {
      if (!set2.has(item)) return false
    }
    return true
  }
  
  // Column resizing handlers
  const handleResizeStart = React.useCallback((column: string, e: React.MouseEvent) => {
    e.preventDefault()
    setResizing(column)
    setResizeStartX(e.clientX)
    setResizeStartWidth(columnWidths[column as keyof typeof columnWidths])
  }, [columnWidths])
  
  const handleResizeMove = React.useCallback((e: MouseEvent) => {
    if (!resizing) return
    
    const deltaX = e.clientX - resizeStartX
    const newWidth = Math.max(50, resizeStartWidth + deltaX) // Minimum 50px
    
    setColumnWidths(prev => ({
      ...prev,
      [resizing]: newWidth
    }))
  }, [resizing, resizeStartX, resizeStartWidth])
  
  const handleResizeEnd = React.useCallback(() => {
    setResizing(null)
  }, [])
  
  // Global mouse events for resizing
  React.useEffect(() => {
    if (resizing) {
      document.addEventListener('mousemove', handleResizeMove)
      document.addEventListener('mouseup', handleResizeEnd)
      return () => {
        document.removeEventListener('mousemove', handleResizeMove)
        document.removeEventListener('mouseup', handleResizeEnd)
      }
    }
  }, [resizing, handleResizeMove, handleResizeEnd])
  
  const performNextScan = async () => {
    if (!isConnected || scanResults.length === 0) return
    
    setIsScanning(true)
    
    try {
      // Prepare scan data for DLL
      const scanData = {
        command: 'memory.scan',
        scanType,
        valueType,
        value: scanValue,
        previousResults: scanResults, // Send current scan results as previous results
        firstScan: false,
        ...memoryOptions,
        luaFormula: luaFormula || undefined,
        notOperator,
        unrandomizer
      }
      
      const response = await sendCommand(scanData)
      
      if (response.success) {
        // Process results with module info (already included from DLL)
        const results = response.data.map((result: any) => {
          // Find if this address existed in previous scan results to preserve originalValue
          const existingResult = scanResults.find(r => r.address === result.address)
          return {
            address: result.address,
            value: result.value,
            previousValue: result.previousValue, // Previous value from scan history
            module: result.module || null,
            originalValue: existingResult?.originalValue || result.value, // Preserve original or use current
            hasChanged: existingResult ? (result.value !== existingResult.originalValue) : false
          }
        })
        
        setScanResults(results)
        setScanCount(scanCount + 1)
        setSelectedResults(new Set())
        
        // Note: Monitoring will be handled by the visible area effect
      } else {
        alert(`Next scan failed: ${response.error}`)
      }
    } catch (error) {
      console.error('Next scan error:', error)
      alert(`Next scan error: ${error}`)
    } finally {
      setIsScanning(false)
    }
  }
  
  const performScan = async () => {
    if (!isConnected) {
      alert('Not connected to target process')
      return
    }
    
    if (scanType === 'exact' && !scanValue.trim()) {
      alert('Please enter a value to scan for')
      return
    }
    
    setIsScanning(true)
    const scanStartTime = performance.now()
    
    try {
      const scanData = {
        command: 'memory.scan',
        scanType,
        valueType,
        value: scanValue,
        previousResults: scanCount > 0 ? scanResults : undefined,
        ...memoryOptions,
        firstScan: scanCount === 0,
        luaFormula: luaFormula || undefined,
        notOperator,
        unrandomizer
      }
      
      const response = await sendCommand(scanData)
      const scanTime = performance.now() - scanStartTime
      
      
      if (response.success) {
        // Process results with module info (already included from DLL)
        const results = response.data.map((result: any) => ({
          address: result.address,
          value: result.value,
          previousValue: scanCount > 0 ? result.previousValue : undefined,
          module: result.module || null,  // Module info now comes from DLL
          originalValue: result.value,    // Store original value for change detection
          hasChanged: false               // Initialize as unchanged
        }))
        
        setScanResults(results)
        setScanCount(scanCount + 1)
        setSelectedResults(new Set())
        
        console.log(`✅ Scan completed: ${results.length} results found`)
        console.log('First few results:', results.slice(0, 3))
        
        // === VIEWPORT-BASED REAL-TIME MONITORING ===
        // Only monitor currently visible addresses for optimal performance
        console.log(`🚀 Viewport-based monitoring enabled for ${results.length} results`)
        console.log('⚡ Only visible addresses are monitored in real-time!')
        
        
        setPerformanceMetrics({
          scanTime,
          updateLatency: 0,
          monitoredCount: visibleResults.length, // Only visible results
          communicationMode: 'real-time'
        })
        
        console.log(`✅ Scan complete - monitoring will follow viewport scrolling`)
        console.log('👁️ Performance: Only visible addresses monitored!')
        
      } else {
        alert(`Scan failed: ${response.error}`)
      }
    } catch (error) {
      console.error('Scan error:', error)
      alert(`Scan error: ${error}`)
    } finally {
      setIsScanning(false)
    }
  }
  
  // Get module info for an address using DLL command
  const getModuleInfo = async (address: string) => {
    try {
      const response = await sendCommand({ command: 'module.from_address', address })
      if (response.success && response.data?.displayName) {
        return response.data.displayName
      }
    } catch (error) {
      console.error('Failed to get module info:', error)
    }
    
    // Fallback to local calculation
    const addr = parseInt(address, 16)
    for (const module of modules) {
      const baseAddr = parseInt(module.baseAddress, 16)
      if (addr >= baseAddr && addr < baseAddr + module.size) {
        const offset = addr - baseAddr
        return `${module.name}+0x${offset.toString(16).toUpperCase()}`
      }
    }
    return null
  }
  
  const resetScan = () => {
    // Stop monitoring all scan results
    scanResults.forEach(result => {
      stopMonitoring(result.address)
    })
    
    setScanResults([])
    setSelectedResults(new Set())
    setScanCount(0)
    setScanValue('')
  }
  
  const addSelectedToAddressList = () => {
    selectedResults.forEach(address => {
      const result = scanResults.find(r => r.address === address)
      if (result) {
        addAddress({
          address: result.address,
          value: result.value,
          type: valueType,
          description: `Scanned ${valueType}`,
          frozen: false
        })
      }
    })
    
    setSelectedResults(new Set())
  }
  
  const toggleResultSelection = (address: string) => {
    const newSelected = new Set(selectedResults)
    if (newSelected.has(address)) {
      newSelected.delete(address)
    } else {
      newSelected.add(address)
    }
    setSelectedResults(newSelected)
  }
  
  const selectAllResults = () => {
    if (selectedResults.size === scanResults.length) {
      setSelectedResults(new Set())
    } else {
      setSelectedResults(new Set(scanResults.map(r => r.address)))
    }
  }
  
  const formatValue = (value: string, type: string) => {
    switch (type) {
      case 'int32':
      case 'int64':
        return parseInt(value).toLocaleString()
      case 'float':
      case 'double':
        return parseFloat(value).toFixed(6)
      case 'string':
        return `"${value}"`
      case 'bytes':
        return value.toUpperCase()
      default:
        return value
    }
  }
  
  // Handle value editing
  const handleValueEdit = async (address: string, newValue: string) => {
    // Validate input based on value type
    const trimmedValue = newValue.trim()
    if (!trimmedValue) {
      alert('Please enter a valid value')
      return
    }
    
    // Basic validation for numeric types
    if (['int32', 'int64'].includes(valueType)) {
      const num = parseInt(trimmedValue)
      if (isNaN(num)) {
        alert('Please enter a valid integer')
        return
      }
    } else if (['float', 'double'].includes(valueType)) {
      const num = parseFloat(trimmedValue)
      if (isNaN(num)) {
        alert('Please enter a valid number')
        return
      }
    }
    
    try {
      // Write the new value to memory
      const success = await writeMemory(address, trimmedValue, valueType)
      
      if (success) {
        // Update the UI state only if memory write was successful
        setScanResults(prev => prev.map(result => 
          result.address === address 
            ? { 
                ...result, 
                value: trimmedValue,
                hasChanged: trimmedValue !== result.originalValue 
              } 
            : result
        ))
        console.log(`✅ Successfully wrote value ${trimmedValue} to address ${address}`)
        
        // Show success feedback
        const successMsg = `✅ Value ${trimmedValue} written to ${address}`
        console.log(successMsg)
        
        // Restart monitoring for this address to reflect the updated value
        stopMonitoring(address)
        setTimeout(() => {
          startMonitoring(address, valueType, (newValue: string) => {
            setScanResults(prev => prev.map(r => 
              r.address === address 
                ? { 
                    ...r, 
                    value: newValue,
                    hasChanged: newValue !== r.originalValue 
                  } 
                : r
            ))
          })
        }, 100)
      } else {
        console.error(`❌ Failed to write value ${trimmedValue} to address ${address}`)
        alert(`Failed to write value to memory at ${address}. The address may not be writable or the process lacks permission.`)
      }
    } catch (error) {
      console.error(`❌ Error writing value to ${address}:`, error)
      alert(`Error writing value to memory: ${error}`)
    }
    
    setEditingResult(null)
  }
  
  // Build context menu items
  const getContextMenuItems = (result: ScanResult): ContextMenuItem[] => [
    {
      label: 'Add to address list',
      icon: FiPlus,
      onClick: () => {
        addAddress({
          address: result.address,
          value: result.value,
          type: valueType,
          description: `Scanned ${valueType}${result.module ? ` (${result.module})` : ''}`,
          frozen: false
        })
      }
    },
    { divider: true },
    {
      label: 'Change value',
      icon: FiEdit,
      onClick: () => {
        setEditingResult({ address: result.address, value: result.value })
      }
    },
    { divider: true },
    {
      label: 'Copy address',
      icon: FiCopy,
      onClick: () => navigator.clipboard.writeText(result.address)
    },
    {
      label: 'Copy value',
      icon: FiCopy,
      onClick: () => navigator.clipboard.writeText(result.value)
    },
    result.module ? {
      label: 'Copy module info',
      icon: FiCopy,
      onClick: () => navigator.clipboard.writeText(result.module!)
    } : null,
    { divider: true },
    {
      label: 'Browse this memory region',
      icon: FiCornerDownRight,
      onClick: () => openWindow('MEMORY_VIEWER')
    },
    { divider: true },
    {
      label: 'Remove from results',
      icon: FiTrash2,
      onClick: () => {
        setScanResults(prev => prev.filter(r => r.address !== result.address))
        setSelectedResults(prev => {
          const newSet = new Set(prev)
          newSet.delete(result.address)
          return newSet
        })
      },
      className: 'text-red-400'
    }
  ].filter(Boolean) as ContextMenuItem[]
  
  // Column resizer component
  const ColumnResizer = React.memo(({ column }: { column: string }) => (
    <div 
      className="absolute right-0 top-0 w-1 h-full cursor-col-resize hover:bg-blue-400 transition-colors"
      onMouseDown={(e) => handleResizeStart(column, e)}
      style={{ 
        backgroundColor: resizing === column ? '#60a5fa' : 'transparent',
        zIndex: 10
      }}
    />
  ))
  
  // Ultra-optimized row component with maximum performance
  const ScanResultRow = React.memo(({ 
    result, 
    index, 
    columnWidths, 
    selectedResults, 
    editingResult, 
    valueType, 
    scanCount,
    onToggleSelection, 
    onContextMenu, 
    onEditStart, 
    onEditChange, 
    onEditSubmit, 
    onEditCancel, 
    formatValue 
  }: any) => (
    <div 
      className={`flex text-sm border-b border-gray-800/50 transition-colors ${
        selectedResults.has(result.address) ? 'bg-blue-900/30' : 'hover:bg-gray-800'
      } ${result.hasChanged ? 'bg-red-900/10 border-red-600/20' : ''}`}
      style={{ height: 32 }}
      onContextMenu={(e) => onContextMenu(e, result)}
    >
      <div 
        className="px-4 py-2 flex items-center border-r border-gray-800/50"
        style={{ width: columnWidths.checkbox }}
      >
        <input
          type="checkbox"
          checked={selectedResults.has(result.address)}
          onChange={() => onToggleSelection(result.address)}
          className="rounded"
        />
      </div>
      <div 
        className="px-4 py-2 mono text-yellow-400 font-medium flex items-center border-r border-gray-800/50 overflow-hidden"
        style={{ width: columnWidths.address }}
        title={result.address}
      >
        <span className="truncate">{result.address}</span>
      </div>
      <div 
        className="px-4 py-2 text-blue-400 text-sm flex items-center border-r border-gray-800/50 overflow-hidden"
        style={{ width: columnWidths.module }}
        title={result.module || ''}
      >
        <span className="truncate">{result.module || ''}</span>
      </div>
      <div 
        className={`px-4 py-2 mono flex items-center border-r border-gray-800/50 overflow-hidden ${result.hasChanged ? 'text-red-400' : 'text-green-400'}`}
        style={{ width: columnWidths.value }}
      >
        {editingResult?.address === result.address ? (
          <input
            type="text"
            value={editingResult.value}
            onChange={(e) => onEditChange(e.target.value)}
            onKeyDown={(e) => {
              if (e.key === 'Enter') {
                onEditSubmit(result.address, editingResult.value)
              } else if (e.key === 'Escape') {
                onEditCancel()
              }
            }}
            onBlur={(e) => {
              if (e.target.value !== result.value) {
                onEditSubmit(result.address, e.target.value)
              } else {
                onEditCancel()
              }
            }}
            autoFocus
            className="bg-gray-900 text-white px-1 py-0.5 rounded w-full"
          />
        ) : (
          <span 
            onDoubleClick={() => onEditStart(result.address, result.value)}
            className="flex items-center gap-1 truncate"
            title={formatValue(result.value, valueType)}
          >
            <span className="truncate">{formatValue(result.value, valueType)}</span>
            {result.hasChanged && (
              <span className="text-red-400 text-xs flex-shrink-0" title="Value has changed from original scan">
                ⚠️
              </span>
            )}
          </span>
        )}
      </div>
      {scanCount > 1 && (
        <div 
          className="px-4 py-2 mono text-gray-400 flex items-center border-r border-gray-800/50 overflow-hidden"
          style={{ width: columnWidths.previous }}
          title={result.previousValue ? formatValue(result.previousValue, valueType) : '-'}
        >
          <span className="truncate">
            {result.previousValue ? formatValue(result.previousValue, valueType) : '-'}
          </span>
        </div>
      )}
      <div 
        className="px-4 py-2 text-gray-300 flex items-center overflow-hidden"
        style={{ width: columnWidths.type }}
        title={valueType}
      >
        <span className="truncate">{valueType}</span>
      </div>
    </div>
  ))
  
  return (
    <div className="h-full flex bg-gray-900 text-white">
      {/* Left Panel - Scan Configuration */}
      <div className="w-80 bg-gray-800 border-r border-gray-700 flex flex-col">
        {/* Header */}
        <div className="p-4 border-b border-gray-700">
          <div className="flex items-center gap-3 mb-4">
            <FiSearch className="text-green-400" size={20} />
            <h2 className="text-lg font-semibold">Memory Scanner</h2>
          </div>
          
          <div className="text-sm text-gray-400 space-y-1">
            <div>
              Found: <span className="text-white font-medium">{scanResults.length.toLocaleString()}</span> addresses
              {scanCount > 0 && <span className="ml-2">({scanCount} scans)</span>}
            </div>
            
            {/* Performance metrics */}
            <div className="flex items-center gap-4 text-xs">
              <span>
                Monitored: <span className="text-blue-400">{visibleResults.length}</span>
                <span className="text-gray-500 ml-1">(viewport)</span>
              </span>
              
              {performanceMetrics.scanTime > 0 && (
                <span>
                  Last scan: <span className="text-yellow-400">{Math.round(performanceMetrics.scanTime)}ms</span>
                </span>
              )}
              
              {isStreaming && (
                <span className="text-green-400 flex items-center gap-1">
                  <div className="w-2 h-2 bg-green-400 rounded-full animate-pulse" />
                  Streaming
                </span>
              )}
            </div>
          </div>
        </div>
        
        {/* Scan Configuration */}
        <div className="flex-1 overflow-auto p-4 space-y-4">
          {/* Value Input */}
          <div className="space-y-2">
            <label className="text-sm font-medium text-gray-300">Value</label>
            <div className="relative">
              <input
                type="text"
                value={scanValue}
                onChange={(e) => setScanValue(e.target.value)}
                className="w-full bg-gray-700 border border-gray-600 rounded-lg px-3 py-2 text-white focus:border-green-400 focus:outline-none"
                placeholder={scanType === 'exact' ? 'Enter value to scan' : 'Not needed for this scan type'}
                disabled={['unknown', 'increased', 'decreased', 'changed', 'unchanged'].includes(scanType)}
              />
              {luaFormula && (
                <div className="mt-1 text-xs text-blue-400">
                  Lua formula enabled
                </div>
              )}
            </div>
          </div>
          
          {/* Scan Type */}
          <div className="space-y-2">
            <label className="text-sm font-medium text-gray-300">Scan Type</label>
            <select
              value={scanType}
              onChange={(e) => setScanType(e.target.value as any)}
              className="w-full bg-gray-700 border border-gray-600 rounded-lg px-3 py-2 text-white focus:border-green-400 focus:outline-none"
            >
              <option value="exact">Exact Value</option>
              <option value="fuzzy">Fuzzy Value</option>
              <option value="unknown">Unknown Initial Value</option>
              <option value="increased">Increased Value</option>
              <option value="decreased">Decreased Value</option>
              <option value="changed">Changed Value</option>
              <option value="unchanged">Unchanged Value</option>
            </select>
          </div>
          
          {/* Value Type */}
          <div className="space-y-2">
            <label className="text-sm font-medium text-gray-300">Value Type</label>
            <select
              value={valueType}
              onChange={(e) => setValueType(e.target.value as any)}
              className="w-full bg-gray-700 border border-gray-600 rounded-lg px-3 py-2 text-white focus:border-green-400 focus:outline-none"
            >
              <option value="int32">4 Bytes (Int32)</option>
              <option value="int64">8 Bytes (Int64)</option>
              <option value="float">Float</option>
              <option value="double">Double</option>
              <option value="string">String</option>
              <option value="bytes">Array of Bytes</option>
            </select>
          </div>
          
          {/* Memory Scan Options */}
          <div className="space-y-3">
            <div className="flex items-center justify-between">
              <span className="text-sm font-medium text-gray-300">Memory Scan Options</span>
              <button
                onClick={() => setShowAdvanced(!showAdvanced)}
                className="text-xs text-blue-400 hover:text-blue-300"
              >
                {showAdvanced ? 'Hide' : 'Show'} Advanced
              </button>
            </div>
            
            <div className="space-y-2">
              <div className="flex items-center justify-between">
                <label className="text-sm text-gray-300">Writable</label>
                <select 
                  value={memoryOptions.writable}
                  onChange={e => setMemoryOptions({...memoryOptions, writable: e.target.value as any})}
                  className="bg-gray-700 border border-gray-600 rounded px-2 py-1 text-xs"
                >
                  <option value="yes">Yes</option>
                  <option value="no">No</option>
                  <option value="any">Don't Care</option>
                </select>
              </div>
              <div className="flex items-center justify-between">
                <label className="text-sm text-gray-300">Executable</label>
                <select 
                  value={memoryOptions.executable}
                  onChange={e => setMemoryOptions({...memoryOptions, executable: e.target.value as any})}
                  className="bg-gray-700 border border-gray-600 rounded px-2 py-1 text-xs"
                >
                  <option value="yes">Yes</option>
                  <option value="no">No</option>
                  <option value="any">Don't Care</option>
                </select>
              </div>
              <div className="flex items-center justify-between">
                <label className="text-sm text-gray-300">Copy On Write</label>
                <select 
                  value={memoryOptions.copyOnWrite}
                  onChange={e => setMemoryOptions({...memoryOptions, copyOnWrite: e.target.value as any})}
                  className="bg-gray-700 border border-gray-600 rounded px-2 py-1 text-xs"
                >
                  <option value="yes">Yes</option>
                  <option value="no">No</option>
                  <option value="any">Don't Care</option>
                </select>
              </div>
            </div>
            
            {showAdvanced && (
              <div className="space-y-3 pt-2 border-t border-gray-700">
                <div className="grid grid-cols-2 gap-2">
                  <div>
                    <label className="text-xs text-gray-400">Start</label>
                    <input
                      type="text"
                      value={memoryOptions.startAddress}
                      onChange={(e) => setMemoryOptions({...memoryOptions, startAddress: e.target.value})}
                      className="w-full bg-gray-700 border border-gray-600 rounded px-2 py-1 text-xs mono"
                    />
                  </div>
                  <div>
                    <label className="text-xs text-gray-400">Stop</label>
                    <input
                      type="text"
                      value={memoryOptions.endAddress}
                      onChange={(e) => setMemoryOptions({...memoryOptions, endAddress: e.target.value})}
                      className="w-full bg-gray-700 border border-gray-600 rounded px-2 py-1 text-xs mono"
                    />
                  </div>
                </div>
                
                <label className="flex items-center gap-2">
                  <input
                    type="checkbox"
                    checked={memoryOptions.fastScan}
                    onChange={(e) => setMemoryOptions({...memoryOptions, fastScan: e.target.checked})}
                    className="rounded"
                  />
                  <span className="text-xs text-gray-300">Fast Scan</span>
                  <span className="text-xs text-gray-500">Alignment: {memoryOptions.fastScan ? '4' : '1'}</span>
                </label>
                
                <label className="flex items-center gap-2">
                  <input
                    type="checkbox"
                    checked={notOperator}
                    onChange={(e) => setNotOperator(e.target.checked)}
                    className="rounded"
                  />
                  <span className="text-xs text-gray-300">Not</span>
                </label>
                
                <label className="flex items-center gap-2">
                  <input
                    type="checkbox"
                    checked={unrandomizer}
                    onChange={(e) => setUnrandomizer(e.target.checked)}
                    className="rounded"
                  />
                  <span className="text-xs text-gray-300">Unrandomizer</span>
                </label>
                
                <div>
                  <label className="text-xs text-gray-400">Lua Formula</label>
                  <textarea
                    value={luaFormula}
                    onChange={(e) => setLuaFormula(e.target.value)}
                    className="w-full bg-gray-700 border border-gray-600 rounded px-2 py-1 text-xs mono h-16 resize-none"
                    placeholder="return tonumber(value) > 100"
                  />
                </div>
              </div>
            )}
          </div>
        </div>
        
        {/* Scan Buttons */}
        <div className="p-4 border-t border-gray-700 space-y-2">
          <button
            onClick={scanCount === 0 ? performScan : performNextScan}
            disabled={isScanning || !isConnected}
            className="w-full flex items-center justify-center gap-2 py-3 bg-green-600 hover:bg-green-700 disabled:bg-gray-600 rounded-lg font-medium transition-colors"
          >
            {isScanning ? (
              <>
                <FiRefreshCw className="animate-spin" size={16} />
                Scanning...
              </>
            ) : (
              <>
                <FiSearch size={16} />
                {scanCount === 0 ? 'First Scan' : 'Next Scan'}
              </>
            )}
          </button>
          
          {scanCount > 0 && (
            <button
              onClick={resetScan}
              className="w-full flex items-center justify-center gap-2 py-2 bg-gray-700 hover:bg-gray-600 rounded-lg transition-colors"
            >
              <FiRefreshCw size={16} />
              New Scan
            </button>
          )}
          
          {selectedResults.size > 0 && (
            <button
              onClick={addSelectedToAddressList}
              className="w-full flex items-center justify-center gap-2 py-2 bg-blue-600 hover:bg-blue-700 rounded-lg transition-colors"
            >
              <FiPlus size={16} />
              Add Selected ({selectedResults.size})
            </button>
          )}
        </div>
      </div>
      
      {/* Right Panel - Scan Results */}
      <div className="flex-1 flex flex-col">
        {/* Results Header */}
        <div className="bg-gray-800 border-b border-gray-700 p-4">
          <div className="flex items-center justify-between">
            <div className="flex items-center gap-4">
              <h3 className="font-medium">Scan Results</h3>
              <div className="text-sm text-gray-400">
                {scanResults.length > 0 && (
                  <>
                    {selectedResults.size > 0 && (
                      <span className="text-blue-400">{selectedResults.size} selected • </span>
                    )}
                    {scanResults.length.toLocaleString()} addresses found
                  </>
                )}
              </div>
            </div>
            
            {scanResults.length > 0 && (
              <div className="flex items-center gap-2">
                <button
                  onClick={selectAllResults}
                  className="px-3 py-1 bg-gray-700 hover:bg-gray-600 rounded text-sm transition-colors"
                >
                  {selectedResults.size === scanResults.length ? 'Deselect All' : 'Select All'}
                </button>
                <button
                  onClick={() => setScanResults([])}
                  className="px-3 py-1 bg-red-600 hover:bg-red-700 rounded text-sm transition-colors"
                >
                  Clear Results
                </button>
              </div>
            )}
          </div>
        </div>
        
        {/* Virtualized Results Table */}
        <div className="flex-1 overflow-hidden">
          {scanResults.length === 0 ? (
            <div className="flex items-center justify-center h-full">
              <div className="text-center">
                <FiTarget className="mx-auto text-gray-600 mb-4" size={48} />
                <div className="text-gray-400 text-lg mb-2">
                  {scanCount === 0 ? 'Ready to scan memory' : 'No results found'}
                </div>
                <div className="text-gray-500 text-sm">
                  {scanCount === 0 
                    ? 'Configure your scan parameters and click "First Scan"'
                    : 'Try adjusting your scan parameters or performing a new scan'
                  }
                </div>
              </div>
            </div>
          ) : (
            <div className="h-full flex flex-col">
              {/* Fixed Header with Resizable Columns */}
              <div className="bg-gray-800 border-b border-gray-700">
                <div className="flex text-sm font-semibold">
                  <div 
                    className="px-4 py-3 relative border-r border-gray-600"
                    style={{ width: columnWidths.checkbox }}
                  >
                    <input
                      type="checkbox"
                      checked={selectedResults.size === scanResults.length && scanResults.length > 0}
                      onChange={selectAllResults}
                      className="rounded"
                    />
                    <ColumnResizer column="checkbox" />
                  </div>
                  <div 
                    className="px-4 py-3 relative border-r border-gray-600"
                    style={{ width: columnWidths.address }}
                  >
                    Address
                    <ColumnResizer column="address" />
                  </div>
                  <div 
                    className="px-4 py-3 relative border-r border-gray-600"
                    style={{ width: columnWidths.module }}
                  >
                    Module
                    <ColumnResizer column="module" />
                  </div>
                  <div 
                    className="px-4 py-3 relative border-r border-gray-600"
                    style={{ width: columnWidths.value }}
                  >
                    Value
                    <ColumnResizer column="value" />
                  </div>
                  {scanCount > 1 && (
                    <div 
                      className="px-4 py-3 relative border-r border-gray-600"
                      style={{ width: columnWidths.previous }}
                    >
                      Previous
                      <ColumnResizer column="previous" />
                    </div>
                  )}
                  <div 
                    className="px-4 py-3 relative"
                    style={{ width: columnWidths.type }}
                  >
                    Type
                    <ColumnResizer column="type" />
                  </div>
                </div>
              </div>
              
              {/* Simple virtualized content */}
              <div 
                className="flex-1 overflow-auto"
                style={{ height: containerHeight }}
                onScroll={handleScroll}
              >
                <div style={{ height: totalItems * itemHeight, position: 'relative' }}>
                  <div 
                    style={{
                      position: 'absolute',
                      top: visibleRange.start * itemHeight,
                      left: 0,
                      right: 0
                    }}
                  >
                    {visibleResults.map((result, index) => (
                      <ScanResultRow
                        key={result.address}
                        result={result}
                        index={index}
                        columnWidths={columnWidths}
                        selectedResults={selectedResults}
                        editingResult={editingResult}
                        valueType={valueType}
                        scanCount={scanCount}
                        onToggleSelection={toggleResultSelection}
                        onContextMenu={handleContextMenu}
                        onEditStart={(address: string, value: string) => setEditingResult({ address, value })}
                        onEditChange={(value: string) => setEditingResult(prev => prev ? { ...prev, value } : null)}
                        onEditSubmit={handleValueEdit}
                        onEditCancel={() => setEditingResult(null)}
                        formatValue={formatValue}
                      />
                    ))}
                  </div>
                </div>
              </div>
              
              {/* Performance Info */}
              {scanResults.length > 500 && (
                <div className="bg-gray-800 border-t border-gray-700 p-2 text-xs text-gray-400 text-center">
                  Showing {visibleRange.start + 1}-{Math.min(visibleRange.end, totalItems)} of {totalItems.toLocaleString()} results 
                  • Virtualized for performance • Monitoring {visibleResults.length} visible addresses
                  {isStreaming && <span className="text-green-400 ml-2">🔴 Live</span>}
                </div>
              )}
            </div>
          )}
        </div>
      </div>
      
      {/* Context Menu */}
      {contextMenu && contextMenu.data && (
        <ContextMenu
          x={contextMenu.x}
          y={contextMenu.y}
          items={getContextMenuItems(contextMenu.data)}
          onClose={closeContextMenu}
        />
      )}
    </div>
  )
}