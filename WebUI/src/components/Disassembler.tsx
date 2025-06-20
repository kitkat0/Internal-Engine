import React from 'react'
import { 
  FiCode, FiSearch, FiNavigation, FiRefreshCw, FiCpu, 
  FiCornerDownRight, FiMessageSquare, FiCopy, FiSettings
} from 'react-icons/fi'
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

export default function Disassembler() {
  const [address, setAddress] = React.useState('0x00400000')
  const [instructionCount, setInstructionCount] = React.useState(50)
  const [disassemblyData, setDisassemblyData] = React.useState<DisassemblyInstruction[]>([])
  const [loading, setLoading] = React.useState(false)
  const [error, setError] = React.useState<string | null>(null)
  const [comments, setComments] = React.useState<Record<string, string>>({})
  const [editingComment, setEditingComment] = React.useState<string | null>(null)
  const [showSettings, setShowSettings] = React.useState(false)
  
  const processInfo = useEngineStore(state => state.processInfo)
  const is64Bit = processInfo?.addressWidth === 64
  const { disassembleMemory, disassembly, isConnected } = useEngineStore()
  
  // Context menu
  const { contextMenu, handleContextMenu, closeContextMenu } = useContextMenu()
  
  const loadDisassembly = async () => {
    if (!isConnected) {
      setError('Not connected to engine')
      return
    }
    
    setLoading(true)
    setError(null)
    
    try {
      // Estimate bytes needed for instructions (average 4 bytes per instruction)
      const estimatedSize = instructionCount * 4
      await disassembleMemory(address, estimatedSize)
      
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
          
          setDisassemblyData(instructions.slice(0, instructionCount))
        } catch (parseError) {
          console.error('Failed to parse disassembly:', parseError)
          setError('Failed to parse disassembly data')
          setDisassemblyData([])
        }
      }
    } catch (error) {
      console.error('Failed to disassemble:', error)
      setError(`Disassembly failed: ${error}`)
    } finally {
      setLoading(false)
    }
  }
  
  const handleAddressChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    setAddress(e.target.value)
    setError(null)
  }
  
  const handleKeyPress = (e: React.KeyboardEvent) => {
    if (e.key === 'Enter') {
      loadDisassembly()
    }
  }
  
  const navigateToAddress = (newAddress: string) => {
    setAddress(newAddress)
    loadDisassembly()
  }
  
  const addComment = (instructionAddress: string, comment: string) => {
    setComments(prev => ({ ...prev, [instructionAddress]: comment }))
    setDisassemblyData(prev => prev.map(inst => 
      inst.address === instructionAddress ? { ...inst, comment } : inst
    ))
  }
  
  const getContextMenuItems = (instruction: DisassemblyInstruction): ContextMenuItem[] => [
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
      onClick: () => navigateToAddress('0x' + instruction.address)
    },
    instruction.target ? {
      label: 'Follow jump/call',
      icon: FiCornerDownRight,
      onClick: () => {
        const targetAddr = '0x' + parseInt(instruction.target!, 16).toString(16).toUpperCase().padStart(is64Bit ? 16 : 8, '0')
        navigateToAddress(targetAddr)
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
  ].filter(Boolean) as ContextMenuItem[]
  
  React.useEffect(() => {
    if (isConnected && !error && !loading) {
      loadDisassembly()
    }
  }, [isConnected])
  
  return (
    <div className="h-full flex flex-col bg-gray-900 text-white">
      {/* Header */}
      <div className="bg-gray-800 border-b border-gray-700 p-4">
        <div className="flex items-center justify-between mb-4">
          <div className="flex items-center gap-3">
            <FiCode className="text-purple-400" size={20} />
            <h2 className="text-lg font-semibold">Advanced Disassembler</h2>
          </div>
          <div className="flex items-center gap-2">
            <button
              onClick={() => setShowSettings(!showSettings)}
              className={`flex items-center gap-2 px-3 py-2 rounded-lg transition-colors ${
                showSettings ? 'bg-purple-600 text-white' : 'bg-gray-700 hover:bg-gray-600 text-gray-300'
              }`}
            >
              <FiSettings size={16} />
              Settings
            </button>
            <button
              onClick={loadDisassembly}
              disabled={loading || !isConnected}
              className="flex items-center gap-2 px-4 py-2 bg-purple-600 hover:bg-purple-700 disabled:bg-gray-600 rounded-lg transition-colors"
            >
              <FiRefreshCw className={loading ? 'animate-spin' : ''} size={16} />
              {loading ? 'Disassembling...' : 'Disassemble'}
            </button>
          </div>
        </div>
        
        {/* Controls */}
        <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
          <div className="space-y-2">
            <label className="text-sm font-medium text-gray-300 flex items-center gap-2">
              <FiNavigation size={14} />
              Start Address
            </label>
            <input
              type="text"
              value={address}
              onChange={handleAddressChange}
              onKeyPress={handleKeyPress}
              className="w-full mono bg-gray-700 border border-gray-600 rounded-lg px-3 py-2 text-white focus:border-purple-400 focus:outline-none"
              placeholder="0x00400000"
            />
          </div>
          
          <div className="space-y-2">
            <label className="text-sm font-medium text-gray-300">Instructions</label>
            <input
              type="number"
              value={instructionCount}
              onChange={e => setInstructionCount(Math.max(1, Math.min(200, Number(e.target.value))))}
              className="w-full bg-gray-700 border border-gray-600 rounded-lg px-3 py-2 text-white focus:border-purple-400 focus:outline-none"
              min={1}
              max={200}
            />
          </div>
          
          <div className="space-y-2">
            <label className="text-sm font-medium text-gray-300">Architecture</label>
            <div className="px-3 py-2 bg-gray-700 border border-gray-600 rounded-lg text-gray-300">
              {is64Bit ? 'x86-64' : 'x86-32'}
            </div>
          </div>
        </div>
        
        {/* Settings Panel */}
        {showSettings && (
          <div className="mt-4 p-4 bg-gray-700 rounded-lg">
            <h3 className="text-sm font-semibold mb-3">Disassembly Settings</h3>
            <div className="grid grid-cols-2 gap-4">
              <label className="flex items-center gap-2">
                <input type="checkbox" className="rounded" defaultChecked />
                <span className="text-sm">Show opcodes</span>
              </label>
              <label className="flex items-center gap-2">
                <input type="checkbox" className="rounded" defaultChecked />
                <span className="text-sm">Show addresses</span>
              </label>
              <label className="flex items-center gap-2">
                <input type="checkbox" className="rounded" defaultChecked />
                <span className="text-sm">Syntax highlighting</span>
              </label>
              <label className="flex items-center gap-2">
                <input type="checkbox" className="rounded" defaultChecked />
                <span className="text-sm">Control flow analysis</span>
              </label>
            </div>
          </div>
        )}
      </div>
      
      {/* Error Display */}
      {error && (
        <div className="bg-red-900/30 border border-red-600/30 p-4 m-4 rounded-lg">
          <div className="flex items-center gap-2">
            <span className="text-red-400">⚠️</span>
            <span className="text-red-300">{error}</span>
          </div>
        </div>
      )}
      
      {/* Disassembly Display */}
      <div className="flex-1 overflow-auto bg-gray-900">
        {loading ? (
          <div className="flex items-center justify-center h-full">
            <div className="flex flex-col items-center gap-4">
              <FiRefreshCw className="animate-spin text-purple-400" size={32} />
              <span className="text-gray-400">Disassembling instructions...</span>
            </div>
          </div>
        ) : disassemblyData.length > 0 ? (
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
                    } ${isControlFlow ? 'bg-purple-900/10' : ''}`}
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
                              navigateToAddress(targetAddr)
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
                          className="w-full bg-gray-700 border border-gray-600 rounded px-2 py-1 text-white focus:border-purple-400 focus:outline-none"
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
        ) : (
          <div className="flex items-center justify-center h-full">
            <div className="text-center">
              <FiCode className="mx-auto text-gray-600 mb-4" size={48} />
              <div className="text-gray-400 text-lg mb-2">
                {isConnected ? 'Enter a memory address to disassemble' : 'Connect to engine to start disassembling'}
              </div>
              <div className="text-gray-500 text-sm">
                Use the address input above to start analyzing assembly code
              </div>
            </div>
          </div>
        )}
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