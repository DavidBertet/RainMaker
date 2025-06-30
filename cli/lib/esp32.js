// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

const http = require('http')
const { Spinner } = require('./spinner')

async function testESP32Connection(ip, timeout = 5000) {
  return new Promise((resolve) => {
    const req = http.request(
      {
        hostname: ip,
        port: 80,
        path: '/',
        method: 'GET',
        timeout: timeout,
      },
      (res) => {
        resolve({ connected: true, statusCode: res.statusCode })
      }
    )

    req.on('error', (error) => {
      resolve({ connected: false, error: error.message })
    })

    req.on('timeout', () => {
      req.destroy()
      resolve({ connected: false, error: 'Connection timeout' })
    })

    req.end()
  })
}

async function waitForESP32Restart(ip, maxWaitTime = 60000, checkInterval = 2000) {
  const startTime = Date.now()
  let attempts = 0
  const maxAttempts = Math.ceil(maxWaitTime / checkInterval)

  const spinner = new Spinner(`Waiting for ESP32 at ${ip} to restart...`)
  spinner.start()

  // First, wait for the device to go offline (if it was online)
  let wasOnline = false
  const initialCheck = await testESP32Connection(ip, 3000)
  if (initialCheck.connected) {
    wasOnline = true
    spinner.updateMessage(`ESP32 at ${ip} is rebooting...`)

    // Wait for it to go offline
    while (wasOnline && attempts < 10) {
      await new Promise((resolve) => setTimeout(resolve, 1000))
      const check = await testESP32Connection(ip, 2000)
      if (!check.connected) {
        wasOnline = false
        spinner.updateMessage(`ESP32 at ${ip} is offline, waiting for restart...`)
      }
      attempts++
    }
  }

  // Reset attempts counter for the restart detection
  attempts = 0

  // Now wait for it to come back online
  while (attempts < maxAttempts) {
    const elapsed = Date.now() - startTime
    const remainingTime = Math.max(0, maxWaitTime - elapsed)

    if (remainingTime <= 0) {
      spinner.stop(false, `Timeout waiting for ESP32 at ${ip} to restart`)
      return { success: false, error: 'Timeout' }
    }

    const result = await testESP32Connection(ip, 3000)

    if (result.connected) {
      spinner.stop(true, `ESP32 at ${ip} is back online!`)
      return { success: true, attempts: attempts + 1, elapsedTime: elapsed }
    }

    attempts++
    const remainingSeconds = Math.ceil(remainingTime / 1000)
    spinner.updateMessage(
      `Waiting for ESP32 at ${ip} to restart (${remainingSeconds}s remaining)...`
    )

    // Wait before next attempt
    await new Promise((resolve) => setTimeout(resolve, checkInterval))
  }

  spinner.stop(false, `ESP32 at ${ip} did not come back online`)
  return { success: false, error: 'Device did not restart', attempts }
}

module.exports = { testESP32Connection, waitForESP32Restart }
