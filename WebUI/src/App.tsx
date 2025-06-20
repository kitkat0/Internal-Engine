import React, { useRef } from 'react'
import Draggable from 'react-draggable'
import { Resizable } from 're-resizable'
import {
  FiX, FiMinimize2, FiMaximize2, FiCpu, FiList, FiSearch, 
  FiCode, FiTerminal, FiLayers, FiDatabase, FiZap, FiPower
} from 'react-icons/fi'
import { useWindowManagerStore, WindowInstance, WindowType } from './store/windowManager'
import { useEngineStore } from './store/engineStore'
import ProcessInfo from './components/ProcessInfo'
import ModuleList from './components/ModuleList'
import MemoryViewer from './components/MemoryViewer'
import AddressList from './components/AddressList'
import Scanner from './components/Scanner'
import Disassembler from './components/Disassembler'
import Console from './components/Console'
import AddEditAddressModal from './components/AddEditAddressModal'

const windowComponents: Record<WindowType, React.FC<any>> = {
  PROCESS_INFO: ProcessInfo,
  MODULE_LIST: ModuleList,
  MEMORY_VIEWER: MemoryViewer,
  ADDRESS_LIST: AddressList,
  SCANNER: Scanner,
  DISASSEMBLER: Disassembler,
  CONSOLE: Console,
  ADD_EDIT_ADDRESS: AddEditAddressModal,
}

const Window: React.FC<{ instance: WindowInstance }> = ({ instance }) => {
  const { 
    closeWindow, focusWindow, updateWindowPosition, updateWindowSize, toggleMinimize, activeWindowId 
  } = useWindowManagerStore()
  
  const nodeRef = useRef(null)

  const Component = windowComponents[instance.type]

  return (
    <Draggable
      nodeRef={nodeRef}
      handle=".handle"
      position={{ x: instance.x, y: instance.y }}
      onStart={() => focusWindow(instance.id)}
      onStop={(e, data) => updateWindowPosition(instance.id, data.x, data.y)}
      bounds="parent"
    >
      <div ref={nodeRef} style={{ zIndex: instance.zIndex, display: instance.isMinimized ? 'none' : 'block', position: 'absolute' }}>
        <Resizable
          size={{ width: instance.width, height: instance.height }}
          onResizeStart={() => focusWindow(instance.id)}
          onResizeStop={(e, direction, ref, d) => {
            updateWindowSize(instance.id, instance.width + d.width, instance.height + d.height)
          }}
          minWidth={200}
          minHeight={150}
          style={{
            borderWidth: activeWindowId === instance.id ? '2px' : '1px',
            borderColor: activeWindowId === instance.id ? '#3b82f6' : '#4b5563',
          }}
          className="bg-gray-800 rounded-lg flex flex-col"
        >
          <div className="handle h-10 bg-gray-900 rounded-t-lg flex items-center justify-between px-3 cursor-move flex-shrink-0">
            <span className="font-semibold text-white truncate">{instance.title}</span>
            <div className="flex items-center gap-2">
              <button onClick={() => toggleMinimize(instance.id)} className="p-1 hover:bg-gray-700 rounded"><FiMinimize2 size={14} /></button>
              <button onClick={() => closeWindow(instance.id)} className="p-1 hover:bg-red-500 rounded"><FiX size={14} /></button>
            </div>
          </div>
          
          <div className="flex-1 overflow-auto bg-gray-800 rounded-b-lg">
            <Component />
          </div>
        </Resizable>
      </div>
    </Draggable>
  )
}

