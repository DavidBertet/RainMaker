<!-- Copyright (c) 2025 David Bertet. Licensed under the MIT License. -->

<script>
  import { ScrollArea } from '$lib/components/ui/scroll-area'
  import * as RadioGroup from '$lib/components/ui/radio-group'
  import { Label } from '$lib/components/ui/label'

  import { Lock, Wifi } from 'lucide-svelte'

  import SignalBars from 'src/components/wifi/SignalBars.svelte'

  let { networks = [], selectedNetwork = $bindable(''), getSignalStrength } = $props()

  function handleNetworkSelect(ssid) {
    selectedNetwork = ssid
  }
</script>

<div class="space-y-2">
  <Label class="text-sm font-medium">Available Networks</Label>
  <ScrollArea class="h-64 w-full border rounded-lg">
    <div class="p-2">
      <RadioGroup.Root
        value={selectedNetwork}
        onValueChange={handleNetworkSelect}
        class="space-y-1"
      >
        {#each networks as network (network.ssid)}
          <div
            class="rounded-lg"
            class:bg-secondary={selectedNetwork === network.ssid}
            class:text-secondary-foreground={selectedNetwork === network.ssid}
          >
            <Label
              for={network.ssid}
              class="flex items-center space-x-3 p-3 rounded-lg hover:bg-muted/50 transition-colors cursor-pointer group"
            >
              <RadioGroup.Item value={network.ssid} id={network.ssid} />
              <div class="flex-1 flex items-center justify-between">
                <div class="flex items-center gap-2">
                  <Wifi class="h-4 w-4 text-muted-foreground" />
                  <span class="font-medium">{network.ssid}</span>
                  {#if network.secure}
                    <Lock class="h-3 w-3 text-muted-foreground" />
                  {/if}
                </div>
                <div class="flex items-center gap-2">
                  <span class="text-xs text-muted-foreground hidden sm:inline">
                    {getSignalStrength(network.rssi)}
                  </span>
                  <SignalBars rssi={network.rssi} />
                </div>
              </div>
            </Label>
          </div>
        {/each}
      </RadioGroup.Root>
    </div>
  </ScrollArea>
</div>
