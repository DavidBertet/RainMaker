// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

const { exec, spawn } = require('child_process')

function execPromise(command, options = {}) {
  return new Promise((resolve, reject) => {
    exec(command, options, (error, stdout, stderr) => {
      if (error) {
        reject({ error, stdout, stderr })
      } else {
        resolve({ stdout, stderr })
      }
    })
  })
}

function execWithOutput(command, options = {}, newLine = null) {
  return new Promise((resolve, reject) => {
    const child = spawn(command, { shell: true, ...options })
    let stdout = ''
    let stderr = ''

    child.stdout.on('data', (data) => {
      const output = data.toString()
      stdout += output

      if (!newLine) {
        return
      }

      // Process output line by line
      const lines = output.split('\n')

      for (const line of lines) {
        if (!line.trim()) continue // Skip empty lines

        newLine(line)
      }
    })

    child.stderr.on('data', (data) => {
      stderr += data.toString()
    })

    child.on('close', (code) => {
      if (code === 0) {
        resolve({ stdout, stderr, code })
      } else {
        reject({
          stdout,
          stderr,
          code,
          error: new Error(`Command failed with code ${code}`),
        })
      }
    })

    child.on('error', (error) => {
      reject({ stdout, stderr, error })
    })
  })
}

async function commandExists(command) {
  try {
    await execPromise(`which ${command}`)
    return true
  } catch {
    return false
  }
}

module.exports = { execPromise, execWithOutput, commandExists }
