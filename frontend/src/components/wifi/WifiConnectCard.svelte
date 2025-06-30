<!-- Copyright (c) 2025 David Bertet. Licensed under the MIT License. -->

<script>
  import { onMount, onDestroy } from 'svelte'
  import { wsState, sendMessage, onMessageType } from 'src/lib/ws.svelte.js'
  import { Button } from '$lib/components/ui/button'
  import * as Card from '$lib/components/ui/card'
  import { Input } from '$lib/components/ui/input'
  import { Label } from '$lib/components/ui/label'
  import { Skeleton } from '$lib/components/ui/skeleton'
  import * as Alert from '$lib/components/ui/alert'
  import { RefreshCw, Wifi, Search } from 'lucide-svelte'

  import WifiNetworkList from 'src/components/wifi/WifiNetworkList.svelte'

  // Props
  let { getSignalStrength } = $props()

  // State
  let networks = $state([])
  let selectedNetwork = $state('')
  let password = $state('')
  let connecting = $state(false)
  let loading = $state(true)
  let wifiScanUnsub = $state(null)
  let errorMessageUnsub = $state(null)
  let connectUnsub = $state(null)
  let scanPollInterval = $state(null)

  // Disable ping pong during connection
  $effect(() => (wsState.pingPaused = connecting))

  function startScanPolling() {
    if (scanPollInterval) clearInterval(scanPollInterval)
    scanPollInterval = setInterval(() => {
      if (!connecting) {
        sendMessage({ type: 'wifi_scan' })
      }
    }, 5000)
  }

  function scanNetworks() {
    loading = true
    sendMessage({ type: 'wifi_scan' })
  }

  function connect() {
    connecting = true
    sendMessage({
      type: 'wifi_connect',
      ssid: selectedNetwork,
      password,
    })
  }

  onMount(() => {
    // Listen for wifi scan results
    wifiScanUnsub = onMessageType('wifi_list', (data) => {
      const prevSelected = selectedNetwork
      let newNetworks = data.networks || []

      if (newNetworks.some((net) => net.ssid === prevSelected)) {
        selectedNetwork = prevSelected
      } else if (prevSelected) {
        newNetworks = [{ ssid: prevSelected, rssi: -100, secure: false }, ...newNetworks]
        selectedNetwork = prevSelected
      } else {
        selectedNetwork = newNetworks.length > 0 ? newNetworks[0].ssid : ''
      }
      networks = newNetworks
      loading = false

      if (!scanPollInterval) {
        startScanPolling()
      }
    })

    errorMessageUnsub = onMessageType('error', (data) => {
      connecting = false
    })

    // Listen for connection results
    connectUnsub = onMessageType('wifi_connect_success', (data) => {
      connecting = false
      password = ''
      // Clear polling since we're now connected
      if (scanPollInterval) {
        clearInterval(scanPollInterval)
        scanPollInterval = null
      }
    })

    // Initial scan
    sendMessage({ type: 'wifi_scan' })

    return () => {
      if (wifiScanUnsub) wifiScanUnsub()
      if (connectUnsub) connectUnsub()
      if (scanPollInterval) clearInterval(scanPollInterval)
      if (errorMessageUnsub) errorMessageUnsub()
    }
  })

  onDestroy(() => {
    if (wifiScanUnsub) wifiScanUnsub()
    if (connectUnsub) connectUnsub()
    if (scanPollInterval) clearInterval(scanPollInterval)
    if (errorMessageUnsub) errorMessageUnsub()
  })

  export function stopPolling() {
    if (scanPollInterval) {
      clearInterval(scanPollInterval)
      scanPollInterval = null
    }
  }
</script>

<Card.Root>
  <Card.Header>
    <Card.Title class="flex items-center gap-2">
      <Wifi class="h-5 w-5" />
      Connect to Network
    </Card.Title>
    <Card.Description>Select a WiFi network and enter credentials</Card.Description>
  </Card.Header>
  <Card.Content class="space-y-4">
    {#if loading}
      <div class="space-y-4">
        <div class="flex items-center justify-between">
          <Skeleton class="h-4 w-32" />
          <Skeleton class="h-4 w-4 rounded-full animate-spin" />
        </div>
        <div class="space-y-2">
          {#each Array(3) as _}
            <div class="flex items-center justify-between p-3 border rounded-lg">
              <Skeleton class="h-4 w-40" />
              <div class="flex gap-1">
                {#each Array(4) as _}
                  <Skeleton class="h-3 w-1" />
                {/each}
              </div>
            </div>
          {/each}
        </div>
      </div>
    {:else if networks.length === 0}
      <div
        class="flex flex-col items-center justify-center py-12 text-center border-2 border-dashed rounded-lg"
      >
        <Search class="h-12 w-12 text-muted-foreground mb-4" />
        <h3 class="text-lg font-semibold mb-2">No Networks Found</h3>
        <p class="text-muted-foreground mb-4">
          No WiFi networks are currently available. Make sure your router is on and try scanning
          again.
        </p>
        <Button variant="outline" onclick={scanNetworks}>
          <RefreshCw class="h-4 w-4 mr-2" />
          Scan Again
        </Button>
      </div>
    {:else}
      <WifiNetworkList {networks} bind:selectedNetwork {getSignalStrength} />

      <div class="space-y-2">
        <Label for="password">Password</Label>
        <Input
          id="password"
          type="password"
          bind:value={password}
          placeholder="Enter WiFi password"
          onkeydown={(e) => {
            if (e.key === 'Enter' && !connecting && !loading && selectedNetwork) {
              connect()
            }
          }}
        />
      </div>

      <Button class="w-full" onclick={connect} disabled={connecting || loading || !selectedNetwork}>
        {#if connecting}
          <RefreshCw class="h-4 w-4 mr-2 animate-spin" />
          Connecting...
        {:else}
          <Wifi class="h-4 w-4 mr-2" />
          Connect
        {/if}
      </Button>
    {/if}
  </Card.Content>
</Card.Root>
