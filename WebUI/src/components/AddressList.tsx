import React, { useState } from 'react'
import { FiEdit, FiTrash2, FiLock, FiUnlock, FiPlus, FiCopy, FiCornerDownRight } from 'react-icons/fi'
import { useEngineStore, Address } from '../store/engineStore'
import { useWindowManagerStore } from '../store/windowManager'
import AddEditAddressModal from './AddEditAddressModal'
import ContextMenu, { useContextMenu, ContextMenuItem } from './ContextMenu'

export default function AddressList() {
    const { addresses, removeAddress, toggleFreeze, updateAddressValue, writeMemory } = useEngineStore()
    const { openWindow } = useWindowManagerStore()
    const [editingAddress, setEditingAddress] = useState<Address | null>(null)
    const [editingValue, setEditingValue] = useState('')
    const { contextMenu, handleContextMenu, closeContextMenu } = useContextMenu()

    const handleDoubleClick = (address: Address) => {
        setEditingAddress(address)
        setEditingValue(address.value)
    }
    
    const handleValueChange = (e: React.ChangeEvent<HTMLInputElement>) => {
        setEditingValue(e.target.value)
    }

    const handleValueUpdate = async (address: Address) => {
        if (editingValue !== address.value) {
            const success = await writeMemory(address.address, editingValue, address.type);
            if (success) {
                updateAddressValue(address.id, editingValue); // Update UI only if write was successful
            }
        }
        setEditingAddress(null);
    };

    const handleKeyPress = (e: React.KeyboardEvent<HTMLInputElement>, address: Address) => {
        if (e.key === 'Enter') {
            handleValueUpdate(address)
        } else if (e.key === 'Escape') {
            setEditingAddress(null)
        }
    }
    
    // Build context menu items
    const getContextMenuItems = (address: Address): ContextMenuItem[] => [
        {
            label: address.frozen ? 'Unfreeze value' : 'Freeze value',
            icon: address.frozen ? FiUnlock : FiLock,
            onClick: () => toggleFreeze(address.id)
        },
        { divider: true },
        {
            label: 'Copy address',
            icon: FiCopy,
            onClick: () => navigator.clipboard.writeText(address.address)
        },
        {
            label: 'Copy value',
            icon: FiCopy,
            onClick: () => navigator.clipboard.writeText(address.value)
        },
        {
            label: 'Copy description',
            icon: FiCopy,
            onClick: () => navigator.clipboard.writeText(address.description)
        },
        { divider: true },
        {
            label: 'Browse this memory region',
            icon: FiCornerDownRight,
            onClick: () => openWindow('MEMORY_VIEWER')
        },
        { divider: true },
        {
            label: 'Delete',
            icon: FiTrash2,
            onClick: () => removeAddress(address.id),
            className: 'text-red-400'
        }
    ]

    return (
        <div className="h-full flex flex-col bg-gray-800 text-white">
            <div className="p-4 border-b border-gray-700">
                <button 
                    onClick={() => openWindow('ADD_EDIT_ADDRESS')}
                    className="w-full flex items-center justify-center gap-2 py-2 bg-green-600 hover:bg-green-700 rounded-lg font-medium transition-colors"
                >
                    <FiPlus />
                    Add Address Manually
                </button>
            </div>
            <div className="flex-1 overflow-auto">
                <table className="w-full text-sm">
                    <thead>
                        <tr className="bg-gray-900 sticky top-0">
                            <th className="px-4 py-2 text-left font-semibold w-12">Freeze</th>
                            <th className="px-4 py-2 text-left font-semibold">Description</th>
                            <th className="px-4 py-2 text-left font-semibold">Address</th>
                            <th className="px-4 py-2 text-left font-semibold">Type</th>
                            <th className="px-4 py-2 text-left font-semibold">Value</th>
                            <th className="px-4 py-2 text-left font-semibold w-24">Actions</th>
                        </tr>
                    </thead>
                    <tbody>
                        {addresses.map((addr) => (
                            <tr 
                                key={addr.id} 
                                className="border-b border-gray-700 hover:bg-gray-700/50"
                                onContextMenu={(e) => handleContextMenu(e, addr)}
                            >
                                <td className="px-4 py-2 text-center">
                                    <button onClick={() => toggleFreeze(addr.id)} className="p-1 hover:text-blue-400">
                                        {addr.frozen ? <FiLock /> : <FiUnlock />}
                                    </button>
                                </td>
                                <td className="px-4 py-2">{addr.description}</td>
                                <td className="px-4 py-2 mono text-yellow-400">{addr.address}</td>
                                <td className="px-4 py-2">{addr.type}</td>
                                <td 
                                    className="px-4 py-2 mono text-green-400 cursor-pointer"
                                    onDoubleClick={() => handleDoubleClick(addr)}
                                >
                                    {editingAddress?.id === addr.id ? (
                                        <input 
                                            type="text"
                                            value={editingValue}
                                            onChange={handleValueChange}
                                            onBlur={() => handleValueUpdate(addr)}
                                            onKeyDown={(e) => handleKeyPress(e, addr)}
                                            autoFocus
                                            className="bg-gray-900 text-white w-full"
                                        />
                                    ) : (
                                        addr.value
                                    )}
                                </td>
                                <td className="px-4 py-2">
                                    <div className="flex items-center gap-2">
                                        <button className="p-1 hover:text-green-400"><FiEdit /></button>
                                        <button onClick={() => removeAddress(addr.id)} className="p-1 hover:text-red-400"><FiTrash2 /></button>
                                    </div>
                                </td>
                            </tr>
                        ))}
                    </tbody>
                </table>
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