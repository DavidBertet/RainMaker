// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

function getParameterValue(args, index) {
  const nextArg = args[index + 1]
  // Check if next argument exists and doesn't start with '-' (isn't another flag)
  return nextArg && !nextArg.startsWith('-') ? nextArg : null
}

function validateArgs(parsedArgs) {
  if (parsedArgs.frontendOnly && parsedArgs.backendOnly) {
    console.error('Error: Cannot specify both --frontend-only and --backend-only')
    process.exit(1)
  }

  if (parsedArgs.hasOta && !parsedArgs.otaIP) {
    console.error('Error: --ota flag requires an IP address')
    process.exit(1)
  } else if (parsedArgs.otaIP) {
    const ipv4Regex = /^(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})$/
    if (!ipv4Regex.test(parsedArgs.otaIP)) {
      console.error('Error: --ota flag requires a valid IPv4 address')
      process.exit(1)
    }
  }

  if (parsedArgs.hasUploadPassword && !parsedArgs.uploadPassword) {
    console.error('Error: --upload-password flag requires a password')
    process.exit(1)
  }
}

function parseArgs() {
  const args = process.argv.slice(2)
  const otaIndex = args.findIndex((arg) => arg === '-o' || arg === '--ota')
  const passwordIndex = args.findIndex((arg) => arg === '-p' || arg === '--upload-password')

  const parsedArgs = {
    // -h help is managed by the shell script
    hasOta: otaIndex !== -1,
    otaIP: otaIndex !== -1 ? getParameterValue(args, otaIndex) : null,
    autoYes: args.includes('-y') || args.includes('--yes'),
    frontendOnly: args.includes('-f') || args.includes('--frontend-only'),
    backendOnly: args.includes('-b') || args.includes('--backend-only'),
    hasUploadPassword: passwordIndex !== -1,
    uploadPassword: passwordIndex !== -1 ? getParameterValue(args, passwordIndex) : null,
    args: args,
  }

  validateArgs(parsedArgs)

  return parsedArgs
}

module.exports = { parseArgs }
