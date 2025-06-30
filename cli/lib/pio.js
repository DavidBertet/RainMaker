const path = require('path')
const fs = require('fs')
const os = require('os')
const { commandExists, execPromise, execWithOutput } = require('./system')
const { askQuestion } = require('./prompt')
const { Spinner } = require('./spinner')
const { logger, colors } = require('./logger')
const { drawProgressBar } = require('./progressbar')

/**
 * Find PlatformIO executable, checking VS Code extension if not in PATH
 */
async function findPIOExecutable() {
  const pioExists = await commandExists('pio')
  if (pioExists) {
    return 'pio'
  }

  // Check for VS Code PlatformIO extension
  const vscodeExtensionPaths = getVSCodeExtensionPaths()

  for (const extensionPath of vscodeExtensionPaths) {
    const pioPath = findPIOInExtension(extensionPath)
    if (pioPath) {
      return pioPath
    }
  }

  throw new Error('PlatformIO not found in PATH or VS Code extension')
}

/**
 * Get possible VS Code extension paths based on OS
 */
function getVSCodeExtensionPaths() {
  const homeDir = os.homedir()
  const platform = os.platform()
  const paths = []

  if (platform === 'win32') {
    // Windows paths
    paths.push(path.join(homeDir, '.vscode', 'extensions'))
    paths.push(path.join(homeDir, 'AppData', 'Roaming', 'Code', 'User', 'extensions'))
    paths.push(path.join(homeDir, 'AppData', 'Roaming', 'Code - Insiders', 'User', 'extensions'))
  } else if (platform === 'darwin') {
    // macOS paths
    paths.push(path.join(homeDir, '.vscode', 'extensions'))
    paths.push(path.join(homeDir, 'Library', 'Application Support', 'Code', 'User', 'extensions'))
    paths.push(
      path.join(homeDir, 'Library', 'Application Support', 'Code - Insiders', 'User', 'extensions')
    )
  } else {
    // Linux paths
    paths.push(path.join(homeDir, '.vscode', 'extensions'))
    paths.push(path.join(homeDir, '.config', 'Code', 'User', 'extensions'))
    paths.push(path.join(homeDir, '.config', 'Code - Insiders', 'User', 'extensions'))
  }

  return paths.filter((p) => fs.existsSync(p))
}

/**
 * Find PlatformIO executable in VS Code extension directory
 */
function findPIOInExtension(extensionPath) {
  try {
    const extensionDirs = fs.readdirSync(extensionPath)
    const pioExtension = extensionDirs.find((dir) => dir.startsWith('platformio.platformio-ide-'))

    if (!pioExtension) {
      return null
    }

    const pioExtensionPath = path.join(extensionPath, pioExtension)
    const platform = os.platform()

    // Look for PlatformIO Core in the extension
    const possiblePaths = []

    if (platform === 'win32') {
      possiblePaths.push(
        path.join(pioExtensionPath, 'piocore', 'penv', 'Scripts', 'pio.exe'),
        path.join(pioExtensionPath, 'piocore', 'penv', 'Scripts', 'platformio.exe')
      )
    } else {
      possiblePaths.push(
        path.join(pioExtensionPath, 'piocore', 'penv', 'bin', 'pio'),
        path.join(pioExtensionPath, 'piocore', 'penv', 'bin', 'platformio')
      )
    }

    for (const pioPath of possiblePaths) {
      if (fs.existsSync(pioPath)) {
        return pioPath
      }
    }

    // Also check for global PlatformIO installation managed by the extension
    const globalPioPath = findGlobalPIOFromExtension()
    if (globalPioPath) {
      return globalPioPath
    }

    return null
  } catch (error) {
    return null
  }
}

/**
 * Find global PlatformIO installation that might be managed by VS Code extension
 */
function findGlobalPIOFromExtension() {
  const homeDir = os.homedir()
  const platform = os.platform()
  const possiblePaths = []

  if (platform === 'win32') {
    possiblePaths.push(
      path.join(homeDir, '.platformio', 'penv', 'Scripts', 'pio.exe'),
      path.join(homeDir, '.platformio', 'penv', 'Scripts', 'platformio.exe')
    )
  } else {
    possiblePaths.push(
      path.join(homeDir, '.platformio', 'penv', 'bin', 'pio'),
      path.join(homeDir, '.platformio', 'penv', 'bin', 'platformio')
    )
  }

  for (const pioPath of possiblePaths) {
    if (fs.existsSync(pioPath)) {
      return pioPath
    }
  }

  return null
}

