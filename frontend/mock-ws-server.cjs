// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

const { generateMockResponse } = require('./src/lib/mockdata.js')

// Simple mock WebSocket server for frontend development
const WebSocket = require('ws')
const wss = new WebSocket.Server({ port: 8080 })

wss.on('connection', (ws) => {
  ws.on('message', (msg) => {
    let data
    try {
      data = JSON.parse(msg)
    } catch (e) {
      return
    }
    setTimeout(() => {
      const mockResponse = generateMockResponse(data)

      ws.send(JSON.stringify(mockResponse))
    }, 1000)
  })
})

console.log('Mock WebSocket server running on ws://localhost:8080')
