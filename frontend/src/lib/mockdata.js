// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

export function generateMockResponse(data) {
  switch (data.type) {
    case 'ping':
      return [
        {
          type: 'pong',
        },
      ]

    case 'time_update':
      return [
        {
          type: 'time_update_response',
          success: true,
          current_time: Math.floor(Date.now() / 1000),
          formatted_time: new Date().toISOString(),
        },
      ]

    case 'wifi_scan':
      // Generate 3-6 random networks
      const numNetworks = Math.floor(Math.random() * 4) + 3 // 3 to 6
      const baseNames = ['Mock_Network', 'Test_AP', 'ESP32', 'OfficeNet', 'CafeWiFi', 'IoTNet']
      const networks = Array.from({ length: numNetworks }, (_, i) => ({
        ssid: `${baseNames[i % baseNames.length]}`,
        rssi: -30 - Math.floor(Math.random() * 60), // -30 to -89
        secure: i < 3,
      }))
      return [
        {
          type: 'wifi_list',
          networks,
        },
      ]

    case 'wifi_status':
      return [
        {
          type: 'wifi_status',
          status: wifiDisconnected,
        },
      ]

    case 'wifi_connect':
      if (data.password === 'password') {
        settings.wifi.setup = true
        settings.wifi.connected = true
        return [
          {
            type: 'wifi_status',
            status: wifiConnected,
          },
          {
            type: 'settings',
            ...settings,
          },
        ]
      } else {
        return [{ type: 'error', message: 'Password incorrect' }]
      }

    case 'wifi_disconnect':
      settings.wifi.setup = false
      settings.wifi.connected = false

      return [
        {
          type: 'wifi_status',
          status: wifiDisconnected,
        },
        {
          type: 'settings',
          ...settings,
        },
      ]

    case 'get_system_info':
      return [
        {
          type: 'system_info',
          settings: systemInfo,
        },
      ]

    case 'create_or_update_zone':
      if (data.zone_id) {
        let index = zones.findIndex((z) => z.id === data.zone_id)
        zones[index].name = data.name
        zones[index].output = data.output
      } else {
        zones = [
          ...zones,
          { id: Math.floor(Math.random() * 100), name: data.name, output: data.output },
        ]
      }
      return [
        {
          type: 'zone_list',
          zones: zones,
        },
      ]

    case 'delete_zone':
      return [
        {
          type: 'error',
          message: "This demo can't delete zones",
        },
      ]

    case 'get_zones':
      return [
        {
          type: 'zone_list',
          zones: zones,
        },
      ]

    case 'get_programs':
      return [
        {
          type: 'program_list',
          programs: programs,
        },
      ]

    case 'create_or_update_program':
      return [
        {
          type: 'error',
          message: "This demo can't modify programs",
        },
      ]

    case 'delete_program':
      return [
        {
          type: 'error',
          message: "This demo can't delete programs",
        },
      ]

    case 'test_manual':
      return [
        {
          type: 'error',
          message: "This demo can't run programs",
        },
      ]

    case 'enable':
      if (data.zone_id) {
        let index = zones.findIndex((z) => z.id === data.zone_id)
        zones[index].enabled = data.is_enabled
        zones[index].status = data.is_enabled ? 'idle' : 'disabled'
        return [
          {
            type: 'zone_list',
            zones: zones,
          },
        ]
      } else {
        let index = programs.findIndex((z) => z.id === data.program_id)
        programs[index].enabled = data.is_enabled
        return [
          {
            type: 'program_list',
            programs: programs,
          },
        ]
      }

    case 'get_settings':
      return [
        {
          type: 'settings',
          ...settings,
        },
      ]
  }
}

let settings = {
  ota: {
    requiresPassword: true,
  },
  wifi: {
    connected: false,
    setup: false,
  },
}

let zones = [
  {
    id: 1,
    name: 'Front Lawn Demo',
    output: 999,
    enabled: true,
    lastRun: (new Date().getTime() - 50000) / 1000,
    status: 'idle',
  },
  {
    id: 2,
    name: 'Back Garden Demo',
    output: 998,
    enabled: true,
    lastRun: (new Date().getTime() - 20000) / 1000,
    status: 'running',
  },
  {
    id: 3,
    name: 'Flower Beds Demo',
    output: 997,
    enabled: true,
    lastRun: new Date().getTime() / 1000,
    status: 'idle',
  },
  {
    id: 4,
    name: 'Side Yard Demo',
    output: 996,
    enabled: false,
    lastRun: 0,
    status: 'disabled',
  },
]

let programs = [
  {
    id: 1,
    name: 'Morning Routine  Demo',
    enabled: true,
    schedule: {
      days: [0, 2, 3],
      startTime: '06:00',
    },
    zones: [
      { id: 1, duration: 999, order: 1 },
      { id: 2, duration: 999, order: 2 },
      { id: 3, duration: 999, order: 3 },
    ],
    lastRun: (new Date().getTime() - 5000) / 1000,
    nextRun: (new Date().getTime() + 5000) / 1000,
    status: 'scheduled',
  },
  {
    id: 2,
    name: 'Weekend Deep Water',
    enabled: true,
    schedule: {
      days: [5, 6],
      startTime: '05:30',
    },
    zones: [
      { id: 1, duration: 999, order: 1 },
      { id: 2, duration: 999, order: 2 },
      { id: 4, duration: 999, order: 3 },
    ],
    lastRun: (new Date().getTime() - 8000) / 1000,
    nextRun: (new Date().getTime() + 8000) / 1000,
    status: 'running',
  },
]

let wifiDisconnected = {
  mode: 'AP+STA',
  mac: '12:34:56:78:90:ab',
  sta: { connected: false, configured_ssid: '' },
  ap: {
    ssid: 'RainMaker',
    channel: 10,
    auth_mode: 'WPA_WPA2_PSK',
    ip: '192.168.4.1',
    mac: 'ab:cd:ef:12:34:56',
    connected_stations: 1,
    max_connections: 2,
  },
}

let wifiConnected = {
  mode: 'STA',
  mac: '12:34:56:78:90:ab',
  sta: {
    connected: true,
    ssid: 'Wi Believe I Can Fi',
    rssi: -48,
    channel: 8,
    auth_mode: 'WPA2_PSK',
    ip: '192.168.1.10',
    gateway: '192.168.1.1',
    netmask: '255.255.255.0',
  },
}

let systemInfo = {
  device: {
    status: 'Online and operational',
    reset_reason: 'Power-on reset',
    uptime: '5 minutes, 42seconds',
    time: '2025-01-01 00:00 PM',
  },
  system: { idf_version: '5.4.0', freertos_tasks: 12 },
  hardware: {
    chip_model: 'ESP32',
    chip_revision: 301,
    cpu_cores: 2,
    flash_size: '4.0 MB',
  },
  memory: {
    heap_total: '269.8 KB',
    heap_free: '163.2 KB',
    heap_used: '106.6 KB',
    heap_usage: '39%',
    heap_largest_free_block: '108.0 KB',
    heap_min_free_ever: '145.4 KB',
    internal_total: '302.0 KB',
    internal_free: '194.6 KB',
    internal_usage: '35%',
  },
  psram: { psram_total: '0 bytes', psram_free: '0 bytes', psram_usage: '0%' },
  spiffs: {
    status: 'Mounted and operational',
    partition_size: '960.0 KB',
    partition_label: 'storage',
    partition_address: '0x310000',
    total_space: '875.3 KB',
    used_space: '171.3 KB',
    free_space: '704.0 KB',
    usage: '19%',
    files_count: 4,
    total_size: '168.8 KB',
  },
}
