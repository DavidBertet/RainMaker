// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

const fs = require('fs')
const path = require('path')
const { execPromise } = require('./system')
const { Spinner } = require('./spinner')
const { logger } = require('./logger')

async function buildFrontend() {
  logger.step('Building frontend...')
  logger.separator()

  const frontendPath = path.resolve('frontend')

  // Check if node_modules exists
  if (!fs.existsSync(path.join(frontendPath, 'node_modules'))) {
    const spinner = new Spinner('Installing frontend dependencies...')
    spinner.start()

    try {
      await execPromise('npm install', { cwd: frontendPath })
      spinner.stop(true, 'Frontend dependencies installed')
    } catch (error) {
      spinner.stop(false, 'Failed to install frontend dependencies')
      logger.error(error.stderr || error.stdout || 'Unknown error')
      process.exit(1)
    }
  } else {
    logger.info('Frontend dependencies already installed')
  }

  // Build frontend
  const buildSpinner = new Spinner('Building frontend...')
  buildSpinner.start()

  try {
    await execPromise('npm run build', { cwd: frontendPath })
    buildSpinner.stop(true, 'Frontend built successfully')
  } catch (error) {
    buildSpinner.stop(false, 'Frontend build failed')
    logger.error(error.stderr || error.stdout || 'Unknown error')
    process.exit(1)
  }

  // Copy built files to backend/data
  const copySpinner = new Spinner('Copying files to backend/data...')
  copySpinner.start()

  try {
    // Ensure backend/data directory exists
    const dataDir = path.resolve('backend/data')
    if (!fs.existsSync(dataDir)) {
      fs.mkdirSync(dataDir, { recursive: true })
    }

    // Copy dist folder contents to backend/data
    const distPath = path.join(frontendPath, 'dist')
    if (fs.existsSync(distPath)) {
      await execPromise(`cp -r ${path.join(distPath, '*')} ${dataDir}/`)
      copySpinner.stop(true, 'Files copied to backend/data')
    } else {
      copySpinner.stop(false, 'Frontend dist folder not found')
      process.exit(1)
    }
  } catch (error) {
    copySpinner.stop(false, 'Failed to copy files')
    logger.error(error.stderr || error.stdout || 'Unknown error')
    process.exit(1)
  }

  console.log()
}

module.exports = { buildFrontend }
