import React, { useState } from 'react'
import { FiCopy, FiRefreshCw, FiEye, FiCode, FiSearch, FiNavigation, FiMessageSquare, FiCpu, FiCornerDownRight } from 'react-icons/fi'
import { useEngineStore } from '../store/engineStore'
import ContextMenu, { useContextMenu, ContextMenuItem } from './ContextMenu'

interface DisassemblyInstruction {
  address: string
  bytes: string
  mnemonic: string
  operands: string
  comment?: string
  length?: number
  isJump?: boolean
  isCall?: boolean
  isRet?: boolean
  target?: string
}

export default function MemoryViewer() {
  const [address, setAddress] = React.useState('0x00400000')
  const processInfo = useEngineStore(state => state.processInfo)
  
  // 64비트 프로세스인지 확인
  const is64Bit = processInfo?.addressWidth === 64
  const [bytesPerRow, setBytesPerRow] = React.useState(16)
  const [rows, setRows] = React.useState(32)
  const [memoryData, setMemoryData] = React.useState<number[][]>([])
  const [loading, setLoading] = React.useState(false)
  const [error, setError] = React.useState<string | null>(null)
  const [addressInfo, setAddressInfo] = React.useState<any>(null)
  const [viewMode, setViewMode] = React.useState<'hex' | 'ascii' | 'both'>('both')
  const [dataType, setDataType] = React.useState<'bytes' | 'int' | 'float' | 'double'>('bytes')
  const [selectedByte, setSelectedByte] = React.useState<{row: number, col: number} | null>(null)
  const { writeMemory, disassembleMemory, disassembly } = useEngineStore()
  const [editingCell, setEditingCell] = useState<{row: number, col: number} | null>(null)
  const [editingValue, setEditingValue] = useState('')
  
  // Tabs and disassembly
  const [activeTab, setActiveTab] = React.useState<'memory' | 'disassembly'>('memory')
  const [disassemblyData, setDisassemblyData] = React.useState<DisassemblyInstruction[]>([])
  const [comments, setComments] = React.useState<Record<string, string>>({})
  const [editingComment, setEditingComment] = React.useState<string | null>(null)
  
  // Context menu
  const { contextMenu, handleContextMenu, closeContextMenu } = useContextMenu()
  
  const readMemory = useEngineStore(state => state.readMemory)
  const validateAddress = useEngineStore(state => state.validateAddress)
  const isConnected = useEngineStore(state => state.isConnected)
  
  const loadMemory = async () => {
    if (!isConnected) {
      setError('Not connected to engine')
      return
    }
    
    setLoading(true)
    try {
      if (activeTab === 'memory') {
        await loadMemoryData()
      } else if (activeTab === 'disassembly') {
        await loadDisassembly()
      }
    } finally {
      setLoading(false)
    }
  }
  
  const loadMemoryData = async () => {
    setError(null)
    
    try {
      const totalBytes = bytesPerRow * rows
      const validation = await validateAddress(address, totalBytes)
      
      if (!validation.valid) {
        setError(`Invalid memory address: ${address}`)
        setAddressInfo(null)
        setMemoryData([])
        return
      }
      
      if (!validation.readable) {
        setError(`Memory address is not readable: ${address}`)
        setAddressInfo(validation)
        setMemoryData([])
        return
      }
      
      setAddressInfo(validation)
      
      const data = await readMemory(address, totalBytes, dataType)
      
      if (Array.isArray(data) && data.length > 0) {
        const rowData: number[][] = []
        for (let i = 0; i < rows; i++) {
          const startIdx = i * bytesPerRow
          const endIdx = Math.min(startIdx + bytesPerRow, data.length)
          const rowBytes = data.slice(startIdx, endIdx)
          
          while (rowBytes.length < bytesPerRow) {
            rowBytes.push(0)
          }
          
          rowData.push(rowBytes)
        }
        setMemoryData(rowData)
        setError(null)
      } else {
        setError('Failed to read memory data')
        setMemoryData([])
      }
    } catch (error) {
      console.error('Failed to read memory:', error)
      setError(`Memory read error: ${error}`)
      setMemoryData([])
      setAddressInfo(null)
    } finally {
      // Loading state managed by parent function
    }
  }
  
  const loadDisassembly = async () => {
    if (!isConnected) return
    
    try {
      const instructionSize = 256 // Get about 256 bytes worth of instructions
      await disassembleMemory(address, instructionSize)
      
      // Parse the actual disassembly response from DLL
      if (disassembly) {
        try {
          const parsedInstructions = JSON.parse(disassembly) as any[]
          const instructions: DisassemblyInstruction[] = parsedInstructions.map(inst => ({
            address: inst.address,
            bytes: inst.bytes,
            mnemonic: inst.mnemonic,
            operands: inst.operands,
            comment: comments[inst.address] || '',
            length: inst.length || 1,
            isJump: inst.isJump || false,
            isCall: inst.isCall || false,
            isRet: inst.isRet || false,
            target: inst.target
          }))
          
          setDisassemblyData(instructions)
        } catch (parseError) {
          console.error('Failed to parse disassembly:', parseError)
          setDisassemblyData([])
        }
      }
    } catch (error) {
      console.error('Failed to disassemble:', error)
    } finally {
      // Loading state managed by parent function
    }
  }
  
  const validateAndLoadMemory = async () => {
    try {
      const normalizedAddress = address.startsWith('0x') ? address : '0x' + address
      const parsedAddress = parseInt(normalizedAddress, 16)
      
      if (isNaN(parsedAddress) || parsedAddress < 0) {
        setError('Invalid address format. Use hex format like 0x00400000')
        return
      }
      
      setAddress(normalizedAddress)
      await loadMemory()
    } catch (error) {
      setError('Invalid address format')
    }
  }
  
  const handleAddressChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    setAddress(e.target.value)
    setError(null)
  }
  
  const handleKeyPress = (e: React.KeyboardEvent) => {
    if (e.key === 'Enter') {
      validateAndLoadMemory()
    }
  }
  
  const navigateToAddress = (newAddress: number) => {
    const formattedAddress = '0x' + newAddress.toString(16).toUpperCase().padStart(is64Bit ? 16 : 8, '0')
    setAddress(formattedAddress)
    loadMemory()
  }
  
  const navigateByPages = (pageDirection: number) => {
    const currentAddr = parseInt(address, 16)
    const totalBytesPerPage = bytesPerRow * rows
    const newAddr = currentAddr + (pageDirection * totalBytesPerPage)
    
    // Ensure address alignment and bounds checking
    const alignedAddr = Math.max(0, Math.floor(newAddr / bytesPerRow) * bytesPerRow)
    navigateToAddress(alignedAddr)
  }
  
  const navigateByRows = (rowDirection: number) => {
    const currentAddr = parseInt(address, 16)
    const newAddr = currentAddr + (rowDirection * bytesPerRow)
    const alignedAddr = Math.max(0, Math.floor(newAddr / bytesPerRow) * bytesPerRow)
    navigateToAddress(alignedAddr)
  }
  
  const copyToClipboard = async (text: string) => {
    try {
      await navigator.clipboard.writeText(text)
      // Could add a toast notification here
    } catch (error) {
      console.error('Failed to copy to clipboard:', error)
    }
  }
  
  const handleByteClick = (rowIndex: number, colIndex: number, byte: number) => {
    setSelectedByte({row: rowIndex, col: colIndex})
    const byteAddress = parseInt(address, 16) + rowIndex * bytesPerRow + colIndex
    copyToClipboard(`0x${byteAddress.toString(16).toUpperCase()}: 0x${byte.toString(16).toUpperCase().padStart(2, '0')}`)
  }
  
  const handleCellDoubleClick = (row: number, col: number, byte: number) => {
    setEditingCell({row, col})
    setEditingValue(byte.toString(16).toUpperCase().padStart(2, '0'))
  }

  const handleCellValueChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    setEditingValue(e.target.value)
  }

  const handleCellValueUpdate = async (row: number, col: number) => {
    const addressToUpdate = parseInt(address, 16) + row * bytesPerRow + col
    const hexValue = editingValue.replace(/[^0-9a-fA-F]/g, '')
    if (hexValue.length > 0 && hexValue.length <= 2) {
      await writeMemory(`0x${addressToUpdate.toString(16)}`, hexValue, 'bytes')
      // Optionally, re-read memory to confirm the change
      loadMemory()
    }
    setEditingCell(null)
  }
  
  const addComment = (instructionAddress: string, comment: string) => {
    setComments(prev => ({ ...prev, [instructionAddress]: comment }))
    setDisassemblyData(prev => prev.map(inst => 
      inst.address === instructionAddress ? { ...inst, comment } : inst
    ))
  }
  
  const getContextMenuItems = (data: any): ContextMenuItem[] => {
    if (activeTab === 'memory') {
      const { address: byteAddr, value } = data
      return [
        {
          label: 'Copy address',
          icon: FiCopy,
          onClick: () => navigator.clipboard.writeText(byteAddr)
        },
        {
          label: 'Copy value',
          icon: FiCopy,
          onClick: () => navigator.clipboard.writeText(value)
        },
        { divider: true },
        {
          label: 'Go to address',
          icon: FiNavigation,
          onClick: () => {
            setAddress(byteAddr)
            loadMemory()
          }
        },
        {
          label: 'Disassemble here',
          icon: FiCode,
          onClick: () => {
            setAddress(byteAddr)
            setActiveTab('disassembly')
            loadMemory()
          }
        }
      ]
    } else {
      const instruction = data as DisassemblyInstruction
      return [
        {
          label: 'Copy address',
          icon: FiCopy,
          onClick: () => navigator.clipboard.writeText('0x' + instruction.address)
        },
        {
          label: 'Copy instruction',
          icon: FiCopy,
          onClick: () => navigator.clipboard.writeText(`${instruction.mnemonic} ${instruction.operands}`)
        },
        {
          label: 'Copy bytes',
          icon: FiCopy,
          onClick: () => navigator.clipboard.writeText(instruction.bytes)
        },
        { divider: true },
        {
          label: 'Add/Edit comment',
          icon: FiMessageSquare,
          onClick: () => setEditingComment(instruction.address)
        },
        { divider: true },
        {
          label: 'Go to address',
          icon: FiNavigation,
          onClick: () => {
            setAddress('0x' + instruction.address)
            loadMemory()
          }
        },
        instruction.target ? {
          label: 'Follow jump/call',
          icon: FiCornerDownRight,
          onClick: () => {
            const targetAddr = '0x' + parseInt(instruction.target!, 16).toString(16).toUpperCase().padStart(is64Bit ? 16 : 8, '0')
            setAddress(targetAddr)
            loadMemory()
          }
        } : null,
        { divider: true },
        {
          label: 'Set breakpoint',
          icon: FiCpu,
          onClick: () => {
            // TODO: Implement breakpoint setting
            console.log('Setting breakpoint at', instruction.address)
          }
        },
        {
          label: 'Patch instruction',
          icon: FiCode,
          onClick: () => {
            // TODO: Implement instruction patching
            console.log('Patching instruction at', instruction.address)
          }
        }
      ].filter(Boolean)
    }
  }
  
  React.useEffect(() => {
    if (isConnected && !error && !loading) {
      loadMemory()
    }
  }, [isConnected, bytesPerRow, rows, dataType, activeTab])
  
  const renderMemoryValue = (rowData: number[], format: 'hex' | 'ascii' | 'int' | 'float') => {
    switch (format) {
      case 'hex':
        return rowData.map(byte => byte.toString(16).toUpperCase().padStart(2, '0')).join(' ')
      case 'ascii':
        return rowData.map(byte => {
          const char = String.fromCharCode(byte)
          return byte >= 32 && byte <= 126 ? char : '.'
        }).join('')
      case 'int':
        if (rowData.length >= 4) {
          const view = new DataView(new Uint8Array(rowData.slice(0, 4)).buffer)
          return view.getInt32(0, true).toString()
        }
        return 'N/A'
      case 'float':
        if (rowData.length >= 4) {
          const view = new DataView(new Uint8Array(rowData.slice(0, 4)).buffer)
          return view.getFloat32(0, true).toFixed(6)
        }
        return 'N/A'
      default:
        return ''
    }
  }
  
  return (
    <div className="h-full flex flex-col bg-gray-900 text-white">
      {/* Modern Header */}
      <div className="bg-gray-800 border-b border-gray-700 p-4">
        <div className="flex items-center justify-between mb-4">
          <div className="flex items-center gap-3">
            <FiEye className="text-blue-400" size={20} />
            <h2 className="text-lg font-semibold">Memory Viewer</h2>
          </div>
          <div className="flex items-center gap-2">
            <button
              onClick={validateAndLoadMemory}
              disabled={loading || !isConnected}
              className="flex items-center gap-2 px-4 py-2 bg-blue-600 hover:bg-blue-700 disabled:bg-gray-600 rounded-lg transition-colors"
            >
              <FiRefreshCw className={loading ? 'animate-spin' : ''} size={16} />
              {loading ? 'Loading...' : 'Refresh'}
            </button>
          </div>
        </div>
        
        {/* Control Grid */}
        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-4">
          {/* Address Input */}
          <div className="space-y-2">
            <label className="text-sm font-medium text-gray-300 flex items-center gap-2">
              <FiNavigation size={14} />
              Memory Address
            </label>
            <div className="flex gap-2">
              <input
                type="text"
                value={address}
                onChange={handleAddressChange}
                onKeyPress={handleKeyPress}
                className="flex-1 mono bg-gray-700 border border-gray-600 rounded-lg px-3 py-2 text-white focus:border-blue-400 focus:outline-none"
                placeholder="0x00400000"
              />
              <button
                onClick={() => copyToClipboard(address)}
                className="p-2 bg-gray-700 hover:bg-gray-600 border border-gray-600 rounded-lg transition-colors"
                title="Copy address"
              >
                <FiCopy size={16} />
              </button>
            </div>
          </div>
          
          {/* Bytes per Row */}
          <div className="space-y-2">
            <label className="text-sm font-medium text-gray-300">Bytes per Row</label>
            <select
              value={bytesPerRow}
              onChange={e => setBytesPerRow(Number(e.target.value))}
              className="w-full bg-gray-700 border border-gray-600 rounded-lg px-3 py-2 text-white focus:border-blue-400 focus:outline-none"
            >
              <option value={8}>8 bytes</option>
              <option value={16}>16 bytes</option>
              <option value={32}>32 bytes</option>
              <option value={64}>64 bytes</option>
            </select>
          </div>
          
          {/* Rows */}
          <div className="space-y-2">
            <label className="text-sm font-medium text-gray-300">Rows</label>
            <input
              type="number"
              value={rows}
              onChange={e => setRows(Math.max(1, Math.min(100, Number(e.target.value))))}
              className="w-full bg-gray-700 border border-gray-600 rounded-lg px-3 py-2 text-white focus:border-blue-400 focus:outline-none"
              min={1}
              max={100}
            />
          </div>
          
          {/* Data Type */}
          <div className="space-y-2">
            <label className="text-sm font-medium text-gray-300">Data Type</label>
            <select
              value={dataType}
              onChange={e => setDataType(e.target.value as any)}
              className="w-full bg-gray-700 border border-gray-600 rounded-lg px-3 py-2 text-white focus:border-blue-400 focus:outline-none"
            >
              <option value="bytes">Raw Bytes</option>
              <option value="int">32-bit Integer</option>
              <option value="float">Float</option>
              <option value="double">Double</option>
            </select>
          </div>
        </div>
        
        {/* Tab Navigation */}
        <div className="mt-4 flex items-center gap-4">
          <div className="flex bg-gray-700 rounded-lg p-1">
            {[{ id: 'memory', label: 'Memory View', icon: FiEye }, { id: 'disassembly', label: 'Disassembly', icon: FiCode }].map(tab => {
              const Icon = tab.icon
              return (
                <button
                  key={tab.id}
                  onClick={() => setActiveTab(tab.id as any)}
                  className={`px-4 py-2 rounded-md text-sm font-medium transition-all flex items-center gap-2 ${
                    activeTab === tab.id 
                      ? 'bg-blue-600 text-white shadow-md' 
                      : 'text-gray-300 hover:text-white hover:bg-gray-600'
                  }`}
                >
                  <Icon size={16} />
                  {tab.label}
                </button>
              )
            })}
          </div>
          
          {activeTab === 'memory' && (
            <>
              <span className="text-sm font-medium text-gray-300">View Mode:</span>
              <div className="flex bg-gray-700 rounded-lg p-1">
                {(['hex', 'ascii', 'both'] as const).map(mode => (
                  <button
                    key={mode}
                    onClick={() => setViewMode(mode)}
                    className={`px-4 py-2 rounded-md text-sm font-medium transition-all ${
                      viewMode === mode 
                        ? 'bg-blue-600 text-white shadow-md' 
                        : 'text-gray-300 hover:text-white hover:bg-gray-600'
                    }`}
                  >
                    {mode.toUpperCase()}
                  </button>
                ))}
              </div>
            </>
          )}
        </div>
      </div>
      
      {/* Address Information Card */}
      {addressInfo && (
        <div className="bg-gray-800 border-b border-gray-700 p-4">
          <div className="grid grid-cols-2 md:grid-cols-4 gap-4 text-sm">
            <div className="flex items-center gap-2">
              <span className="text-gray-400">Valid:</span>
              <span className={`flex items-center gap-1 ${addressInfo.valid ? 'text-green-400' : 'text-red-400'}`}>
                {addressInfo.valid ? '✓' : '✗'} {addressInfo.valid ? 'Yes' : 'No'}
              </span>
            </div>
            <div className="flex items-center gap-2">
              <span className="text-gray-400">Readable:</span>
              <span className={`flex items-center gap-1 ${addressInfo.readable ? 'text-green-400' : 'text-red-400'}`}>
                {addressInfo.readable ? '✓' : '✗'} {addressInfo.readable ? 'Yes' : 'No'}
              </span>
            </div>
            <div className="flex items-center gap-2">
              <span className="text-gray-400">Writable:</span>
              <span className={`flex items-center gap-1 ${addressInfo.writable ? 'text-green-400' : 'text-red-400'}`}>
                {addressInfo.writable ? '✓' : '✗'} {addressInfo.writable ? 'Yes' : 'No'}
              </span>
            </div>
            {addressInfo.region && (
              <div className="flex items-center gap-2">
                <span className="text-gray-400">Module:</span>
                <span className="text-blue-400 font-medium">
                  {addressInfo.region.moduleName || 'Unknown'}
                </span>
              </div>
            )}
          </div>
        </div>
      )}
      
      {/* Error Display */}
      {error && (
        <div className="bg-red-900/30 border border-red-600/30 p-4 m-4 rounded-lg">
          <div className="flex items-center gap-2">
            <span className="text-red-400">⚠️</span>
            <span className="text-red-300">{error}</span>
          </div>
        </div>
      )}
      
      {/* Memory Display */}
      <div className="flex-1 overflow-auto bg-gray-900">
        {loading ? (
          <div className="flex items-center justify-center h-full">
            <div className="flex flex-col items-center gap-4">
              <FiRefreshCw className="animate-spin text-blue-400" size={32} />
              <span className="text-gray-400">Loading memory data...</span>
            </div>
          </div>
        ) : memoryData.length > 0 ? (
          <div className="memory-grid">
            <table className="w-full text-sm mono">
              <thead>
                <tr className="bg-gray-800 border-b border-gray-700 sticky top-0">
                  <th className="memory-address text-left px-4 py-3 font-semibold">
                    Address
                  </th>
                  {viewMode !== 'ascii' && Array.from({ length: bytesPerRow }, (_, i) => (
                    <th key={i} className="text-center px-2 py-3 text-gray-400 font-medium">
                      {i.toString(16).toUpperCase().padStart(2, '0')}
                    </th>
                  ))}
                  {(viewMode === 'both' || viewMode === 'ascii') && (
                    <th className="text-left px-4 py-3 font-semibold text-gray-300">
                      ASCII
                    </th>
                  )}
                  {dataType !== 'bytes' && (
                    <th className="text-left px-4 py-3 font-semibold text-gray-300">
                      {dataType.toUpperCase()}
                    </th>
                  )}
                </tr>
              </thead>
              <tbody>
                {memoryData.map((rowData, rowIndex) => {
                  const rowAddress = parseInt(address, 16) + rowIndex * bytesPerRow
                  return (
                    <tr key={rowIndex} className="hover:bg-gray-800 border-b border-gray-800/50 transition-colors">
                      <td 
                        className="memory-address px-4 py-2 cursor-pointer hover:text-yellow-300 transition-colors"
                        onClick={() => copyToClipboard('0x' + rowAddress.toString(16).toUpperCase())}
                        title="Click to copy address"
                      >
                        0x{rowAddress.toString(16).toUpperCase().padStart(is64Bit ? 16 : 8, '0')}
                      </td>
                      
                      {viewMode !== 'ascii' && rowData.map((byte, byteIndex) => {
                        const isSelected = selectedByte?.row === rowIndex && selectedByte?.col === byteIndex
                        return (
                          <td 
                            key={byteIndex} 
                            className={`memory-byte px-2 py-2 text-center transition-all ${
                              isSelected ? 'selected' : ''
                            }`}
                            onClick={() => handleByteClick(rowIndex, byteIndex, byte)}
                            title={`Address: 0x${(rowAddress + byteIndex).toString(16).toUpperCase()}\nValue: ${byte} (0x${byte.toString(16).toUpperCase()})\nClick to copy`}
                            onDoubleClick={() => handleCellDoubleClick(rowIndex, byteIndex, byte)}
                          >
                            {editingCell?.row === rowIndex && editingCell?.col === byteIndex ? (
                              <input 
                                value={editingValue}
                                onChange={handleCellValueChange}
                                onBlur={() => handleCellValueUpdate(rowIndex, byteIndex)}
                                className="w-full bg-gray-700 border border-gray-600 rounded-lg px-3 py-2 text-white focus:border-blue-400 focus:outline-none"
                              />
                            ) : (
                              byte.toString(16).toUpperCase().padStart(2, '0')
                            )}
                          </td>
                        )
                      })}
                      
                      {(viewMode === 'both' || viewMode === 'ascii') && (
                        <td className="px-4 py-2 text-gray-300 font-medium">
                          {renderMemoryValue(rowData, 'ascii')}
                        </td>
                      )}
                      
                      {dataType !== 'bytes' && (
                        <td className="px-4 py-2 text-blue-400 font-medium">
                          {renderMemoryValue(rowData, dataType as any)}
                        </td>
                      )}
                    </tr>
                  )
                })}
              </tbody>
            </table>
          </div>
        ) : activeTab === 'disassembly' && disassemblyData.length > 0 ? (
          <div className="disassembly-grid">
            <table className="w-full text-sm mono">
              <thead>
                <tr className="bg-gray-800 border-b border-gray-700 sticky top-0">
                  <th className="text-left px-4 py-3 font-semibold w-32">Address</th>
                  <th className="text-left px-4 py-3 font-semibold w-40">Bytes</th>
                  <th className="text-left px-4 py-3 font-semibold flex-1">Instruction</th>
                  <th className="text-left px-4 py-3 font-semibold w-64">Comment</th>
                </tr>
              </thead>
              <tbody>
                {disassemblyData.map((instruction, index) => {
                  const isJumpTarget = disassemblyData.some(inst => 
                    inst.target && parseInt(inst.target, 16) === parseInt(instruction.address, 16)
                  )
                  const isControlFlow = instruction.isJump || instruction.isCall || instruction.isRet
                  
                  return (
                    <tr 
                      key={index} 
                      className={`hover:bg-gray-800 border-b border-gray-800/50 transition-colors ${
                        isJumpTarget ? 'bg-yellow-900/20 border-yellow-600/30' : ''
                      } ${isControlFlow ? 'bg-blue-900/10' : ''}`}
                      onContextMenu={(e) => handleContextMenu(e, instruction)}
                    >
                      <td className="px-4 py-2 text-yellow-400 font-medium relative">
                        {isJumpTarget && (
                          <div className="absolute left-0 top-0 bottom-0 w-1 bg-yellow-400"></div>
                        )}
                        0x{instruction.address.toUpperCase().padStart(is64Bit ? 16 : 8, '0')}
                      </td>
                      <td className="px-4 py-2 text-gray-300 font-mono text-xs">
                        {instruction.bytes.padEnd(20, ' ')}
                      </td>
                      <td className="px-4 py-2 relative">
                        <div className="flex items-center">
                          {/* Control flow indicators */}
                          {instruction.isCall && (
                            <FiCornerDownRight className="text-green-400 mr-2" size={12} />
                          )}
                          {instruction.isJump && (
                            <FiNavigation className="text-orange-400 mr-2" size={12} />
                          )}
                          {instruction.isRet && (
                            <span className="text-red-400 mr-2 text-xs">←</span>
                          )}
                          
                          <div className="flex-1">
                            {/* Mnemonic with syntax highlighting */}
                            <span className={`font-semibold ${
                              instruction.isCall ? 'text-green-400' :
                              instruction.isJump ? 'text-orange-400' :
                              instruction.isRet ? 'text-red-400' :
                              instruction.mnemonic.startsWith('mov') ? 'text-blue-400' :
                              ['add', 'sub', 'mul', 'div', 'and', 'or', 'xor'].some(op => instruction.mnemonic.startsWith(op)) ? 'text-purple-400' :
                              ['cmp', 'test'].some(op => instruction.mnemonic.startsWith(op)) ? 'text-yellow-400' :
                              'text-blue-400'
                            }`}>
                              {instruction.mnemonic}
                            </span>
                            
                            {instruction.operands && (
                              <span className="ml-2">
                                {/* Enhanced operand highlighting */}
                                {instruction.operands.split(',').map((operand, opIndex) => (
                                  <span key={opIndex}>
                                    {opIndex > 0 && <span className="text-gray-500">, </span>}
                                    <span className={
                                      operand.trim().startsWith('0x') ? 'text-cyan-300' :
                                      operand.trim().match(/^[re][a-z]{2}$/) ? 'text-green-300' :
                                      operand.trim().startsWith('[') ? 'text-orange-300' :
                                      'text-gray-200'
                                    }>
                                      {operand.trim()}
                                    </span>
                                  </span>
                                ))}
                              </span>
                            )}
                          </div>
                        </div>
                        
                        {/* Jump target indicator */}
                        {instruction.target && (
                          <div className="absolute right-2 top-1/2 transform -translate-y-1/2">
                            <span 
                              className="text-xs text-cyan-400 cursor-pointer hover:text-cyan-300"
                              onClick={() => {
                                const targetAddr = '0x' + parseInt(instruction.target!, 16).toString(16).toUpperCase().padStart(is64Bit ? 16 : 8, '0')
                                setAddress(targetAddr)
                                loadMemory()
                              }}
                              title={`Jump to ${instruction.target}`}
                            >
                              → {instruction.target}
                            </span>
                          </div>
                        )}
                      </td>
                      <td className="px-4 py-2">
                        {editingComment === instruction.address ? (
                          <input
                            type="text"
                            defaultValue={instruction.comment || ''}
                            onKeyDown={(e) => {
                              if (e.key === 'Enter') {
                                addComment(instruction.address, e.currentTarget.value)
                                setEditingComment(null)
                              } else if (e.key === 'Escape') {
                                setEditingComment(null)
                              }
                            }}
                            onBlur={(e) => {
                              addComment(instruction.address, e.currentTarget.value)
                              setEditingComment(null)
                            }}
                            autoFocus
                            className="w-full bg-gray-700 border border-gray-600 rounded px-2 py-1 text-white focus:border-blue-400 focus:outline-none"
                            placeholder="Add comment..."
                          />
                        ) : (
                          <span 
                            className="text-green-400 cursor-pointer hover:text-green-300 text-sm"
                            onClick={() => setEditingComment(instruction.address)}
                          >
                            {instruction.comment || '// Click to add comment'}
                          </span>
                        )}
                      </td>
                    </tr>
                  )
                })}
              </tbody>
            </table>
          </div>
        ) : (
          <div className="flex items-center justify-center h-full">
            <div className="text-center">
              <FiCode className="mx-auto text-gray-600 mb-4" size={48} />
              <div className="text-gray-400 text-lg mb-2">
                {isConnected ? `Enter a memory address to view ${activeTab}` : 'Connect to engine to view memory'}
              </div>
              <div className="text-gray-500 text-sm">
                Use the address input above to start exploring memory
              </div>
            </div>
          </div>
        )}
      </div>
      
      {/* Navigation Footer */}
      {memoryData.length > 0 && (
        <div className="bg-gray-800 border-t border-gray-700 p-4">
          <div className="flex justify-between items-center">
            <div className="flex gap-2">
              <button
                onClick={() => navigateByPages(-1)}
                className="flex items-center gap-2 px-3 py-2 bg-gray-700 hover:bg-gray-600 border border-gray-600 rounded-lg transition-colors text-sm"
              >
                ⬅️ Previous Page
              </button>
              <button
                onClick={() => navigateByRows(-1)}
                className="flex items-center gap-2 px-3 py-2 bg-gray-700 hover:bg-gray-600 border border-gray-600 rounded-lg transition-colors text-sm"
              >
                ⬆️ Up Row
              </button>
              <button
                onClick={() => navigateByRows(1)}
                className="flex items-center gap-2 px-3 py-2 bg-gray-700 hover:bg-gray-600 border border-gray-600 rounded-lg transition-colors text-sm"
              >
                ⬇️ Down Row
              </button>
              <button
                onClick={() => navigateByPages(1)}
                className="flex items-center gap-2 px-3 py-2 bg-gray-700 hover:bg-gray-600 border border-gray-600 rounded-lg transition-colors text-sm"
              >
                Next Page ➡️
              </button>
            </div>
            
            <div className="text-sm text-gray-400">
              Showing <span className="text-white font-medium">{memoryData.length * bytesPerRow}</span> bytes from{' '}
              <span className="text-blue-400 font-mono">{address}</span>
            </div>
          </div>
        </div>
      )}
      
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