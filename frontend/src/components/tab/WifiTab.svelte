<!-- Copyright (c) 2025 David Bertet. Licensed under the MIT License. -->

<script>
  import { onMount, onDestroy } from 'svelte'
  import { wsState, sendMessage, onMessageType } from 'src/lib/ws.svelte.js'
  import { Button } from '$lib/components/ui/button'
  import * as Card from '$lib/components/ui/card'
  import * as AlertDialog from '$lib/components/ui/alert-dialog'
  import { Badge } from '$lib/components/ui/badge'
  import { Skeleton } from '$lib/components/ui/skeleton'
  import { cn } from '$lib/utils'

  import { RefreshCw, Activity, AlertTriangle } from 'lucide-svelte'

  import SectionHeader from 'src/components/common/SectionHeader.svelte'
  import WifiStatusCard from 'src/components/wifi/WifiStatusCard.svelte'
  import WifiConnectCard from 'src/components/wifi/WifiConnectCard.svelte'

  let wifiStatus = $state(null)
  let wifiStatusLoading = $state(true)
  let wifiStatusUnsub = $state(null)
  let showDisconnectDialog = $state(false)
  let isDisconnecting = $state(false)

  // Disable ping pong when disconnecting
  $effect(() => (wsState.pingPaused = isDisconnecting))

  function getSignalStrength(rssi) {
    if (rssi >= -30) return 'Excellent'
    if (rssi >= -50) return 'Good'
    if (rssi >= -60) return 'Fair'
    if (rssi >= -70) return 'Weak'
    return 'Very Weak'
  }

  onMount(() => {
    wifiStatusUnsub = onMessageType('wifi_status', (data) => {
      wifiStatus = data.status || null
      wifiStatusLoading = false
    })

    // Also listen for successful connections to refresh status
    const connectSuccessUnsub = onMessageType('wifi_connect_success', (data) => {
      // Refresh status after successful connection
      setTimeout(() => {
        refreshStatus()
      }, 500)
    })

    sendMessage({ type: 'wifi_status' })

    return () => {
      if (wifiStatusUnsub) wifiStatusUnsub()
      if (connectSuccessUnsub) connectSuccessUnsub()
    }
  })

  onDestroy(() => {
    if (wifiStatusUnsub) wifiStatusUnsub()
  })

  function refreshStatus() {
    wifiStatusLoading = true
    sendMessage({ type: 'wifi_status' })
  }

  function confirmDisconnect() {
    showDisconnectDialog = true
  }

  function disconnect() {
    if (window.location.host.includes('192.168.4.1')) {
      showDisconnectDialog = false
    } else {
      isDisconnecting = true
    }
    sendMessage({ type: 'wifi_disconnect' })
  }
</script>

<!-- Header -->
<SectionHeader title="WiFi Settings" subtitle="Connect your device to a wireless network" />

<div class="space-y-6">
  <!-- Device Status Card -->
  <Card.Root>
    <Card.Header>
      <div class="flex items-center justify-between">
        <div class="space-y-1">
          <Card.Title class="flex items-center gap-2">
            <Activity class="h-5 w-5" />
            Device Status
          </Card.Title>
          <Card.Description>Current WiFi configuration and status</Card.Description>
        </div>
        <div class="flex items-center gap-2">
          {#if wifiStatusLoading}
            <Skeleton class="h-6 w-20" />
          {:else if wifiStatus}
            <Badge variant="secondary">
              {wifiStatus.mode} Mode
            </Badge>
          {:else}
            <Badge variant="destructive">Unknown</Badge>
          {/if}

          <Button
            variant="outline"
            size="icon"
            onclick={refreshStatus}
            disabled={wifiStatusLoading}
          >
            <div class:animate-spin={wifiStatusLoading}><RefreshCw class="h-4 w-4" /></div>
          </Button>
        </div>
      </div>
    </Card.Header>
    <Card.Content>
      {#if wifiStatusLoading}
        <WifiStatusCard loading={true} />
      {:else if wifiStatus}
        <WifiStatusCard status={wifiStatus} {getSignalStrength} onDisconnect={confirmDisconnect} />
      {:else}
        <div class="flex flex-col items-center justify-center py-12 text-center">
          <AlertTriangle class="h-12 w-12 text-muted-foreground mb-4" />
          <h3 class="text-lg font-semibold mb-2">No WiFi Status Available</h3>
          <p class="text-muted-foreground mb-4">
            Unable to retrieve device WiFi information. Check your connection and try refreshing.
          </p>
          <Button variant="outline" onclick={refreshStatus}>
            <RefreshCw class="h-4 w-4 mr-2" />
            Refresh Status
          </Button>
        </div>
      {/if}
    </Card.Content>
  </Card.Root>

  <!-- Connection Setup Card -->
  {#if wifiStatus && !wifiStatus.sta.connected}
    <WifiConnectCard {getSignalStrength} />
  {/if}
</div>

<div
  class={cn(
    'fixed inset-0 z-50 backdrop-blur-sm bg-black/10 transition-all duration-300 cursor-not-allowed',
    !isDisconnecting && 'hidden',
  )}
></div>

<!-- Disconnect Confirmation Dialog -->
<AlertDialog.Root bind:open={showDisconnectDialog}>
  <AlertDialog.Content>
    <AlertDialog.Header>
      <AlertDialog.Title
        >{isDisconnecting
          ? 'Disconnecting...'
          : `Disconnect from ${wifiStatus.sta.ssid}?`}</AlertDialog.Title
      >
      <AlertDialog.Description class="space-y-3 text-sm">
        <p>Once disconnected, you will need to reconnect manually.</p>

        <div class="bg-muted/50 p-4 rounded-lg border-l-4 border-orange-500">
          <p class="font-semibold text-foreground mb-2">To set up a new WiFi connection:</p>
          <ol class="space-y-2 text-sm">
            <li class="flex items-start gap-2">
              <span
                class="flex-shrink-0 w-5 h-5 bg-primary text-primary-foreground rounded-full text-xs flex items-center justify-center font-medium"
                >1</span
              >
              <span
                >Connect your computer to the <span
                  class="font-mono bg-background px-1.5 py-0.5 rounded border">"RainMaker"</span
                > WiFi network</span
              >
            </li>
            <li class="flex items-start gap-2">
              <span
                class="flex-shrink-0 w-5 h-5 bg-primary text-primary-foreground rounded-full text-xs flex items-center justify-center font-medium"
                >2</span
              >
              <span
                >Visit <span
                  class="font-mono bg-background px-1.5 py-0.5 rounded border text-blue-600"
                >
                  {#if isDisconnecting}
                    <a href="http://192.168.4.1">http://192.168.4.1</a>
                  {:else}
                    http://192.168.4.1
                  {/if}
                </span> in your browser</span
              >
            </li>
          </ol>
        </div>
      </AlertDialog.Description>
    </AlertDialog.Header>

    {#if !isDisconnecting}
      <AlertDialog.Footer>
        <AlertDialog.Cancel
          onclick={() => {
            showDisconnectDialog = false
          }}
        >
          Cancel
        </AlertDialog.Cancel>
        <AlertDialog.Action
          onclick={disconnect}
          class="bg-destructive text-destructive-foreground hover:bg-destructive/90"
        >
          Disconnect
        </AlertDialog.Action>
      </AlertDialog.Footer>
    {/if}
  </AlertDialog.Content>
</AlertDialog.Root>
