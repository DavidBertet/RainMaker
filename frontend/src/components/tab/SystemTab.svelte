<!-- Copyright (c) 2025 David Bertet. Licensed under the MIT License. -->

<script>
  import { onMount, onDestroy } from 'svelte'
  import { sendMessage, onMessageType } from 'src/lib/ws.svelte.js'
  import * as Card from '$lib/components/ui/card'
  import { Separator } from '$lib/components/ui/separator'
  import { Skeleton } from '$lib/components/ui/skeleton'

  import SectionHeader from 'src/components/common/SectionHeader.svelte'

  let settingsUnsub = $state(null)
  let settings = $state([])
  let loading = $state(true)
  let scanPollTimeout = $state(null)

  onMount(() => {
    settingsUnsub = onMessageType('system_info', (data) => {
      settings = data.settings
      loading = false

      scanPollTimeout = setTimeout(() => {
        sendMessage({ type: 'get_system_info' })
      }, 5000)
    })
    sendMessage({ type: 'get_system_info' })
    return () => {
      if (settingsUnsub) settingsUnsub()
      if (scanPollTimeout) clearTimeout(scanPollTimeout)
    }
  })

  onDestroy(() => {
    if (settingsUnsub) settingsUnsub()
    if (scanPollTimeout) clearTimeout(scanPollTimeout)
  })

  function formatKey(key) {
    return key
      .split('_')
      .map((word) => word[0].toUpperCase() + word.slice(1))
      .join(' ')
  }

  function formatValue(value) {
    if (typeof value === 'boolean') return value ? 'Yes' : 'No'
    if (typeof value === 'object') return JSON.stringify(value)
    return String(value)
  }
</script>

<SectionHeader title="System Information" subtitle="View device status and specifications" />

<div>
  {#if loading}
    <div class="space-y-8">
      {#each Array(2) as _}
        <div class="space-y-4">
          <Skeleton class="h-8 w-48" />
          <Separator />
          <div class="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
            {#each Array(3) as _}
              <Card.Root>
                <Card.Header class="pb-3">
                  <Skeleton class="h-4 w-24" />
                </Card.Header>
                <Card.Content>
                  <Skeleton class="h-6 w-32" />
                </Card.Content>
              </Card.Root>
            {/each}
          </div>
        </div>
      {/each}
    </div>
  {:else}
    <div class="space-y-8">
      {#each Object.entries(settings) as [sectionKey, sectionData]}
        <div class="space-y-4">
          <div class="flex items-center space-x-4">
            <Card.Title class="text-2xl font-semibold tracking-tight">
              {formatKey(sectionKey)}
            </Card.Title>
            <Separator class="flex-1" />
          </div>

          <div class="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
            {#each Object.entries(sectionData) as [key, value]}
              <Card.Root class="transition-shadow hover:shadow-md">
                <Card.Header class="pb-3">
                  <Card.Description class="text-sm font-medium">
                    {formatKey(key)}
                  </Card.Description>
                </Card.Header>
                <Card.Content class="pt-0">
                  <div class="flex items-center space-x-2">
                    <p class="text-lg font-semibold break-words">
                      {formatValue(value)}
                    </p>
                  </div>
                </Card.Content>
              </Card.Root>
            {/each}
          </div>
        </div>
      {/each}
    </div>
  {/if}
</div>
