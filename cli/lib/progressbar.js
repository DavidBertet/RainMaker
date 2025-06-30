// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

function drawProgressBar(percentage, msg) {
  const width = 40
  const filled = Math.round((percentage / 100) * width)
  const empty = width - filled
  const bar = '█'.repeat(filled) + '░'.repeat(empty)
  const message = msg ? `${msg}: ` : ''
  process.stdout.write(`\r${message}[${bar}] ${percentage}%`)
}

module.exports = { drawProgressBar }
