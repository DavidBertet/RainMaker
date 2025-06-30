<!-- Copyright (c) 2025 David Bertet. Licensed under the MIT License. -->

<script>
  import { Button } from '$lib/components/ui/button'
  import { Input } from '$lib/components/ui/input'
  import { Label } from '$lib/components/ui/label'
  import * as Dialog from '$lib/components/ui/dialog'
  import { Badge } from '$lib/components/ui/badge'
  import * as Card from '$lib/components/ui/card'
  import { Separator } from '$lib/components/ui/separator'
  import { GripVertical, Plus, X } from 'lucide-svelte'

  import ZoneSelect from 'src/components/program/ZoneSelect.svelte'
  import DaysSelector from 'src/components/common/DaysSelector.svelte'

  // Props
  let { program = null, zones = [], oncancel, onsave, open = $bindable(true) } = $props()

  // State
  let draggedZone = $state(null)
  let dragOverIndex = $state(-1)

  // Create a working copy of the program
  let workingProgram = $state(
    program
      ? {
          ...program,
          schedule: { ...program.schedule, days: [...program.schedule.days] },
          zones: program.zones.map((z) => ({ ...z })),
        }
      : {
          name: '',
          schedule: {
            days: [],
            startTime: '06:00',
          },
          zones: [],
        },
  )

  let isEditing = $derived(program !== null)
  let isFormValid = $derived(
    workingProgram.name.trim() &&
      workingProgram.schedule.days.length > 0 &&
      workingProgram.zones.length > 0,
  )

  function getTotalDuration(program) {
    return program.zones.reduce((total, zone) => total + zone.duration, 0)
  }

  function addZoneToProgram() {
    const unusedZones = zones.filter((z) => !workingProgram.zones.some((pz) => pz.id === z.id))

    if (unusedZones.length > 0) {
      const newZone = unusedZones[0]
      const maxOrder = Math.max(0, ...workingProgram.zones.map((z) => z.order))
      workingProgram.zones.push({
        id: newZone.id,
        duration: 10,
        order: maxOrder + 1,
      })
    }
  }

  function removeZoneFromProgram(id) {
    workingProgram.zones = workingProgram.zones.filter((z) => z.id !== id)
    updateZoneOrders()
  }

  function updateZoneOrders() {
    workingProgram.zones.forEach((zone, index) => {
      zone.order = index + 1
    })
  }

  function updateZoneDuration(zoneConfig, newDuration) {
    zoneConfig.duration = parseInt(newDuration) || 0
  }

  // Drag and Drop zone functions
  function handleDragStart(event, zone, index) {
    draggedZone = { zone, index }
    event.dataTransfer.effectAllowed = 'move'
    event.dataTransfer.setData('text/html', event.target.outerHTML)
    event.target.style.opacity = '0.5'
  }

  function handleDragEnd(event) {
    event.target.style.opacity = '1'
    draggedZone = null
    dragOverIndex = -1
  }

  function handleDragOver(event, index) {
    event.preventDefault()
    dragOverIndex = index
  }

  function handleDragLeave() {
    dragOverIndex = -1
  }

  function handleDrop(event, targetIndex) {
    event.preventDefault()

    if (draggedZone && draggedZone.index !== targetIndex) {
      const zones = [...workingProgram.zones]
      const draggedItem = zones.splice(draggedZone.index, 1)[0]
      zones.splice(targetIndex, 0, draggedItem)

      workingProgram.zones = zones
      updateZoneOrders()
    }

    draggedZone = null
    dragOverIndex = -1
  }

  function handleSave() {
    onsave({
      program: workingProgram,
      isEditing,
    })
  }

  function handleCancel() {
    oncancel()
  }

  function getZoneById(id) {
    return zones.find((z) => z.id === id)
  }
</script>

