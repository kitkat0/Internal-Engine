import React from 'react'
import { FiLayers, FiRefreshCw } from 'react-icons/fi'
import { useEngineStore } from '../store/engineStore'

export default function ModuleList() {
    const { sendCommand, isConnected } = useEngineStore()
    const [modules, setModules] = React.useState<any[]>([])
    const [loading, setLoading] = React.useState(false)

    const loadModules = async () => {
        if (!isConnected) return
        setLoading(true)
        try {
            const response = await sendCommand({ command: 'module.list' })
            if (response.success && response.data) {
                setModules(response.data)
            }
        } catch (error) {
            console.error('Failed to load modules:', error)
        } finally {
            setLoading(false)
        }
    }

    React.useEffect(() => {
        loadModules()
    }, [isConnected])

    return (
        <div className="h-full flex flex-col bg-gray-800 text-white">
            <div className="flex-1 overflow-auto">
                {modules.length === 0 ? (
                    <div className="p-4 text-center text-gray-400">
                        {loading ? 'Loading modules...' : 'No modules found'}
                    </div>
                ) : (
                    <div className="divide-y divide-gray-700">
                        {modules.map((module, index) => (
                            <div key={index} className="p-3 hover:bg-gray-700 transition-colors cursor-pointer">
                                <div className="flex items-center justify-between mb-1">
                                    <span className="text-white font-medium text-sm truncate" title={module.name}>{module.name}</span>
                                    <span className="text-xs text-gray-400 font-mono">
                                        {module.size ? `${(module.size / 1024).toFixed(0)}KB` : ''}
                                    </span>
                                </div>
                                <div className="text-xs text-gray-400 font-mono">
                                    Base: 0x{(typeof module.baseAddress === 'string' ?
                                        parseInt(module.baseAddress, 16) :
                                        module.baseAddress)?.toString(16).toUpperCase() || 'Unknown'}
                                </div>
                                {module.path && (
                                    <div className="text-xs text-gray-500 truncate mt-1" title={module.path}>
                                        {module.path}
                                    </div>
                                )}
                            </div>
                        ))}
                    </div>
                )}
            </div>
        </div>
    )
} 