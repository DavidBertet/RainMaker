<!-- Copyright (c) 2025 David Bertet. Licensed under the MIT License. -->

<script>
  import { onMount } from 'svelte'
  import { sendMessage, onMessageType } from 'src/lib/ws.svelte.js'

  import * as Card from '$lib/components/ui/card'
  import * as AlertDialog from '$lib/components/ui/alert-dialog'
  import { Button } from '$lib/components/ui/button'
  import { Plus, Droplets } from 'lucide-svelte'

  import SectionHeader from 'src/components/common/SectionHeader.svelte'
  import ProgramCard from 'src/components/program/ProgramCard.svelte'
  import EditProgramModal from 'src/components/program/EditProgramModal.svelte'
  import ProgramCardSkeleton from 'src/components/program/ProgramCardSkeleton.svelte'

  let loading = $state(true)
  let programsUnsub = $state(null)
  let zonesUnsub = $state(null)
  let programs = $state([])
  let zones = $state([])
  let showDeleteDialog = $state(false)

  onMount(() => {
    programsUnsub = onMessageType('program_list', (data) => {
      programs = data.programs || []
      loading = false
    })
    zonesUnsub = onMessageType('zone_list', (data) => {
      zones = data.zones || []
    })
    loading = true
    sendMessage({ type: 'get_programs' })
    sendMessage({ type: 'get_zones' })
    return () => {
      if (programsUnsub) programsUnsub()
      if (zonesUnsub) zonesUnsub()
    }
  })

  let editingProgram = $state(null)
  let showAddModal = $state(false)
  let programToDelete = $state(null)
  let runningProgram = $derived(programs.find((p) => p.status === 'running'))

  function getTotalDuration(program) {
    return program.zones.reduce((total, zone) => total + zone.duration, 0)
  }

  function editProgram(program) {
    editingProgram = {
      ...program,
      schedule: { ...program.schedule, days: [...program.schedule.days] },
      zones: program.zones.map((z) => ({ ...z })),
    }
  }

  function saveProgram(program) {
    sendMessage({
      type: 'create_or_update_program',
      id: program.id,
      name: program.name,
      schedule: {
        days: program.schedule.days,
        start_time: program.schedule.startTime,
      },
      zones: program.zones,
    })
    editingProgram = null
  }

  function cancelEdit() {
    editingProgram = null
  }

  function addNewProgram(program) {
    sendMessage({
      type: 'create_or_update_program',
      name: program.name,
      schedule: {
        days: program.schedule.days,
        start_time: program.schedule.startTime,
      },
      zones: program.zones,
    })

    showAddModal = false
  }

  function confirmDeleteProgram(program) {
    showDeleteDialog = true
    programToDelete = program
  }

  function deleteProgram() {
    if (programToDelete) {
      sendMessage({ type: 'delete_program', program_id: programToDelete.id })
      showDeleteDialog = false
      programToDelete = null
    }
  }

  function toggleProgram(program) {
    sendMessage({ type: 'enable', program_id: program.id, is_enabled: !program.enabled })
  }

  async function runProgramNow(program) {
    sendMessage({ type: 'test_manual', program_id: program.id, action: 'start' })
  }

  function stopProgramNow(program) {
    sendMessage({ type: 'test_manual', action: 'stop' })
  }

  function calculateNextRun(schedule) {
    // Simplified next run calculation
    const now = new Date()
    const tomorrow = new Date(now)
    tomorrow.setDate(tomorrow.getDate() + 1)
    return tomorrow.toLocaleDateString() + ' ' + schedule.startTime
  }
</script>

<div class="programs-container">
  <div class="flex justify-between items-center gap-2">
    <SectionHeader
      title="Programs"
      subtitle="Automated schedules that control when and how long zones run"
    />
    {#if zones.length !== 0}
      <Button onclick={() => (showAddModal = true)}>
        <Plus />
        <span class="hidden sm:inline">Create Program</span>
      </Button>
    {/if}
  </div>

  {#if loading}
    <div class="flex flex-col gap-6">
      {#each Array(1) as _, i}
        <ProgramCardSkeleton />
      {/each}
    </div>
  {:else if zones.length === 0 && programs.length === 0}
    <Card.Root class="text-center py-12">
      <Card.Content>
        <Droplets class="size-12 mx-auto text-muted-foreground mb-4" />
        <h3 class="text-lg font-semibold mb-2">No Zones Yet</h3>
        <p class="text-muted-foreground mb-4">
          Create your first zone to get started with automated irrigation.
        </p>
      </Card.Content>
    </Card.Root>
  {:else if programs.length === 0}
    <Card.Root class="text-center py-12">
      <Card.Content>
        <Droplets class="size-12 mx-auto text-muted-foreground mb-4" />
        <h3 class="text-lg font-semibold mb-2">No Programs Yet</h3>
        <p class="text-muted-foreground mb-4">
          Create your first watering program to get started with automated irrigation.
        </p>
        <Button onclick={() => (showAddModal = true)}>
          <Plus class="h-4 w-4 mr-2" />
          Creste Your First Program
        </Button>
      </Card.Content>
    </Card.Root>
  {:else}
    <div class="flex flex-col gap-6">
      {#each programs as program (program.id)}
        <ProgramCard
          {program}
          {zones}
          canRun={runningProgram === undefined}
          onenable={toggleProgram}
          onedit={editProgram}
          ondelete={confirmDeleteProgram}
          onrun={runProgramNow}
          onstop={stopProgramNow}
        />
      {/each}
    </div>
  {/if}
</div>

{#if editingProgram || showAddModal}
  <EditProgramModal
    program={editingProgram}
    {zones}
    onsave={({ isEditing, program }) => {
      isEditing ? saveProgram(program) : addNewProgram(program)
    }}
    oncancel={() => (editingProgram ? cancelEdit() : (showAddModal = false))}
    open={editingProgram || showAddModal}
  />
{/if}

<!-- Delete Confirmation Dialog -->
<AlertDialog.Root bind:open={showDeleteDialog}>
  <AlertDialog.Content>
    <AlertDialog.Header>
      <AlertDialog.Title>Delete Program</AlertDialog.Title>
      <AlertDialog.Description>
        Are you sure you want to delete "{programToDelete?.name}"? This action cannot be undone.
      </AlertDialog.Description>
    </AlertDialog.Header>
    <AlertDialog.Footer>
      <AlertDialog.Cancel
        onclick={() => {
          showDeleteDialog = false
          programToDelete = null
        }}
      >
        Cancel
      </AlertDialog.Cancel>
      <AlertDialog.Action
        onclick={deleteProgram}
        class="bg-destructive text-destructive-foreground hover:bg-destructive/90"
      >
        Delete Program
      </AlertDialog.Action>
    </AlertDialog.Footer>
  </AlertDialog.Content>
</AlertDialog.Root>
