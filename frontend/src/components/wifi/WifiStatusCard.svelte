<!-- Copyright (c) 2025 David Bertet. Licensed under the MIT License. -->

<script>
  import { Badge } from '$lib/components/ui/badge'
  import { Button } from '$lib/components/ui/button'
  import { Separator } from '$lib/components/ui/separator'
  import { Skeleton } from '$lib/components/ui/skeleton'
  import { Wifi, WifiOff, Shield, Users, MapPin, Hash } from 'lucide-svelte'

  import SignalBars from 'src/components/wifi/SignalBars.svelte'

  let { status = null, loading = false, getSignalStrength, onDisconnect } = $props()
</script>

{#if loading}
  <div class="space-y-6">
    <div class="flex items-center justify-between">
      <Skeleton class="h-4 w-32" />
      <Skeleton class="h-4 w-24" />
    </div>

    <div class="space-y-4">
      <div class="space-y-2">
        <Skeleton class="h-5 w-40" />
        <div class="bg-muted/50 rounded-lg p-4 space-y-3">
          <div class="grid grid-cols-1 md:grid-cols-2 gap-4">
            {#each Array(4) as _}
              <div class="space-y-1">
                <Skeleton class="h-3 w-16" />
                <Skeleton class="h-4 w-24" />
              </div>
            {/each}
          </div>
        </div>
      </div>
    </div>
  </div>
{:else if status}
  <div class="space-y-6">
    <!-- Basic Info -->
    <div class="flex items-center justify-between">
      <div class="space-y-1">
        <p class="text-sm font-medium">MAC Address</p>
        <p class="text-sm text-muted-foreground font-mono">{status.mac}</p>
      </div>
    </div>

    <!-- Station Connection -->
    {#if status.sta}
      <Separator />

      <div class="space-y-4">
        <div class="flex items-center justify-between">
          <h4 class="text-sm font-semibold flex items-center gap-2">
            {#if status.sta.connected}
              <Wifi class="h-4 w-4 text-green-600" />
            {:else}
              <WifiOff class="h-4 w-4 text-red-500" />
            {/if}
            Station Connection
          </h4>
          <div class="flex items-center gap-2">
            <Badge variant={status.sta.connected ? 'default' : 'destructive'}>
              {status.sta.connected ? 'Connected' : 'Disconnected'}
            </Badge>
            {#if status.sta.connected}
              <Button variant="outline" size="sm" onclick={onDisconnect}>
                <WifiOff class="h-3 w-3 mr-1" />
                Disconnect
              </Button>
            {/if}
          </div>
        </div>

        {#if status.sta.connected}
          <div class="bg-muted/50 rounded-lg p-4 space-y-4">
            <div class="flex items-center justify-between">
              <h5 class="font-medium">{status.sta.ssid}</h5>
              <div class="flex items-center gap-2">
                <SignalBars rssi={status.sta.rssi} />
                <span class="text-xs text-muted-foreground">
                  {status.sta.rssi} dBm ({getSignalStrength(status.sta.rssi)})
                </span>
              </div>
            </div>

            <div class="grid grid-cols-1 md:grid-cols-2 gap-4">
              <div class="space-y-1">
                <p class="text-xs font-medium text-muted-foreground">IP Address</p>
                <p class="text-sm font-mono">{status.sta.ip}</p>
              </div>
              <div class="space-y-1">
                <p class="text-xs font-medium text-muted-foreground">Gateway</p>
                <p class="text-sm font-mono">{status.sta.gateway}</p>
              </div>
              <div class="space-y-1">
                <p class="text-xs font-medium text-muted-foreground">Netmask</p>
                <p class="text-sm font-mono">{status.sta.netmask}</p>
              </div>
              <div class="space-y-1">
                <p class="text-xs font-medium text-muted-foreground">Channel</p>
                <p class="text-sm">{status.sta.channel}</p>
              </div>
              <div class="space-y-1">
                <p class="text-xs font-medium text-muted-foreground">Security</p>
                <div class="flex items-center gap-1">
                  <Shield class="h-3 w-3" />
                  <p class="text-sm">{status.sta.auth_mode}</p>
                </div>
              </div>
            </div>
          </div>
        {/if}
      </div>
    {/if}

    <!-- Access Point -->
    {#if status.ap}
      <Separator />
      <div class="space-y-4">
        <div class="flex items-center justify-between">
          <h4 class="text-sm font-semibold flex items-center gap-2">
            <Wifi class="h-4 w-4 text-blue-600" />
            Access Point
          </h4>
          <Badge variant="secondary">Active</Badge>
        </div>

        <div class="bg-muted/50 rounded-lg p-4 space-y-4">
          <div class="flex items-center justify-between">
            <h5 class="font-medium">{status.ap.ssid}</h5>
            <div class="flex items-center gap-1 text-sm text-muted-foreground">
              <Users class="h-3 w-3" />
              {status.ap.connected_stations}/{status.ap.max_connections} clients
            </div>
          </div>

          <div class="grid grid-cols-1 md:grid-cols-2 gap-4">
            <div class="space-y-1">
              <p class="text-xs font-medium text-muted-foreground">IP Address</p>
              <p class="text-sm font-mono">{status.ap.ip}</p>
            </div>
            <div class="space-y-1">
              <p class="text-xs font-medium text-muted-foreground">MAC Address</p>
              <p class="text-sm font-mono">{status.ap.mac}</p>
            </div>
            <div class="space-y-1">
              <p class="text-xs font-medium text-muted-foreground">Channel</p>
              <p class="text-sm">{status.ap.channel}</p>
            </div>
            <div class="space-y-1">
              <p class="text-xs font-medium text-muted-foreground">Security</p>
              <div class="flex items-center gap-1">
                <Shield class="h-3 w-3" />
                <p class="text-sm">{status.ap.auth_mode}</p>
              </div>
            </div>
          </div>
        </div>
      </div>
    {/if}
  </div>
{/if}
