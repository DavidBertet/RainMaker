// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

if (import.meta.env.MODE === 'github') {
  // Import mock only in github build
  require('./mockws.js')
}

// Store for connection status
export const wsState = $state({ isConnected: false, pingPaused: false })

let ws = null
let url = ''
let messageQueue = []
let reconnectTimeout = null
let pingTimeout = null
let pongTimeout = null

// Configuration
const PING_INTERVAL = 10000 // Wait 10 seconds after pong before next ping
const PONG_TIMEOUT = 6000 // Wait 6 seconds for pong response
const RECONNECT_DELAY = 3000 // Wait 3 seconds before reconnecting

// Event-based subscriptions
const listeners = {}

export function onMessageType(type, callback) {
  if (!listeners[type]) listeners[type] = []
  listeners[type].push(callback)
  // Return unsubscribe function
  return () => {
    listeners[type] = listeners[type].filter((cb) => cb !== callback)
  }
}

function schedulePing() {
  // Clear any existing ping timeout
  if (pingTimeout) clearTimeout(pingTimeout)

  pingTimeout = setTimeout(() => {
    if (ws && ws.readyState === WebSocket.OPEN) {
      if (wsState.pingPaused) {
        schedulePing()
        return
      }
      // Send ping message
      sendMessage({ type: 'ping', timestamp: Date.now() })

      // Set timeout for pong response
      if (pongTimeout) clearTimeout(pongTimeout)
      pongTimeout = setTimeout(() => {
        console.warn('WebSocket: No pong received, connection may be dead')
        wsState.isConnected = false
        // Force close and reconnect
        if (ws) {
          ws.close()
        }
      }, PONG_TIMEOUT)
    }
  }, PING_INTERVAL)
}

function stopPingPong() {
  if (pingTimeout) {
    clearTimeout(pingTimeout)
    pingTimeout = null
  }
  if (pongTimeout) {
    clearTimeout(pongTimeout)
    pongTimeout = null
  }
}

function handlePong(data) {
  // Clear the pong timeout since we received a response
  if (pongTimeout) {
    clearTimeout(pongTimeout)
    pongTimeout = null
  }

  // Calculate round-trip time if timestamp is included
  if (data.timestamp) {
    const rtt = Date.now() - data.timestamp
    console.log(`WebSocket RTT: ${rtt}ms`)
  }

  schedulePing()
}

function attemptReconnect() {
  if (reconnectTimeout) return // Already attempting reconnect

  console.log('WebSocket: Attempting to reconnect...')
  reconnectTimeout = setTimeout(() => {
    reconnectTimeout = null
    connectWebSocket(url)
  }, RECONNECT_DELAY)
}

export function connectWebSocket(wsUrl) {
  url = wsUrl

  // Clear any pending reconnect
  if (reconnectTimeout) {
    clearTimeout(reconnectTimeout)
    reconnectTimeout = null
  }

  // Close existing connection
  if (ws) {
    stopPingPong()
    ws.close()
  }

  console.log('WebSocket: Connecting to', url)
  ws = new WebSocket(url)

  ws.onopen = () => {
    console.log('WebSocket: Connected')
    wsState.isConnected = true

    // Send all queued messages
    while (messageQueue.length > 0) {
      ws.send(JSON.stringify(messageQueue.shift()))
    }

    // Start ping-pong mechanism by scheduling the first ping
    schedulePing()
  }

  ws.onmessage = (event) => {
    try {
      const data = JSON.parse(event.data)

      // Handle ping-pong messages
      if (data.type === 'pong') {
        handlePong(data)
        return // Don't add pong messages to the general message store
      }

      // Notify type-specific listeners
      if (data.type && listeners[data.type]) {
        listeners[data.type].forEach((cb) => cb(data))
      }
    } catch (e) {
      console.error('WebSocket: Failed to parse message', e)
    }
  }

  ws.onerror = (err) => {
    console.error('WebSocket: Error', err)
  }

  ws.onclose = (event) => {
    console.log('WebSocket: Disconnected', event.code, event.reason)
    wsState.isConnected = false
    stopPingPong()

    // Attempt reconnect unless it was a clean close
    if (event.code !== 1000) {
      attemptReconnect()
    }
  }
}

export function sendMessage(msg) {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify(msg))
  } else {
    // Queue message for when connection is restored
    messageQueue.push(msg)
  }
}

export function closeWebSocket() {
  console.log('WebSocket: Closing connection')

  // Clear reconnect attempts
  if (reconnectTimeout) {
    clearTimeout(reconnectTimeout)
    reconnectTimeout = null
  }

  stopPingPong()

  if (ws) {
    ws.close(1000, 'Client requested disconnect') // Clean close
    ws = null
  }

  messageQueue = []
  wsState.isConnected = false
}
