<!-- Copyright (c) 2025 David Bertet. Licensed under the MIT License. -->

<script>
  import { onMount } from 'svelte'
  import { sendMessage, onMessageType } from 'src/lib/ws.svelte.js'

  import * as Card from '$lib/components/ui/card'
  import { Button } from '$lib/components/ui/button'
  import * as AlertDialog from '$lib/components/ui/alert-dialog'
  import { Plus, Droplets } from 'lucide-svelte'

  import SectionHeader from 'src/components/common/SectionHeader.svelte'
  import EditZoneModal from 'src/components/zone/EditZoneModal.svelte'
  import ZoneCardSkeleton from 'src/components/zone/ZoneCardSkeleton.svelte'
  import ZoneCard from 'src/components/zone/ZoneCard.svelte'

  let loading = $state(true)
  let zonesUnsub = $state(null)
  let zones = $state([])
  let oneIsRunning = $derived(zones.find((z) => z.status === 'running') != null)

  onMount(() => {
    zonesUnsub = onMessageType('zone_list', (data) => {
      zones = data.zones || []
      loading = false
    })
    loading = true
    sendMessage({ type: 'get_zones' })
    return () => {
      if (zonesUnsub) zonesUnsub()
    }
  })

  let editingZone = $state(null)
  let showEditModal = $state(false)
  let showDeleteDialog = $state(false)
  let zoneToDelete = $state(null)

  // New zone form
  let newZone = $state({
    name: '',
    output: 1,
  })

  function addZone() {
    editingZone = null
    showEditModal = true
  }

  function editZone(zone) {
    editingZone = zone
    showEditModal = true
  }

  function saveZone(zone) {
    sendMessage({
      type: 'create_or_update_zone',
      ...zone,
    })
    editingZone = null
  }

  function cancelEdit() {
    editingZone = null
  }

  function enableZone(zone) {
    sendMessage({ type: 'enable', zone_id: zone.id, is_enabled: !zone.enabled })
  }

  function confirmDeleteZone(zone) {
    zoneToDelete = zone
    showDeleteDialog = true
  }

  function deleteZone() {
    if (zoneToDelete) {
      sendMessage({ type: 'delete_zone', zone_id: zoneToDelete.id })
      zoneToDelete = null
      showDeleteDialog = false
    }
  }

  async function testZone(zone) {
    sendMessage({ type: 'test_manual', zone_id: zone.id, action: 'start' })
  }

  function stopTest(zone) {
    sendMessage({ type: 'test_manual', action: 'stop' })
  }
</script>

<div>
  <div class="flex justify-between items-center gap-2">
    <SectionHeader title="Zones" subtitle="Separate watering areas in your irrigation system" />
    <Button onclick={addZone}>
      <Plus />
      <span class="hidden sm:inline">Create Zone</span>
    </Button>
  </div>

  {#if loading}
    <div class="grid grid-cols-1 lg:grid-cols-2 xl:grid-cols-3 gap-6">
      {#each Array(3) as _}
        <ZoneCardSkeleton />
      {/each}
    </div>
  {:else if zones.length === 0}
    <Card.Root class="text-center py-12">
      <Card.Content>
        <Droplets class="h-12 w-12 mx-auto text-muted-foreground mb-4" />
        <h3 class="text-lg font-semibold mb-2">No zones configured yet</h3>
        <p class="text-muted-foreground mb-4">
          Create your first watering zone to get started with automated irrigation.
        </p>
        <Button onclick={addZone}>
          <Plus class="h-4 w-4 mr-2" />
          Create Your First Zone
        </Button>
      </Card.Content>
    </Card.Root>
  {:else}
    <div class="grid grid-cols-1 lg:grid-cols-2 xl:grid-cols-3 gap-6">
      {#each zones as zone (zone.id)}
        <ZoneCard
          {zone}
          {oneIsRunning}
          onenable={enableZone}
          onedit={editZone}
          ondelete={confirmDeleteZone}
          onrun={testZone}
          onstop={stopTest}
        />
      {/each}
    </div>
  {/if}
</div>

<EditZoneModal bind:open={showEditModal} zone={editingZone} onsave={saveZone} />

<!-- Delete Confirmation Dialog -->
<AlertDialog.Root bind:open={showDeleteDialog}>
  <AlertDialog.Content>
    <AlertDialog.Header>
      <AlertDialog.Title>Delete Zone</AlertDialog.Title>
      <AlertDialog.Description>
        Are you sure you want to delete "{zoneToDelete?.name}"? This action cannot be undone.
      </AlertDialog.Description>
    </AlertDialog.Header>
    <AlertDialog.Footer>
      <AlertDialog.Cancel
        onclick={() => {
          showDeleteDialog = false
          zoneToDelete = null
        }}
      >
        Cancel
      </AlertDialog.Cancel>
      <AlertDialog.Action
        onclick={deleteZone}
        class="bg-destructive text-destructive-foreground hover:bg-destructive/90"
      >
        Delete Zone
      </AlertDialog.Action>
    </AlertDialog.Footer>
  </AlertDialog.Content>
</AlertDialog.Root>
