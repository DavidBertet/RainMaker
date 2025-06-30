// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

const { logger } = require('./logger')
const { uploadBackendPIO, uploadFrontendPIO } = require('./pio')
const { uploadBackendOTA, uploadFrontendOTA } = require('./ota')

// Build and upload backend
async function uploadToDevice(args) {
  let backendUploaded = false
  let frontendUploaded = false

  // Choose upload method
  if (args.otaIP) {
    logger.step(`Uploading via OTA to ${args.otaIP}...`)
    logger.separator()
    try {
      if (!args.frontendOnly) {
        backendUploaded = await uploadBackendOTA(args)
      }
      if (!args.backendOnly) {
        frontendUploaded = await uploadFrontendOTA(args)
      }
    } catch (error) {
      logger.error('OTA upload failed:')
      logger.error(error.message)
      logger.info('You can try serial upload instead by running without --ota')
      process.exit(1)
    }
  } else {
    logger.step(`Uploading via Serial...`)
    logger.separator()
    logger.info('Make sure your ESP32 is connected via USB and drivers are installed.')

    try {
      if (!args.frontendOnly) {
        backendUploaded = await uploadBackendPIO(args)
      }
      if (!args.backendOnly) {
        frontendUploaded = await uploadFrontendPIO(args)
      }
    } catch (error) {
      logger.warning('Common issues:')
      logger.warning('• ESP32 not connected or wrong USB port')
      logger.warning('• ESP32 not in download mode (try holding BOOT button)')
      logger.warning('• USB drivers not installed')
      logger.warning('• Another program using the serial port')
      process.exit(1)
    }
  }

  console.log()
  return { backendUploaded, frontendUploaded }
}

module.exports = { uploadToDevice }
