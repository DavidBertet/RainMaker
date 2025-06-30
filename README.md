# RainMaker

This is a sprinkler controller designed to manage irrigation systems. It features a lightweight frontend to define zones and programs, and is running on an ESP32.

## Installation

Use the cli by running `./install.sh`

## Folder Architecture

- **`frontend/`**: Contains the Svelte application for the user interface.

  - **`src/`**: Source files for the Svelte application.
  - **`public/`**: Static files served by the application.
  - **`dist/`**: Compiled production files ready for deployment to the ESP32.

- **`backend/`**: Contains the ESP32 backend code.
  - **`src/`**: Source files for the ESP-IDF application.
  - **`data/`**: Directory for static files served by the backend.
  - **`platformio.ini`**: Configuration file for PlatformIO.
