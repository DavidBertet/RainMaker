// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

const { generateMockResponse } = require('./src/lib/mockdata.js')

window.WebSocket = class MockWebSocket extends EventTarget {
  constructor(url) {
    super()
    this.url = url
    this.readyState = WebSocket.CONNECTING

    // Simulate connection opening
    setTimeout(() => {
      this.readyState = WebSocket.OPEN
      this.dispatchEvent(new Event('open'))
    }, 100)
  }

  send(msg) {
    let data
    try {
      data = JSON.parse(msg)
    } catch (e) {
      return
    }
    const mockResponse = this.generateMockResponse(data)

    setTimeout(() => {
      this.dispatchEvent(new MessageEvent('message', { data: mockResponse }))
    }, 200)
  }

  close() {
    this.readyState = WebSocket.CLOSED
    this.dispatchEvent(new Event('close'))
  }
}
