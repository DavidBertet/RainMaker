// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

const http = require('http')
const crypto = require('crypto')
const { EventEmitter } = require('events')

// Basic web socket implementation so I don't have to use dependencies
class SimpleWebSocket extends EventEmitter {
  constructor(url, sessionId) {
    super()
    this.url = url
    this.sessionId = sessionId
    this.socket = null
    this.connected = false
    this.closed = false
    this.buffer = Buffer.alloc(0) // Buffer for incomplete frames
  }

  connect() {
    const urlParts = new URL(this.url)
    const key = crypto.randomBytes(16).toString('base64')

    const options = {
      hostname: urlParts.hostname,
      port: urlParts.port || (urlParts.protocol === 'wss:' ? 443 : 80),
      path: urlParts.pathname,
      headers: {
        Upgrade: 'websocket',
        Connection: 'Upgrade',
        'Sec-WebSocket-Key': key,
        'Sec-WebSocket-Version': '13',
        Cookie: `session_id=${this.sessionId}`,
      },
    }

    const req = http.request(options)

    req.on('upgrade', (res, socket, head) => {
      if (this.closed) {
        socket.end()
        return
      }
      this.socket = socket
      this.connected = true
      this.emit('open')

      socket.on('data', (data) => {
        this.handleData(data)
      })

      socket.on('close', () => {
        this.connected = false
        this.emit('close')
      })

      socket.on('error', (error) => {
        this.emit('error', error)
      })
    })

    req.on('error', (error) => {
      this.emit('error', error)
    })

    req.end()
  }

  handleData(newData) {
    // Append new data to existing buffer
    this.buffer = Buffer.concat([this.buffer, newData])

    // Process all complete frames in the buffer
    while (this.buffer.length > 0) {
      const result = this.processFrame(this.buffer)

      if (result.consumed === 0) {
        // No complete frame available, wait for more data
        break
      }

      // Remove processed bytes from buffer
      this.buffer = this.buffer.slice(result.consumed)

      // Emit message if we got one
      if (result.message) {
        this.emit('message', result.message)
      }
    }
  }

  processFrame(buffer) {
    try {
      if (buffer.length < 2) {
        return { consumed: 0, message: null }
      }

      const firstByte = buffer[0]
      const secondByte = buffer[1]

      const fin = (firstByte & 0x80) === 0x80
      const opcode = firstByte & 0x0f
      const masked = (secondByte & 0x80) === 0x80
      let payloadLength = secondByte & 0x7f

      let offset = 2

      // Handle extended payload length
      if (payloadLength === 126) {
        if (buffer.length < offset + 2) {
          return { consumed: 0, message: null }
        }
        payloadLength = buffer.readUInt16BE(offset)
        offset += 2
      } else if (payloadLength === 127) {
        if (buffer.length < offset + 8) {
          return { consumed: 0, message: null }
        }
        payloadLength = Number(buffer.readBigUInt64BE(offset))
        offset += 8
      }

      // Check if we have enough bytes for mask + payload
      const totalNeeded = offset + (masked ? 4 : 0) + payloadLength
      if (buffer.length < totalNeeded) {
        return { consumed: 0, message: null }
      }

      let payload
      if (masked) {
        const maskKey = buffer.slice(offset, offset + 4)
        offset += 4
        payload = buffer.slice(offset, offset + payloadLength)

        // Unmask the payload
        for (let i = 0; i < payload.length; i++) {
          payload[i] ^= maskKey[i % 4]
        }
      } else {
        payload = buffer.slice(offset, offset + payloadLength)
      }

      // Handle different frame types
      switch (opcode) {
        case 0x01:
          // Text frame
          if (fin && payload.length > 0) {
            const message = payload.toString('utf8')
            if (message.trim()) {
              return { consumed: totalNeeded, message: message }
            }
          }
          break
        case 0x08:
          // Close frame
          this.emit('close')
          return { consumed: totalNeeded, message: null }
        case 0x09:
          // Ping frame
          this.sendPong(payload)
          return { consumed: totalNeeded, message: null }
        case 0x0a:
          // Pong frame, not managed
          return { consumed: totalNeeded, message: null }
      }

      return { consumed: totalNeeded, message: null }
    } catch (error) {
      return { consumed: buffer.length, message: null } // Skip malformed frame
    }
  }

  sendPong(payload) {
    if (!this.connected || !this.socket) return

    // Create pong frame
    const frame = Buffer.alloc(2 + payload.length)
    frame[0] = 0x8a // FIN + Pong opcode
    frame[1] = payload.length
    if (payload.length > 0) {
      payload.copy(frame, 2)
    }

    this.socket.write(frame)
  }

  send(message) {
    if (!this.connected || !this.socket) return

    const payload = Buffer.from(message, 'utf8')
    const frame = Buffer.alloc(2 + payload.length)

    frame[0] = 0x81 // FIN + Text opcode
    frame[1] = payload.length
    payload.copy(frame, 2)

    this.socket.write(frame)
  }

  close() {
    this.closed = true
    if (this.socket) {
      // Send close frame
      const closeFrame = Buffer.from([0x88, 0x00])
      this.socket.write(closeFrame)
      this.socket.end()
    }
  }
}

module.exports = { SimpleWebSocket }
