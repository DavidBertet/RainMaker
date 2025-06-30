// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

const readline = require('readline')
const { colors } = require('./logger')

function askQuestion(question, autoYes = false) {
  const rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout,
  })

  if (autoYes) {
    return new Promise((resolve) => {
      rl.close()
      resolve('')
    })
  }

  return new Promise((resolve) => {
    rl.question(`${colors.cyan}? ${question}${colors.reset} `, (answer) => {
      rl.close()
      resolve(answer.trim())
    })
  })
}

module.exports = { askQuestion }
