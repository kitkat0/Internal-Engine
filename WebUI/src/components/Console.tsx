import React from 'react'
import { FiSend } from 'react-icons/fi'
import { useEngineStore } from '../store/engineStore'

interface ConsoleMessage {
  id: number
  type: 'input' | 'output' | 'error'
  text: string
  timestamp: Date
}

export default function Console() {
  const [input, setInput] = React.useState('')
  const [messages, setMessages] = React.useState<ConsoleMessage[]>([
    {
      id: 1,
      type: 'output',
      text: 'Internal Engine Console v1.0',
      timestamp: new Date()
    },
    {
      id: 2,
      type: 'output',
      text: 'Type "help" for available commands',
      timestamp: new Date()
    }
  ])
  const messagesEndRef = React.useRef<HTMLDivElement>(null)
  
  const isConnected = useEngineStore(state => state.isConnected)
  const client = useEngineStore(state => state.client)
  
  const scrollToBottom = () => {
    messagesEndRef.current?.scrollIntoView({ behavior: 'smooth' })
  }
  
  React.useEffect(() => {
    scrollToBottom()
  }, [messages])
  
  const addMessage = (type: ConsoleMessage['type'], text: string) => {
    setMessages(prev => [...prev, {
      id: Date.now(),
      type,
      text,
      timestamp: new Date()
    }])
  }
  
  const executeCommand = async (command: string) => {
    if (!command.trim()) return
    
    // Add input message
    addMessage('input', command)
    
    // Clear input
    setInput('')
    
    // Process command
    try {
      if (command === 'help') {
        addMessage('output', `Available commands:
  help              - Show this help message
  clear             - Clear console
  modules           - List loaded modules
  scan <value>      - Scan memory for value
  read <addr> <size> - Read memory at address
  write <addr> <val> - Write value to address
  pattern <pattern> - Pattern scan (e.g., "48 8B ?? ?? 89")`)
      } else if (command === 'clear') {
        setMessages([])
      } else if (command === 'modules') {
        if (!isConnected) {
          addMessage('error', 'Not connected to engine')
          return
        }
        
        const refreshModules = useEngineStore.getState().refreshModules
        await refreshModules()
        
        const modules = useEngineStore.getState().processInfo?.modules || []
        modules.forEach(mod => {
          addMessage('output', `${mod.name} - Base: ${mod.base} Size: ${mod.size}`)
        })
      } else {
        // Send raw command to engine
        if (client) {
          const response = await client.sendCommand('console.execute', { command })
          addMessage(response.success ? 'output' : 'error', response.data || response.error)
        } else {
          addMessage('error', 'Not connected to engine')
        }
      }
    } catch (error) {
      addMessage('error', `Error: ${error}`)
    }
  }
  
  const handleKeyPress = (e: React.KeyboardEvent) => {
    if (e.key === 'Enter') {
      executeCommand(input)
    }
  }
  
  return (
    <div className="h-full flex flex-col p-4">
      {/* Messages */}
      <div className="flex-1 overflow-auto bg-ce-bg-light rounded-t border border-b-0 border-ce-border p-3 font-mono text-sm">
        {messages.map(msg => (
          <div key={msg.id} className="mb-1">
            {msg.type === 'input' && (
              <span className="text-ce-accent">&gt; {msg.text}</span>
            )}
            {msg.type === 'output' && (
              <span className="text-ce-text">{msg.text}</span>
            )}
            {msg.type === 'error' && (
              <span className="text-ce-error">{msg.text}</span>
            )}
          </div>
        ))}
        <div ref={messagesEndRef} />
      </div>
      
      {/* Input */}
      <div className="flex bg-ce-bg-light rounded-b border border-ce-border">
        <input
          type="text"
          value={input}
          onChange={e => setInput(e.target.value)}
          onKeyPress={handleKeyPress}
          className="flex-1 bg-transparent border-none outline-none px-3 py-2 font-mono text-sm"
          placeholder="Enter command..."
          disabled={!isConnected}
        />
        <button
          onClick={() => executeCommand(input)}
          disabled={!isConnected}
          className="px-3"
        >
          <FiSend />
        </button>
      </div>
    </div>
  )
}