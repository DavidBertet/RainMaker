#!/usr/bin/env node

// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

const { parseArgs } = require('./lib/args')
const { logger, colors } = require('./lib/logger')
const { checkPrerequisites } = require('./lib/prerequisites')
const { buildFrontend } = require('./lib/frontend')
const { buildBackendPIO } = require('./lib/pio')
const { configureBackend } = require('./lib/backend')
const { uploadToDevice } = require('./lib/upload')

async function main() {
  try {
    const args = parseArgs()

    await checkPrerequisites(args)
    if (!args.frontendOnly) {
      await configureBackend(args)
      await buildBackendPIO()
    }
    if (!args.backendOnly) {
      await buildFrontend()
    }
    const finalResults = await uploadToDevice(args)

    showCompletionMessage(args, finalResults)
  } catch (error) {
    logger.error('An unexpected error occurred:')
    console.error(error)
    process.exit(1)
  }
}

function showCompletionMessage(args, results) {
  logger.separator()

  if (results.backendUploaded && results.frontendUploaded) {
    if (args.otaIP) {
      logger.success('🎉 OTA deployment completed successfully!')
      logger.info(
        `Check your ESP32 at ${colors.yellow}http://${args.otaIP}${colors.reset} to verify the update.`
      )
      logger.info("You can now access the web interface via the ESP32's IP address.")
    } else {
      logger.success('🎉 Setup and deployment completed successfully!')
      logger.info('Your ESP32 sprinkler controller is now running.')
      logger.info('Check your wifi for "RainMaker" if you haven\'t set it up yet.')
    }
  } else if (results.backendUploaded || results.frontendUploaded) {
    logger.success('🔧 Setup completed with partial deployment')
    if (results.backendUploaded) {
      logger.info('✓ Firmware uploaded - device should be running')
      if (!results.frontendUploaded && !args.backendOnly) {
        logger.warning(
          args.otaIP
            ? '⚠ Filesystem not uploaded via OTA'
            : '⚠ Web interface files not uploaded - use: pio run -t uploadfs'
        )
      }
    }
    if (results.frontendUploaded) {
      logger.info('✓ Web files uploaded')
      if (!results.backendUploaded && !args.frontendOnly) {
        logger.warning(
          args.otaIP
            ? '⚠ Firmware not uploaded via OTA'
            : '⚠ Firmware not uploaded - use: pio run -t upload'
        )
      }
    }
  } else {
    logger.success('🔨 Build completed successfully!')
    let webfilePrepared = !args.backendOnly ? 'Web files prepared' : ''
    let firmwareBuilt = !args.frontendOnly ? 'Firmware built' : ''
    logger.info([firmwareBuilt, webfilePrepared].filter(Boolean).join(' and '))
    if (args.otaIP) {
      logger.info('To deploy via OTA, run this command again and choose to upload.')
    } else {
      logger.info('To deploy to ESP32:')
      logger.info('  • Upload firmware: pio run -t upload')
      logger.info('  • Upload web files: pio run -t uploadfs')
      logger.info('  • Or use OTA: ./setup.sh --ota <ESP32_IP>')
    }
  }
  console.log()
}

// Run the main function
if (require.main === module) {
  main()
}

module.exports = { main }