function managePIONewLine(line, spinner) {
  // Look for serial port information
  const portMatch = line.match(/Serial port ([^\r\n]+)/)
  if (portMatch) {
    if (spinner) spinner.stop(false)
    logger.info(`Using serial port: ${colors.yellow}${portMatch[1]}${colors.reset}`)
    if (spinner) spinner.start()
  }

  // Handle writing progress with progress bar
  const progressMatch = line.match(/Writing at 0x[0-9a-fA-F]+\.\.\.\s*\((\d+)\s*%\)/)
  if (progressMatch) {
    const progress = parseInt(progressMatch[1])
    if (spinner) spinner.stop(false)
    drawProgressBar(progress, 'Writing')
    if (progress === 100) {
      // Clear the progress bar line and move cursor to beginning
      process.stdout.write('\r' + ' '.repeat(50) + '\r')
      if (spinner) spinner.start()
    }
    return
  }

  // Show other relevant output (process each line individually)
  if (line.includes('Connecting')) {
    if (spinner) spinner.stop(false)
    logger.info('Connected')
    if (spinner) spinner.start()
  }
}

async function uploadBackendPIO(args) {
  let backendUploaded = false

  const shouldUpload = await askQuestion('Upload firmware now? (Y/n)', args.autoYes)

  if (shouldUpload.toLowerCase() === 'n' || shouldUpload.toLowerCase() === 'no') {
    logger.warning('Firmware upload skipped by user')
    logger.info('You can upload later with: pio run -t upload')
    backendUploaded = false
  } else {
    const uploadSpinner = new Spinner('Uploading firmware...')
    uploadSpinner.start()
    const backendPath = path.resolve('backend')

    try {
      const pioCmd = await findPIOExecutable()
      await execWithOutput(`"${pioCmd}" run -t upload`, { cwd: backendPath }, (line) =>
        managePIONewLine(line, uploadSpinner)
      )
      uploadSpinner.stop(true, 'Firmware uploaded successfully')
      backendUploaded = true

      // For serial upload, we can't easily detect restart, so just inform the user
      logger.info('ESP32 should restart automatically with the new firmware')
    } catch (error) {
      uploadSpinner.stop(false, 'Firmware upload failed')
      throw error
    }
  }

  return backendUploaded
}

async function uploadFrontendPIO(args) {
  let frontendUploaded = false

  const shouldUploadFS = await askQuestion('Upload frontend (web files)? (Y/n)', args.autoYes)

  if (shouldUploadFS.toLowerCase() === 'n' || shouldUploadFS.toLowerCase() === 'no') {
    logger.warning('Frontend upload skipped by user')
    logger.info('You can upload later with: pio run -t uploadfs')
    frontendUploaded = false
  } else {
    const frontendSpinner = new Spinner('Uploading frontend...')
    frontendSpinner.start()
    const backendPath = path.resolve('backend')

    try {
      const pioCmd = await findPIOExecutable()
      await execWithOutput(`"${pioCmd}" run -t uploadfs`, { cwd: backendPath }, (line) =>
        managePIONewLine(line, frontendSpinner)
      ),
        frontendSpinner.stop(true, 'Frontend uploaded successfully')
      frontendUploaded = true
    } catch (error) {
      frontendSpinner.stop(false, 'Frontend upload failed')
      throw error
    }
  }

  return frontendUploaded
}

async function buildBackendPIO() {
  logger.step('Building backend...')
  logger.separator()

  const backendPath = path.resolve('backend')

  // Build firmware
  const buildSpinner = new Spinner('Building firmware...')
  buildSpinner.start()

  try {
    const pioCmd = await findPIOExecutable()
    await execPromise(`"${pioCmd}" run`, { cwd: backendPath })
    buildSpinner.stop(true, 'Firmware built successfully\n')
  } catch (error) {
    buildSpinner.stop(false, 'Firmware build failed')
    logger.error('Build error:')
    console.log(error.stderr || error.stdout || 'Unknown error')
    process.exit(1)
  }
}

module.exports = { findPIOExecutable, uploadBackendPIO, uploadFrontendPIO, buildBackendPIO }
