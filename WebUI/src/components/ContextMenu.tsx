import React from 'react'
import { IconType } from 'react-icons'

export interface ContextMenuItem {
  label: string
  icon?: IconType
  onClick: () => void
  divider?: boolean
  className?: string
  disabled?: boolean
}

interface ContextMenuProps {
  x: number
  y: number
  items: ContextMenuItem[]
  onClose: () => void
}

export default function ContextMenu({ x, y, items, onClose }: ContextMenuProps) {
  const menuRef = React.useRef<HTMLDivElement>(null)
  
  // Adjust position to ensure menu stays within viewport
  React.useEffect(() => {
    if (menuRef.current) {
      const rect = menuRef.current.getBoundingClientRect()
      const viewportWidth = window.innerWidth
      const viewportHeight = window.innerHeight
      
      let adjustedX = x
      let adjustedY = y
      
      // Adjust horizontal position
      if (x + rect.width > viewportWidth) {
        adjustedX = x - rect.width
      }
      
      // Adjust vertical position
      if (y + rect.height > viewportHeight) {
        adjustedY = y - rect.height
      }
      
      menuRef.current.style.left = `${adjustedX}px`
      menuRef.current.style.top = `${adjustedY}px`
    }
  }, [x, y])
  
  // Close on click outside
  React.useEffect(() => {
    const handleClick = (e: MouseEvent) => {
      if (menuRef.current && !menuRef.current.contains(e.target as Node)) {
        onClose()
      }
    }
    
    const handleContextMenu = (e: MouseEvent) => {
      if (menuRef.current && !menuRef.current.contains(e.target as Node)) {
        onClose()
      }
    }
    
    const handleEscape = (e: KeyboardEvent) => {
      if (e.key === 'Escape') {
        onClose()
      }
    }
    
    document.addEventListener('click', handleClick)
    document.addEventListener('contextmenu', handleContextMenu)
    document.addEventListener('keydown', handleEscape)
    
    return () => {
      document.removeEventListener('click', handleClick)
      document.removeEventListener('contextmenu', handleContextMenu)
      document.removeEventListener('keydown', handleEscape)
    }
  }, [onClose])
  
  return (
    <div
      ref={menuRef}
      className="fixed bg-gray-800 border border-gray-700 rounded-lg shadow-xl py-1 z-50"
      style={{ left: x, top: y }}
      onClick={(e) => e.stopPropagation()}
      onContextMenu={(e) => e.preventDefault()}
    >
      {items.map((item, index) => {
        if (item.divider) {
          return <div key={index} className="border-t border-gray-700 my-1" />
        }
        
        const Icon = item.icon
        
        return (
          <button
            key={index}
            onClick={() => {
              if (!item.disabled) {
                item.onClick()
                onClose()
              }
            }}
            disabled={item.disabled}
            className={`w-full px-4 py-2 text-left hover:bg-gray-700 flex items-center gap-2 transition-colors ${
              item.disabled ? 'opacity-50 cursor-not-allowed' : ''
            } ${item.className || ''}`}
          >
            {Icon && <Icon size={14} />}
            {item.label}
          </button>
        )
      })}
    </div>
  )
}

// Context menu hook for easier usage
export function useContextMenu() {
  const [contextMenu, setContextMenu] = React.useState<{
    x: number
    y: number
    data?: any
  } | null>(null)
  
  const handleContextMenu = React.useCallback((e: React.MouseEvent, data?: any) => {
    e.preventDefault()
    setContextMenu({
      x: e.clientX,
      y: e.clientY,
      data
    })
  }, [])
  
  const closeContextMenu = React.useCallback(() => {
    setContextMenu(null)
  }, [])
  
  return {
    contextMenu,
    handleContextMenu,
    closeContextMenu
  }
}