const Taskbar: React.FC = () => {
  const { windows, openWindow, focusWindow, toggleMinimize } = useWindowManagerStore()
  const { isConnected, connect, disconnect, connectionStatus } = useEngineStore()

  const tools = [
    { type: 'PROCESS_INFO' as WindowType, icon: FiCpu, name: 'Process' },
    { type: 'MODULE_LIST' as WindowType, icon: FiLayers, name: 'Modules' },
    { type: 'MEMORY_VIEWER' as WindowType, icon: FiDatabase, name: 'Memory' },
    { type: 'ADDRESS_LIST' as WindowType, icon: FiList, name: 'Addresses' },
    { type: 'SCANNER' as WindowType, icon: FiSearch, name: 'Scanner' },
    { type: 'DISASSEMBLER' as WindowType, icon: FiCode, name: 'Disasm' },
    { type: 'CONSOLE' as WindowType, icon: FiTerminal, name: 'Console' },
  ]
  
  const getStatusIndicator = () => {
    switch(connectionStatus) {
      case 'connected':
        return <div className="w-3 h-3 rounded-full bg-green-400" title="Connected" />
      case 'connecting':
        return <div className="w-3 h-3 rounded-full bg-yellow-400 animate-pulse" title="Connecting..." />
      case 'error':
        return <div className="w-3 h-3 rounded-full bg-red-400" title="Connection Error" />
      default:
        return <div className="w-3 h-3 rounded-full bg-gray-500" title="Disconnected" />
    }
  }
  
  return (
    <div className="absolute bottom-0 left-0 right-0 h-16 bg-gray-900/80 backdrop-blur-md border-t border-gray-700 flex items-center justify-between px-4">
      {/* Start Menu / Main Tools */}
      <div className="flex items-center gap-2">
        <div className="flex items-center gap-3 pr-4 border-r border-gray-700">
          <div className="w-8 h-8 bg-gradient-to-br from-blue-500 to-purple-600 rounded-lg flex items-center justify-center">
            <FiZap className="text-white" size={18} />
          </div>
          <h1 className="text-lg font-bold text-white">Internal Engine</h1>
        </div>
        
        <div className="flex items-center gap-1">
          {tools.map(tool => (
            <button
              key={tool.type}
              onClick={() => openWindow(tool.type)}
              className="flex flex-col items-center justify-center p-2 rounded-lg hover:bg-gray-700 w-20 h-14 transition-colors"
              title={`Open ${tool.name}`}
            >
              <tool.icon size={20} className="text-gray-300" />
              <span className="text-xs mt-1 text-gray-400">{tool.name}</span>
            </button>
          ))}
        </div>
      </div>
      
      <div className="flex-1 flex items-center justify-center">
        {/* Open Windows */}
        <div className="flex items-center gap-2">
          {windows.map(win => (
            <button
              key={win.id}
              onClick={() => {
                if (win.isMinimized) toggleMinimize(win.id)
                focusWindow(win.id)
              }}
              className={`px-4 py-2 rounded-lg text-sm transition-all duration-150 border-b-2 ${
                win.isMinimized 
                  ? 'bg-gray-700/50 text-gray-400 border-transparent hover:bg-gray-700'
                  : 'bg-blue-600/80 text-white border-blue-400'
              }`}
            >
              {win.title}
            </button>
          ))}
        </div>
      </div>
      
      {/* System Tray */}
      <div className="flex items-center gap-4">
        <div className="flex items-center gap-2">
          {getStatusIndicator()}
          <span className="text-sm text-gray-300">{connectionStatus}</span>
        </div>
        <button 
          onClick={isConnected ? disconnect : connect}
          className={`p-2 rounded-lg transition-colors ${
            isConnected ? 'hover:bg-red-500' : 'hover:bg-green-500'
          }`}
        >
          <FiPower size={18} />
        </button>
      </div>
    </div>
  )
}

function App() {
  const { windows, openWindow } = useWindowManagerStore()
  const { connect } = useEngineStore()
  
  const initRef = useRef(false)

  React.useEffect(() => {
    if (initRef.current) return
    initRef.current = true
    
    connect()
    openWindow('PROCESS_INFO')
  }, [])

  return (
    <div className="h-screen bg-gray-900 text-white overflow-hidden relative">
      {/* Workspace Area */}
      <div className="w-full h-full bg-grid-pattern">
        {windows.map(win => (
          <Window key={win.id} instance={win} />
        ))}
      </div>
      
      {/* Taskbar */}
      <Taskbar />
    </div>
  )
}

export default App