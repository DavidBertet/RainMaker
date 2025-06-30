// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

const fs = require('fs')
const { execPromise } = require('./system')
const { testESP32Connection } = require('./esp32')
const { Spinner } = require('./spinner')
const { logger, colors } = require('./logger')
const { askQuestion } = require('./prompt')
const { findPIOExecutable } = require('./pio')

async function checkOtaIpFormat(args) {
  if (!args.otaIP) {
    return
  }
  const ipRegex = /^[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}$/

  if (!ipRegex.test(args.otaIP)) {
    logger.warning(`IP address format may be invalid: ${args.otaIP}`)
    logger.info('Expected format: 192.168.1.10')

    const shouldChange = await askQuestion('Continue anyway? (y/N)', args.autoYes)

    if (shouldChange.toLowerCase() !== 'y' && shouldChange.toLowerCase() !== 'yes') {
      logger.info('Installation cancelled by user')
      process.exit(0)
    }
  }
}

function checkProjectStructure() {
  const requiredPaths = ['frontend', 'backend', 'backend/platformio.ini']

  for (const reqPath of requiredPaths) {
    if (!fs.existsSync(reqPath)) {
      logger.error(`Required path not found: ${reqPath}`)
      logger.info("Make sure you're running this script from the project root directory.")
      process.exit(1)
    }
  }

  logger.success('Project structure verified')
}

async function checkPrerequisites(args) {
  logger.step('Checking other prerequisites...')
  logger.separator()

  // Check PlatformIO
  const spinner = new Spinner('Checking PlatformIO...')
  spinner.start()

  const pioCmd = await findPIOExecutable()

  if (pioCmd) {
    try {
      spinner.stop(true, `PlatformIO found in: ${pioCmd}`)
      spinner.start()
      const { stdout } = await execPromise(`"${pioCmd}" --version`)
      spinner.stop(true, `PlatformIO info (${stdout.trim()})`)
    } catch {}
  } else {
    spinner.stop(false, 'PlatformIO not found')
    logger.separator()
    logger.error('PlatformIO is required but not installed.')
    console.log()
    console.log(`${colors.yellow}Install PlatformIO:${colors.reset}`)
    console.log('  Option 1 - Using VSCode: install PlatformIO extension')
    console.log('  Option 2 - Using pip: pip install platformio')
    console.log('  Option 3 - Using brew: brew install platformio')
    console.log('Visit: https://platformio.org/install for details')
    console.log()
    logger.error('Please install PlatformIO and run this script again.')
    process.exit(1)
  }

  // Test OTA connection if IP provided
  if (args.otaIP) {
    await checkOtaIpFormat(args)

    const otaSpinner = new Spinner(`Testing OTA connection to ${args.otaIP}...`)
    otaSpinner.start()

    const result = await testESP32Connection(args.otaIP)
    if (result.connected) {
      otaSpinner.stop(true, `OTA connection to ${args.otaIP} successful`)
    } else {
      otaSpinner.stop(false, `Cannot connect to ${args.otaIP} (${result.error})`)
      logger.error('Make sure the ESP32 is powered on and connected to the network')
      process.exit(0)
    }
  }

  checkProjectStructure()

  logger.success('Prerequisites check completed!')
  console.log()
}

module.exports = { checkPrerequisites }
