// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

const fs = require('fs')
const path = require('path')
const http = require('http')
const crypto = require('crypto')
const { SimpleWebSocket } = require('./ws.js')
const { askQuestion } = require('./prompt')
const { waitForESP32Restart } = require('./esp32')
const { logger } = require('./logger')
const { findBuildFiles } = require('./backend')
const { drawProgressBar } = require('./progressbar')

function createWebSocket(ip, sessionId) {
  return new Promise((resolve) => {
    const ws = new SimpleWebSocket(`ws://${ip}/ws`, sessionId)
    let connected = false

    ws.on('open', () => {
      connected = true
      resolve({ ws, connected: true })
    })

    ws.on('error', (error) => {
      console.log('WebSocket error (continuing without progress):', error.message)
    })

    ws.connect()

    // Timeout for WebSocket connection
    setTimeout(() => {
      if (!connected) {
        console.log('WebSocket connection timeout, continuing without progress tracking...')
        resolve({ ws: null, connected: false })
      }
    }, 3000)
  })
}

function handleWebSocketMessage(data, currentUploadInfo) {
  try {
    const message = JSON.parse(data)
    if (message.type === 'upload_progress' && currentUploadInfo) {
      const loaded = parseInt(message.loaded)
      const total = parseInt(message.total)
      const progress = Math.round((loaded / total) * 100)

      drawProgressBar(
        progress,
        `Uploading ${currentUploadInfo.fileName} (${currentUploadInfo.index}/${currentUploadInfo.total})`
      )
    }
  } catch (error) {
    // Ignore JSON parse errors
  }
}

function uploadSingleFile(ip, filePath, fileName, sessionId, uploadPassword, index, total) {
  return new Promise((resolve, reject) => {
    // Show initial progress
    drawProgressBar(0, `Uploading ${fileName} (${index}/${total})`)

    const postData = fs.readFileSync(filePath)

    const options = {
      hostname: ip,
      port: 80,
      path: `/upload/${fileName}`,
      method: 'POST',
      headers: {
        'Content-Type': 'application/octet-stream',
        'Content-Length': postData.length,
        Cookie: `session_id=${sessionId}`,
        'X-OTA-Password': uploadPassword,
      },
    }

    const req = http.request(options, (res) => {
      let responseData = ''

      res.on('data', (chunk) => {
        responseData += chunk
      })

      res.on('end', () => {
        // Clear progress bar
        process.stdout.write('\r' + ' '.repeat(100) + '\r')

        if (res.statusCode === 200) {
          resolve({ fileName, success: true, response: responseData })
        } else {
          reject(new Error(`Status ${res.statusCode} - ${responseData}`))
        }
      })
    })

    req.on('error', (error) => {
      reject(error)
    })

    req.write(postData)
    req.end()
  })
}

function uploadFilesHTTP(ip, uploadPassword, filePaths) {
  return new Promise(async (resolve, reject) => {
    const sessionId = crypto.randomBytes(8).toString('hex')
    const results = []
    let ws = null
    let currentUploadInfo = null

    try {
      // Initialize WebSocket for progress tracking
      const wsResult = await createWebSocket(ip, sessionId)
      ws = wsResult.ws

      if (ws) {
        ws.on('message', (data) => {
          handleWebSocketMessage(data, currentUploadInfo)
        })
      }

      // Upload files sequentially
      for (let i = 0; i < filePaths.length; i++) {
        const filePath = filePaths[i]
        const fileName = path.basename(filePath)
        currentUploadInfo = { fileName, index: i + 1, total: filePaths.length }

        try {
          const result = await uploadSingleFile(
            ip,
            filePath,
            fileName,
            sessionId,
            uploadPassword,
            i + 1,
            filePaths.length
          )
          logger.success(`Uploaded ${fileName}`)
          results.push(result)
        } catch (error) {
          if (error.message.includes('401')) {
            logger.error(`Upload aborted: Invalid credentials`)
            break
          }
          logger.error(`Failed to upload ${fileName}: ${error.message}`)
          results.push({
            fileName,
            success: false,
            error: error.message,
          })
        }
      }

      resolve(results)
    } catch (error) {
      reject(error)
    } finally {
      // Cleanup
      if (ws) {
        ws.close()
      }
    }
  })
}

