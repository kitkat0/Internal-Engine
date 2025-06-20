// Ultra-fast binary protocol client for maximum performance
// Eliminates JSON parsing overhead for real-time memory monitoring

export enum BinaryOpcode {
  MEMORY_READ = 0x01,
  MEMORY_WRITE = 0x02,
  MEMORY_SCAN = 0x03,
  VALUE_UPDATE = 0x04,
  BULK_UPDATE = 0x05,
  PROCESS_INFO = 0x06,
  MODULE_LIST = 0x07,
  PING = 0x08,
  PONG = 0x09
}

export enum DataType {
  INT32 = 0x01,
  INT64 = 0x02,
  FLOAT = 0x03,
  DOUBLE = 0x04,
  STRING = 0x05,
  BYTES = 0x06
}

export interface BinaryHeader {
  magic: number // 0x494E544C
  version: number // 0x0001
  opcode: BinaryOpcode
  flags: number
  payloadSize: number
  requestId: number
}

export interface MemoryReadRequest {
  address: bigint
  size: number
  type: DataType
}

export interface MemoryWriteRequest {
  address: bigint
  type: DataType
  dataSize: number
  data: Uint8Array
}

export interface ValueUpdate {
  address: bigint
  type: DataType
  value: Uint8Array
}

export class BinaryProtocolClient {
  private requestIdCounter = 1
  private pendingRequests = new Map<number, (response: any) => void>()
  
  // Encode binary messages
  encodeMemoryRead(address: string, size: number, type: string): ArrayBuffer {
    const requestId = this.requestIdCounter++
    const buffer = new ArrayBuffer(32) // Header + MemoryReadRequest
    const view = new DataView(buffer)
    
    // Header
    view.setUint32(0, 0x494E544C, true) // magic
    view.setUint16(4, 0x0001, true) // version
    view.setUint8(6, BinaryOpcode.MEMORY_READ)
    view.setUint8(7, 0) // flags
    view.setUint32(8, 16, true) // payload size
    view.setUint32(12, requestId, true)
    
    // Payload
    const addr = BigInt(address)
    view.setBigUint64(16, addr, true)
    view.setUint32(24, size, true)
    view.setUint8(28, this.stringToDataType(type))
    
    return buffer
  }
  
  encodeMemoryWrite(address: string, type: string, data: Uint8Array): ArrayBuffer {
    const requestId = this.requestIdCounter++
    const buffer = new ArrayBuffer(32 + data.length) // Header + MemoryWriteRequest + data
    const view = new DataView(buffer)
    
    // Header
    view.setUint32(0, 0x494E544C, true) // magic
    view.setUint16(4, 0x0001, true) // version
    view.setUint8(6, BinaryOpcode.MEMORY_WRITE)
    view.setUint8(7, 0) // flags
    view.setUint32(8, 16 + data.length, true) // payload size
    view.setUint32(12, requestId, true)
    
    // Payload
    const addr = BigInt(address)
    view.setBigUint64(16, addr, true)
    view.setUint8(24, this.stringToDataType(type))
    view.setUint32(25, data.length, true)
    
    // Data
    const uint8View = new Uint8Array(buffer, 29)
    uint8View.set(data)
    
    return buffer
  }
  
  // Decode binary messages
  decodeHeader(buffer: ArrayBuffer): BinaryHeader | null {
    if (buffer.byteLength < 16) return null
    
    const view = new DataView(buffer)
    const magic = view.getUint32(0, true)
    
    if (magic !== 0x494E544C) return null
    
    return {
      magic,
      version: view.getUint16(4, true),
      opcode: view.getUint8(6),
      flags: view.getUint8(7),
      payloadSize: view.getUint32(8, true),
      requestId: view.getUint32(12, true)
    }
  }
  
  decodeValueUpdate(buffer: ArrayBuffer, offset: number = 16): ValueUpdate | null {
    if (buffer.byteLength < offset + 16) return null
    
    const view = new DataView(buffer, offset)
    const address = view.getBigUint64(0, true)
    const type = view.getUint8(8) as DataType
    const valueSize = view.getUint32(9, true)
    
    if (buffer.byteLength < offset + 13 + valueSize) return null
    
    const value = new Uint8Array(buffer, offset + 13, valueSize)
    
    return { address, type, value }
  }
  
  decodeBulkUpdate(buffer: ArrayBuffer, offset: number = 16): ValueUpdate[] {
    const updates: ValueUpdate[] = []
    const view = new DataView(buffer, offset)
    const count = view.getUint32(0, true)
    
    let currentOffset = offset + 4
    for (let i = 0; i < count; i++) {
      const update = this.decodeValueUpdate(buffer, currentOffset)
      if (!update) break
      
      updates.push(update)
      currentOffset += 13 + update.value.length
    }
    
    return updates
  }
  
  // Utility functions
  private stringToDataType(type: string): DataType {
    switch (type) {
      case 'int32': return DataType.INT32
      case 'int64': return DataType.INT64
      case 'float': return DataType.FLOAT
      case 'double': return DataType.DOUBLE
      case 'string': return DataType.STRING
      case 'bytes': return DataType.BYTES
      default: return DataType.BYTES
    }
  }
  
  private dataTypeToString(type: DataType): string {
    switch (type) {
      case DataType.INT32: return 'int32'
      case DataType.INT64: return 'int64'
      case DataType.FLOAT: return 'float'
      case DataType.DOUBLE: return 'double'
      case DataType.STRING: return 'string'
      case DataType.BYTES: return 'bytes'
      default: return 'bytes'
    }
  }
  
  // Convert value bytes to string based on type
  valueToString(value: Uint8Array, type: DataType): string {
    const view = new DataView(value.buffer, value.byteOffset, value.byteLength)
    
    switch (type) {
      case DataType.INT32:
        return view.getInt32(0, true).toString()
      case DataType.INT64:
        return view.getBigInt64(0, true).toString()
      case DataType.FLOAT:
        return view.getFloat32(0, true).toString()
      case DataType.DOUBLE:
        return view.getFloat64(0, true).toString()
      case DataType.STRING:
        return new TextDecoder().decode(value)
      case DataType.BYTES:
        return Array.from(value).map(b => b.toString(16).padStart(2, '0')).join(' ')
      default:
        return ''
    }
  }
  
  // Convert string to value bytes based on type
  stringToValue(str: string, type: DataType): Uint8Array {
    switch (type) {
      case DataType.INT32: {
        const buffer = new ArrayBuffer(4)
        new DataView(buffer).setInt32(0, parseInt(str), true)
        return new Uint8Array(buffer)
      }
      case DataType.INT64: {
        const buffer = new ArrayBuffer(8)
        new DataView(buffer).setBigInt64(0, BigInt(str), true)
        return new Uint8Array(buffer)
      }
      case DataType.FLOAT: {
        const buffer = new ArrayBuffer(4)
        new DataView(buffer).setFloat32(0, parseFloat(str), true)
        return new Uint8Array(buffer)
      }
      case DataType.DOUBLE: {
        const buffer = new ArrayBuffer(8)
        new DataView(buffer).setFloat64(0, parseFloat(str), true)
        return new Uint8Array(buffer)
      }
      case DataType.STRING:
        return new TextEncoder().encode(str)
      case DataType.BYTES:
        return new Uint8Array(str.split(' ').map(b => parseInt(b, 16)))
      default:
        return new Uint8Array()
    }
  }
}

export const binaryProtocol = new BinaryProtocolClient()