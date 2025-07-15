```swift
 ___      _      __  __      _
| _ \__ _(_)_ _ |  \/  |__ _| |_____ _ _
|   / _` | | ' \| |\/| / _` | / / -_) '_|
|_|_\__,_|_|_||_|_|  |_\__,_|_\_\___|_|
```

<div align="center">
  <img src="https://img.shields.io/badge/version-1.0.0-blue?style=for-the-badge" alt="Version">
  <img src="https://img.shields.io/badge/license-MIT-green?style=for-the-badge" alt="License">
  <img src="https://img.shields.io/badge/status-active-brightgreen?style=for-the-badge" alt="Status">
</div>

---

This is a sprinkler controller designed to manage irrigation systems. It features a lightweight frontend to define zones and programs, and is running on an ESP32.

Try the [sample](https://davidbertet.github.io/RainMaker/) to see what it is capable of!

[![Web Sample](./assets/programs_preview.jpg)](https://davidbertet.github.io/RainMaker/)

## 🚀 Quick Start

Use the cli to install it on your ESP32 by running `./install.sh`

![CLI Sample](./assets/cli_preview.gif)

Once installed once, can update over the air using the `--ota <IP>` option

Here is how to use it

```shell
Usage: ./install.sh [OPTIONS]

Options:
  -o, --ota <IP>              Set OTA IP address for over-the-air updates
  -p, --upload-password <PWD> Set upload password for authentication
  -y, --yes                   Auto-confirm all prompts (non-interactive mode)
  -f, --frontend-only         Build/deploy frontend only
  -b, --backend-only          Build/deploy backend only
  -h, --help                  Show this help message
```

## 📁 Folder Architecture

- **`cli/`**: Contains the CLI tool for installing/updating the project
- **`frontend/`**: Contains the Svelte application for the user interface.

  - **`src/`**: Source files for the Svelte application.
  - **`dist/`**: Compiled production files ready for deployment to the ESP32.

- **`backend/`**: Contains the ESP32 backend code.
  - **`src/`**: Source files for the ESP-IDF application.
  - **`data/`**: Directory for static files served by the backend.
  - **`platformio.ini`**: Configuration file for PlatformIO.
