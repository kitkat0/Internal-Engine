import React, { useState, useEffect } from 'react'
import { FiPlus, FiTrash2, FiTarget, FiType, FiTag } from 'react-icons/fi'
import { useEngineStore, Address } from '../store/engineStore'

interface AddEditAddressModalProps {
  addressToEdit?: Address
  onClose: () => void
}

export default function AddEditAddressModal({ addressToEdit, onClose }: AddEditAddressModalProps) {
  const [address, setAddress] = useState('')
  const [description, setDescription] = useState('')
  const [type, setType] = useState('int32')
  const [isPointer, setIsPointer] = useState(false)
  const [offsets, setOffsets] = useState<string[]>(['0'])

  const { addAddress, updateAddressValue, sendCommand } = useEngineStore()

  useEffect(() => {
    if (addressToEdit) {
      // Logic to parse pointer address and offsets would go here
      setAddress(addressToEdit.address)
      setDescription(addressToEdit.description)
      setType(addressToEdit.type)
      // setIsPointer(...)
      // setOffsets(...)
    }
  }, [addressToEdit])
  
  const handleAddOffset = () => {
    setOffsets([...offsets, '0'])
  }
  
  const handleRemoveOffset = (index: number) => {
    if (offsets.length > 1) {
      setOffsets(offsets.filter((_, i) => i !== index))
    }
  }

  const handleOffsetChange = (index: number, value: string) => {
    const newOffsets = [...offsets]
    newOffsets[index] = value
    setOffsets(newOffsets)
  }

  const handleSubmit = () => {
    let finalAddress = address
    if (isPointer) {
      finalAddress = `P->${address}`
      if(offsets.length > 0) {
        finalAddress += offsets.map(o => `,${o}`).join('')
      }
    }

    if (addressToEdit) {
      // updateAddress logic
    } else {
      addAddress({
        address: finalAddress,
        value: '???',
        type,
        description,
        frozen: false
      })
    }
    onClose()
  }

  return (
    <div className="bg-gray-800 text-white rounded-lg shadow-xl p-6 w-[450px]">
      <h2 className="text-xl font-bold mb-6">
        {addressToEdit ? 'Edit Address' : 'Add Address Manually'}
      </h2>

      <div className="space-y-4">
        {/* Description */}
        <div className="space-y-2">
          <label className="text-sm font-medium flex items-center gap-2"><FiTag /> Description</label>
          <input type="text" value={description} onChange={e => setDescription(e.target.value)} className="w-full" placeholder="No description" />
        </div>

        {/* Address */}
        <div className="space-y-2">
          <label className="text-sm font-medium flex items-center gap-2"><FiTarget /> Address</label>
          <input type="text" value={address} onChange={e => setAddress(e.target.value)} className="w-full mono" placeholder="Enter address or module+offset" />
        </div>

        {/* Type */}
        <div className="space-y-2">
          <label className="text-sm font-medium flex items-center gap-2"><FiType /> Type</label>
          <select value={type} onChange={e => setType(e.target.value)} className="w-full">
            <option value="int32">4 Bytes</option>
            <option value="int64">8 Bytes</option>
            <option value="float">Float</option>
            <option value="double">Double</option>
            <option value="string">String</option>
            <option value="bytes">Array of Bytes</option>
          </select>
        </div>

        {/* Pointer */}
        <label className="flex items-center gap-2 cursor-pointer">
          <input type="checkbox" checked={isPointer} onChange={e => setIsPointer(e.target.checked)} />
          <span>Pointer</span>
        </label>

        {isPointer && (
          <div className="p-4 bg-gray-700/50 rounded-lg space-y-3">
            {offsets.map((offset, index) => (
              <div key={index} className="flex items-center gap-2">
                <input 
                  type="text"
                  value={offset}
                  onChange={e => handleOffsetChange(index, e.target.value)}
                  className="w-full mono"
                  placeholder="Offset (hex)"
                />
                <span className="text-gray-400">{'-> ???'}</span>
                <button onClick={() => handleRemoveOffset(index)} className="p-1 text-red-400 hover:bg-red-500/20 rounded">
                  <FiTrash2 />
                </button>
              </div>
            ))}
            <button onClick={handleAddOffset} className="text-sm text-blue-400 hover:underline">
              Add offset
            </button>
          </div>
        )}
      </div>

      {/* Action Buttons */}
      <div className="flex justify-end gap-4 mt-8">
        <button onClick={onClose} className="px-4 py-2 bg-gray-700 hover:bg-gray-600 rounded-lg">Cancel</button>
        <button onClick={handleSubmit} className="px-4 py-2 bg-blue-600 hover:bg-blue-700 rounded-lg">OK</button>
      </div>
    </div>
  )
} 