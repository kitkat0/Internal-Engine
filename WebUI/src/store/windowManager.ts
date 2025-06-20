import { create } from 'zustand'

export type WindowType = 
  | 'PROCESS_INFO' 
  | 'MODULE_LIST' 
  | 'MEMORY_VIEWER' 
  | 'ADDRESS_LIST' 
  | 'SCANNER' 
  | 'DISASSEMBLER' 
  | 'CONSOLE'
  | 'ADD_EDIT_ADDRESS'

export interface WindowInstance {
  id: string
  type: WindowType
  title: string
  x: number
  y: number
  width: number
  height: number
  zIndex: number
  isMinimized: boolean
}

interface WindowManager {
  windows: WindowInstance[]
  activeWindowId: string | null
  
  openWindow: (type: WindowType) => void
  closeWindow: (id: string) => void
  focusWindow: (id: string) => void
  
  updateWindowPosition: (id: string, x: number, y: number) => void
  updateWindowSize: (id: string, width: number, height: number) => void

  toggleMinimize: (id: string) => void
}

const getNextZIndex = (windows: WindowInstance[]) => {
  if (windows.length === 0) return 1
  return Math.max(...windows.map(w => w.zIndex)) + 1
}

const getInitialPosition = (windows: WindowInstance[]) => {
    const offset = (windows.length % 10) * 30;
    return { x: 50 + offset, y: 50 + offset };
}

const getWindowTitle = (type: WindowType): string => {
    switch (type) {
        case 'PROCESS_INFO': return 'Process Info';
        case 'MODULE_LIST': return 'Module List';
        case 'MEMORY_VIEWER': return 'Memory Viewer';
        case 'ADDRESS_LIST': return 'Address List';
        case 'SCANNER': return 'Scanner';
        case 'DISASSEMBLER': return 'Disassembler';
        case 'CONSOLE': return 'Console';
        case 'ADD_EDIT_ADDRESS': return 'Add/Edit Address';
        default: return 'Window';
    }
}

const getWindowInitialSize = (type: WindowType) => {
    switch(type) {
        case 'PROCESS_INFO': return { width: 400, height: 450 };
        case 'MODULE_LIST': return { width: 450, height: 600 };
        case 'MEMORY_VIEWER': return { width: 800, height: 600 };
        case 'ADDRESS_LIST': return { width: 500, height: 600 };
        case 'SCANNER': return { width: 700, height: 600 };
        case 'DISASSEMBLER': return { width: 600, height: 500 };
        case 'CONSOLE': return { width: 800, height: 400 };
        case 'ADD_EDIT_ADDRESS': return { width: 450, height: 500 };
        default: return { width: 500, height: 400 };
    }
}


export const useWindowManagerStore = create<WindowManager>((set, get) => ({
  windows: [],
  activeWindowId: null,

  openWindow: (type: WindowType) => {
    const { windows } = get()
    
    // Check if a window of this type already exists
    const existingWindow = windows.find(w => w.type === type)
    if (existingWindow) {
      // If it exists, just bring it to front
      set(state => ({
        activeWindowId: existingWindow.id,
        windows: state.windows.map(w => 
          w.id === existingWindow.id 
            ? { ...w, zIndex: getNextZIndex(state.windows), isMinimized: false }
            : w
        )
      }))
      return
    }
    
    const { x, y } = getInitialPosition(windows)
    const { width, height } = getWindowInitialSize(type)
    
    const newWindow: WindowInstance = {
      id: `${type}-${Date.now()}`,
      type,
      title: getWindowTitle(type),
      x,
      y,
      width,
      height,
      zIndex: getNextZIndex(windows),
      isMinimized: false,
    }
    
    set(state => ({
      windows: [...state.windows, newWindow],
      activeWindowId: newWindow.id,
    }))
  },
  
  closeWindow: (id: string) => {
    set(state => {
      const newWindows = state.windows.filter(w => w.id !== id);
      let newActiveId = state.activeWindowId;

      if (state.activeWindowId === id) {
        if (newWindows.length > 0) {
          // Find the window with the highest zIndex to make it active
          newActiveId = newWindows.reduce((prev, current) => (prev.zIndex > current.zIndex) ? prev : current).id;
        } else {
          newActiveId = null;
        }
      }
      
      return {
        windows: newWindows,
        activeWindowId: newActiveId,
      };
    });
  },

  focusWindow: (id: string) => {
    set(state => ({
      windows: state.windows.map(w => 
        w.id === id 
          ? { ...w, zIndex: getNextZIndex(state.windows) }
          : w
      ),
      activeWindowId: id,
    }))
  },

  updateWindowPosition: (id: string, x: number, y: number) => {
    set(state => ({
      windows: state.windows.map(w =>
        w.id === id ? { ...w, x, y } : w
      ),
    }))
  },

  updateWindowSize: (id: string, width: number, height: number) => {
    set(state => ({
      windows: state.windows.map(w =>
        w.id === id ? { ...w, width, height } : w
      ),
    }))
  },

  toggleMinimize: (id: string) => {
    set(state => ({
        windows: state.windows.map(w => 
            w.id === id ? { ...w, isMinimized: !w.isMinimized } : w
        ),
        activeWindowId: state.activeWindowId === id && !get().windows.find(w => w.id === id)?.isMinimized
            ? null 
            : state.activeWindowId
    }));
  },
})) 