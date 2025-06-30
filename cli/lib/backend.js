// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

const fs = require('fs')
const path = require('path')
const { askQuestion } = require('./prompt')
const { logger, colors } = require('./logger')

async function configureBackend(args) {
  logger.step('Configuring backend...')
  logger.separator()

  const platformioPath = path.resolve('backend/platformio.ini')
  let platformioContent = fs.readFileSync(platformioPath, 'utf8')

  // Extract current board
  const boardMatch = platformioContent.match(/board\s*=\s*(.+)/)
  const currentBoard = boardMatch ? boardMatch[1].trim() : 'esp32doit-devkit-v1'

  logger.info(`Current board: ${colors.yellow}${currentBoard}${colors.reset}`)

  const shouldChange = await askQuestion('Do you want to change the board? (y/N)', args.autoYes)

  if (shouldChange.toLowerCase() === 'y' || shouldChange.toLowerCase() === 'yes') {
    console.log()
    logger.info('Common ESP32 boards:')
    console.log('  • esp32doit-devkit-v1 (ESP32 DevKit V1)')
    console.log('  • esp32dev (Generic ESP32)')
    console.log('  • nodemcu-32s (NodeMCU-32S)')
    console.log('  • esp32-s3-devkitc-1 (ESP32-S3 DevKit)')
    console.log('  • esp32-c3-devkitm-1 (ESP32-C3 DevKit)')
    console.log()
    logger.info(
      'Full list: https://docs.platformio.org/en/latest/platforms/espressif32.html#boards'
    )
    console.log()

    const newBoard = await askQuestion('Enter board name:', args.autoYes)

    if (newBoard && newBoard !== currentBoard) {
      platformioContent = platformioContent.replace(/board\s*=\s*.+/, `board = ${newBoard}`)
      fs.writeFileSync(platformioPath, platformioContent)
      logger.success(`Board updated to: ${newBoard}`)
    }
  }

  console.log()
}


function findBuildFiles() {
  const backendPath = path.resolve('backend')

  // Look for firmware file
  const pioenvDirs = fs
    .readdirSync(path.join(backendPath, '.pio', 'build'))
    .filter((dir) => fs.statSync(path.join(backendPath, '.pio', 'build', dir)).isDirectory())

  if (pioenvDirs.length === 0) {
    throw new Error('No build environment found. Run build first.')
  }

  const envDir = pioenvDirs[0] // Use first environment
  const firmwarePath = path.join(backendPath, '.pio', 'build', envDir, 'firmware.bin')

  if (!fs.existsSync(firmwarePath)) {
    throw new Error('Firmware binary not found. Build may have failed.')
  }

  return {
    firmware: firmwarePath,
    envDir,
  }
}

module.exports = { configureBackend, findBuildFiles }
