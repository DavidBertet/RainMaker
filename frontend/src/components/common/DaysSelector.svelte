<!-- Copyright (c) 2025 David Bertet. Licensed under the MIT License. -->

<script>
  import * as ToggleGroup from '$lib/components/ui/toggle-group'
  import { Label } from '$lib/components/ui/label'

  let { selectedDays = $bindable([]) } = $props()

  const dayOptions = ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun']
</script>

<div class="space-y-2">
  <Label>Days of the Week</Label>
  <div class="flex flex-wrap gap-2">
    <ToggleGroup.Root variant="outline" size="lg" type="multiple" bind:value={selectedDays}>
      {#each dayOptions as day, index}
        <ToggleGroup.Item
          value={(index + 1) % 7}
          class="data-[state=on]:bg-primary data-[state=on]:text-primary-foreground"
          >{day}</ToggleGroup.Item
        >
      {/each}
    </ToggleGroup.Root>
  </div>
</div>
