<!-- Copyright (c) 2025 David Bertet. Licensed under the MIT License. -->

<script>
  import * as Select from '$lib/components/ui/select'

  let { zoneId = $bindable(), programZones, zones } = $props()

  let currentLabel = $derived(zones.find((zone) => zone.id === zoneId).name)
  let programZoneIdSet = $derived(new Set(programZones.map((zone) => zone.id)))
  let zoneIdSet = $derived(new Set(zones.map((zone) => zone.id)))
  let availableZoneIds = $derived(zoneIdSet.difference(programZoneIdSet))
</script>

<Select.Root type="single" bind:value={zoneId}>
  <Select.Trigger class="cursor-pointer w-full">
    {currentLabel ?? 'Choose a Zone'}
  </Select.Trigger>
  <Select.Content>
    {#each zones as zone}
      <Select.Item
        class="cursor-pointer hover:bg-muted"
        value={zone.id}
        disabled={zone.id != zoneId && !availableZoneIds.has(zone.id)}>{zone.name}</Select.Item
      >
    {/each}
  </Select.Content>
</Select.Root>
