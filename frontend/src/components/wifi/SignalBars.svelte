<!-- Copyright (c) 2025 David Bertet. Licensed under the MIT License. -->

<script>
  let { rssi } = $props()

  let bars = $derived(getSignalBars(rssi))
  let signalClass = $derived(
    bars >= 3 ? 'text-green-500' : bars >= 2 ? 'text-yellow-500' : 'text-red-500',
  )

  function getSignalBars(rssi) {
    if (rssi >= -30) return 4
    if (rssi >= -50) return 3
    if (rssi >= -60) return 2
    if (rssi >= -70) return 1
    return 0
  }
</script>

<div class="flex items-end gap-0.5 h-4" aria-label="Signal strength: {bars} out of 4 bars">
  {#each Array(4) as _, i}
    <div
      class="w-1 bg-current transition-colors {i < bars ? signalClass : 'text-muted-foreground/30'}"
      style="height: {(i + 1) * 25}%"
    ></div>
  {/each}
</div>