<Dialog.Root bind:open onOpenChange={(isOpen) => !isOpen && handleCancel()}>
  <Dialog.Content class="!w-[700px] md:!max-w-[90vw] max-h-[90vh] overflow-y-auto">
    <Dialog.Header>
      <Dialog.Title>
        {isEditing ? `Edit Program: ${program.name}` : 'Add New Program'}
      </Dialog.Title>
    </Dialog.Header>

    <div class="space-y-6">
      <!-- Program Name -->
      <div class="space-y-2">
        <Label for="program-name">Program Name</Label>
        <Input
          id="program-name"
          bind:value={workingProgram.name}
          placeholder="Enter program name"
        />
      </div>

      <Separator />

      <!-- Schedule Section -->
      <div class="space-y-4">
        <h4 class="text-lg font-semibold">Schedule</h4>

        <!-- Days Selector -->
        <DaysSelector bind:selectedDays={workingProgram.schedule.days} />

        <!-- Start Time -->
        <div class="space-y-2">
          <Label for="start-time">Start Time</Label>
          <Input id="start-time" type="time" bind:value={workingProgram.schedule.startTime} />
        </div>
      </div>

      <Separator />

      <!-- Zones Section -->
      <div class="space-y-4">
        <div class="flex justify-between items-center">
          <h4 class="text-lg font-semibold">Zone Sequence</h4>
          <Button
            variant="outline"
            size="sm"
            onclick={addZoneToProgram}
            disabled={workingProgram.zones.length >= zones.length}
          >
            <Plus class="h-4 w-4 mr-2" />
            Create Zone
          </Button>
        </div>

        {#if workingProgram.zones.length === 0}
          <Card.Root>
            <Card.Content class="pt-6">
              <p class="text-center text-muted-foreground">
                No zones added yet. Click "Add Zone" to start building your program.
              </p>
            </Card.Content>
          </Card.Root>
        {:else}
          <div class="space-y-2">
            {#each workingProgram.zones.toSorted((a, b) => a.order - b.order) as zone, index (`${index}-${zone.id}`)}
              <Card.Root
                class="transition-all duration-200 py-4 {dragOverIndex === index
                  ? 'border-primary bg-primary/5'
                  : ''}"
                draggable="true"
                ondragstart={(e) => handleDragStart(e, zone, index)}
                ondragend={handleDragEnd}
                ondragover={(e) => handleDragOver(e, index)}
                ondragleave={handleDragLeave}
                ondrop={(e) => handleDrop(e, index)}
              >
                <Card.Content>
                  <div class="grid grid-cols-[auto_1fr_auto_auto] gap-4 items-center">
                    <!-- Drag Handle -->
                    <div
                      class="flex flex-col items-center gap-1 cursor-grab active:cursor-grabbing"
                    >
                      <GripVertical class="h-4 w-4 text-muted-foreground" />
                    </div>

                    <!-- Zone Selection -->
                    <div class="space-x-2 flex flex-row">
                      <Label class="text-sm">Zone</Label>
                      <ZoneSelect
                        bind:zoneId={zone.id}
                        programZones={workingProgram.zones}
                        {zones}
                      />
                    </div>

                    <!-- Duration -->
                    <div class="space-x-2 flex flex-row">
                      <Label class="text-sm">Duration</Label>
                      <div class="flex items-center gap-2">
                        <Input
                          type="number"
                          value={zone.duration}
                          oninput={(e) => updateZoneDuration(zone, e.target.value)}
                          class="w-20"
                          min="1"
                        />
                        <span class="text-sm text-muted-foreground">min</span>
                      </div>
                    </div>

                    <!-- Remove Button -->
                    <Button
                      variant="destructive"
                      size="sm"
                      onclick={() => removeZoneFromProgram(zone.id)}
                    >
                      <X class="h-4 w-4" />
                    </Button>
                  </div>
                </Card.Content>
              </Card.Root>
            {/each}
          </div>

          <!-- Total Duration -->
          <Card.Root class="bg-blue-50 border-blue-200 dark:bg-blue-950 py-4">
            <Card.Content>
              <p class="text-center font-semibold text-blue-900 dark:text-blue-50">
                Total Duration: {getTotalDuration(workingProgram)} minutes
              </p>
            </Card.Content>
          </Card.Root>
        {/if}
      </div>
    </div>

    <Dialog.Footer>
      <Button variant="outline" onclick={handleCancel}>Cancel</Button>
      <Button onclick={handleSave} disabled={!isFormValid}>
        {isEditing ? 'Save Changes' : 'Create Program'}
      </Button>
    </Dialog.Footer>
  </Dialog.Content>
</Dialog.Root>

<style>
  /* Custom styles for drag and drop */
  :global(.drag-handle:active) {
    cursor: grabbing !important;
  }

  /* Responsive adjustments */
  @media (max-width: 768px) {
    :global(.grid-cols-\[auto_1fr_auto_auto\]) {
      grid-template-columns: 1fr;
      gap: 1rem;
      text-align: center;
    }
  }
</style>
