// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

const { logger, colors } = require('./logger')

class Spinner {
  constructor(message) {
    this.message = message
    this.frames = ['⠋', '⠙', '⠹', '⠸', '⠼', '⠴', '⠦', '⠧', '⠇', '⠏']
    this.current = 0
    this.interval = null
  }

  start() {
    process.stdout.write(`${colors.yellow}${this.frames[0]} ${this.message}${colors.reset}`)
    this.interval = setInterval(() => {
      process.stdout.write('\r' + ' '.repeat(process.stdout.columns || 80) + '\r')
      this.current = (this.current + 1) % this.frames.length
      process.stdout.write(
        `${colors.yellow}${this.frames[this.current]} ${this.message}${colors.reset}`
      )
    }, 100)
  }

  stop(success = true, message = null) {
    if (this.interval) {
      clearInterval(this.interval)
      this.interval = null
    }
    process.stdout.write('\r' + ' '.repeat(process.stdout.columns || 80) + '\r')
    if (message) {
      if (success) {
        logger.success(message)
      } else {
        logger.error(message)
      }
    }
  }

  updateMessage(message) {
    this.message = message
  }
}

module.exports = { Spinner }