module.exports = { uploadFilesHTTP }

// Upload all files from data directory individually
async function uploadDataFiles(ip, uploadPassword, dataDir) {
  const files = []

  // Recursively get all files from data directory
  function getFilesRecursively(dir, relativePath = '') {
    const items = fs.readdirSync(dir)

    for (const item of items) {
      if (item === '.DS_Store') continue

      const fullPath = path.join(dir, item)
      const relativeFilePath = path.join(relativePath, item)

      if (fs.statSync(fullPath).isDirectory()) {
        getFilesRecursively(fullPath, relativeFilePath)
      } else {
        files.push({
          fullPath,
          relativePath: relativeFilePath,
          name: item,
        })
      }
    }
  }

  getFilesRecursively(dataDir)

  if (files.length === 0) {
    logger.info('No files found in data directory')
    return true
  }

  logger.info(`Found ${files.length} files to upload`)

  let uploadStatus = await uploadFilesHTTP(
    ip,
    uploadPassword,
    files.map((file) => file.fullPath)
  )
  let successCount = uploadStatus.filter(({ success }) => success).length
  let failCount = files.length - successCount

  if (failCount > 0) {
    logger.warning(`${failCount} files failed to upload, ${successCount} succeeded`)
    return false
  } else {
    logger.success(`All ${successCount} files uploaded successfully`)
    return true
  }
}

async function uploadBackendOTA(args) {
  let backendUploaded = false

  const shouldUploadFirmware = await askQuestion('Upload firmware via OTA? (Y/n)', args.autoYes)

  if (shouldUploadFirmware.toLowerCase() !== 'n' && shouldUploadFirmware.toLowerCase() !== 'no') {
    const buildFiles = findBuildFiles()

    try {
      let result = await uploadFilesHTTP(args.otaIP, args.uploadPassword, [buildFiles.firmware])
      if (result[0].success == false) {
        logger.error('Firmware OTA upload failed')
        return
      }
      logger.success('Firmware uploaded successfully via OTA')
      backendUploaded = true

      // Wait for ESP32 to restart after firmware upload
      logger.info('ESP32 will now restart with the new firmware...')
      const restartResult = await waitForESP32Restart(args.otaIP)

      if (restartResult.success) {
        logger.success(
          `ESP32 restarted successfully in ${Math.round(restartResult.elapsedTime / 1000)}s`
        )
      } else {
        logger.warning(`ESP32 restart detection failed: ${restartResult.error}`)
        logger.info('The device may still be restarting. Check manually if needed.')
      }
    } catch (error) {
      logger.error('Firmware OTA upload failed')
      logger.error(error.message)
      throw error
    }
  } else {
    logger.warning('Firmware OTA upload skipped by user')
  }

  return backendUploaded
}

async function uploadFrontendOTA(args) {
  let frontendUploaded = false

  const dataDir = path.join(path.resolve('backend'), 'data')
  if (fs.existsSync(dataDir)) {
    const shouldUploadFiles = await askQuestion('Upload web files via OTA? (Y/n)', args.autoYes)

    if (shouldUploadFiles.toLowerCase() !== 'n' && shouldUploadFiles.toLowerCase() !== 'no') {
      logger.info('Uploading individual files from data directory...')

      try {
        const uploadSuccess = await uploadDataFiles(args.otaIP, args.uploadPassword, dataDir)
        frontendUploaded = uploadSuccess

        if (uploadSuccess) {
          logger.info('Files should be immediately available on the web interface')
        } else {
          logger.warning('Some files failed to upload')
        }
      } catch (error) {
        logger.error('Web files upload failed:')
        logger.error(error.message)
        // Don't throw here, web files upload is optional
      }
    } else {
      logger.warning('Web files OTA upload skipped by user')
    }
  } else {
    logger.info('No data directory found - no web files to upload')
  }

  return frontendUploaded
}

module.exports = { uploadBackendOTA, uploadFrontendOTA }
