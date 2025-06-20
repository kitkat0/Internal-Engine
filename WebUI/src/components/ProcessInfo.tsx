import React from 'react'
import { FiCpu, FiTerminal, FiDatabase, FiHardDrive } from 'react-icons/fi'
import { useEngineStore } from '../store/engineStore'

const InfoRow = ({ label, value, valueClass = '' }: { label: string, value: React.ReactNode, valueClass?: string }) => (
  <div className="flex justify-between items-center py-2 px-3 hover:bg-gray-700/50 rounded-md">
    <span className="text-sm text-gray-400">{label}</span>
    <span className={`text-sm font-medium text-white ${valueClass}`}>{value}</span>
  </div>
)

const ProgressBar = ({ value, max, colorClass }: { value: number, max: number, colorClass: string }) => {
    const percentage = max > 0 ? (value / max) * 100 : 0
    return (
        <div className="w-full bg-gray-600 rounded-full h-2.5">
            <div className={`${colorClass} h-2.5 rounded-full`} style={{ width: `${percentage}%` }}></div>
        </div>
    )
}

const ProcessInfo: React.FC = () => {
  const { processInfo, isConnected, connectionStatus } = useEngineStore()
  
  const getConnectionStatusColor = () => {
    switch (connectionStatus) {
      case 'connected': return 'text-green-400'
      case 'error': return 'text-red-400'
      case 'connecting': return 'text-yellow-400'
      default: return 'text-gray-400'
    }
  }

  const getConnectionStatusText = () => {
    switch (connectionStatus) {
      case 'connected': return 'Connected to Target Process'
      case 'error': return 'Connection Error - DLL Not Found'
      case 'connecting': return 'Connecting to DLL...'
      default: return 'Disconnected'
    }
  }

  if (!isConnected || !processInfo) {
    return (
      <div className="h-full flex items-center justify-center text-gray-500">
        Not connected to any process.
      </div>
    )
  }

  const mainModule = (processInfo as any).mainModule || {}
  const memoryMetrics = (processInfo as any).memoryMetrics || {}

  return (
    <div className="h-full flex flex-col bg-gray-800 text-white p-4 space-y-5">
      
      <div>
        <h3 className="text-md font-semibold text-gray-300 mb-2 flex items-center gap-2"><FiCpu /><span>Core Information</span></h3>
        <div className="bg-gray-900/50 rounded-lg p-2 space-y-1">
          <InfoRow label="Process Name" value={processInfo.name} valueClass="font-bold text-blue-300 truncate" />
          <InfoRow label="Process ID" value={processInfo.pid} valueClass="font-mono" />
          <InfoRow label="Architecture" value={(processInfo as any).platform || 'N/A'} valueClass="font-mono" />
        </div>
      </div>

      <div>
        <h3 className="text-md font-semibold text-gray-300 mb-2 flex items-center gap-2"><FiHardDrive /><span>Main Module</span></h3>
        <div className="bg-gray-900/50 rounded-lg p-2 space-y-1">
          <InfoRow label="Base Address" value={`0x${mainModule.baseAddress?.toString(16).toUpperCase() || 'N/A'}`} valueClass="font-mono text-yellow-300" />
          <InfoRow label="Size" value={mainModule.size ? `${(mainModule.size / 1024 / 1024).toFixed(2)} MB` : 'N/A'} valueClass="font-mono" />
        </div>
      </div>
      
      <div>
        <h3 className="text-md font-semibold text-gray-300 mb-2 flex items-center gap-2"><FiDatabase /><span>Memory Metrics</span></h3>
        <div className="bg-gray-900/50 rounded-lg p-3 space-y-4">
            <div>
                <div className="flex justify-between text-xs text-gray-400 mb-1">
                    <span>Total</span>
                    <span>{memoryMetrics.total ? (memoryMetrics.total / 1024 / 1024 / 1024).toFixed(2) : 'N/A'} GB</span>
                </div>
                <ProgressBar value={memoryMetrics.total || 0} max={memoryMetrics.total || 1} colorClass="bg-blue-500" />
            </div>
            <div>
                <div className="flex justify-between text-xs text-gray-400 mb-1">
                    <span>Writable</span>
                    <span>{memoryMetrics.writable ? (memoryMetrics.writable / 1024 / 1024).toFixed(2) : 'N/A'} MB</span>
                </div>
                <ProgressBar value={memoryMetrics.writable || 0} max={memoryMetrics.total || 1} colorClass="bg-green-500" />
            </div>
            <div>
                <div className="flex justify-between text-xs text-gray-400 mb-1">
                    <span>Executable</span>
                     <span>{memoryMetrics.executable ? (memoryMetrics.executable / 1024 / 1024).toFixed(2) : 'N/A'} MB</span>
                </div>
                <ProgressBar value={memoryMetrics.executable || 0} max={memoryMetrics.total || 1} colorClass="bg-red-500" />
            </div>
        </div>
      </div>

    </div>
  )
}

export default ProcessInfo