<!-- Copyright (c) 2025 David Bertet. Licensed under the MIT License. -->

<script>
  import { onDestroy } from 'svelte'
  import * as Card from '$lib/components/ui/card'
  import { Button } from '$lib/components/ui/button'
  import { Switch } from '$lib/components/ui/switch'
  import { Badge } from '$lib/components/ui/badge'
  import { Play, Square, Edit, Trash2 } from 'lucide-svelte'

  import DateLabel from 'src/components/common/DateLabel.svelte'
  import ProgramSchedule from 'src/components/program/ProgramSchedule.svelte'
  import ProgramZones from 'src/components/program/ProgramZones.svelte'

  let { program, zones = [], canRun = true, onenable, onedit, ondelete, onrun, onstop } = $props()

  let currentTime = $state(new Date())
  let timer = null

  $effect(() => {
    if (program.status === 'running') {
      timer = setInterval(() => {
        currentTime = new Date()
      }, 1000)

      return () => clearInterval(timer)
    }
  })

  onDestroy(() => {
    if (timer) clearInterval(timer)
  })

  function getStatusVariant(status, enabled) {
    if (!enabled) return 'secondary'
    switch (status) {
      case 'running':
        return 'default'
      case 'scheduled':
        return 'outline'
      case 'error':
        return 'destructive'
      default:
        return 'secondary'
    }
  }

  function getStatusText(status, enabled) {
    if (!enabled) return 'Disabled'
    switch (status) {
      case 'running':
        return 'Running'
      case 'scheduled':
        return 'Scheduled'
      case 'error':
        return 'Error'
      default:
        return 'Inactive'
    }
  }

  function getTotalDuration(program) {
    return program.zones.reduce((total, zone) => {
      const zoneData = zones.find((z) => z.id === zone.id)
      return zoneData?.enabled ? total + zone.duration : total
    }, 0)
  }
</script>

<Card.Root
  class="transition-all duration-200 hover:shadow-md
   {!program.enabled ? 'opacity-60' : ''}
   {program.status === 'running' ? 'border-blue-500 bg-blue-50 dark:bg-gray-800' : ''}"
>
  <Card.Header>
    <div class="flex justify-between items-center">
      <Card.Title class="text-xl">{program.name}</Card.Title>

      <Switch
        checked={program.enabled}
        onCheckedChange={() => onenable(program)}
        disabled={program.status === 'running'}
      />
    </div>

    <div class="flex justify-between items-center">
      <Badge variant={getStatusVariant(program.status, program.enabled)}>
        {getStatusText(program.status, program.enabled)}
      </Badge>
      <div class="flex gap-2">
        <Button
          variant="ghost"
          size="sm"
          title="Edit program"
          onclick={() => onedit(program)}
          disabled={program.status === 'running'}
        >
          <Edit size={16} />
        </Button>
        {#if program.status === 'running'}
          <Button variant="destructive" size="sm" title="Stop now" onclick={() => onstop(program)}>
            <Square size={16} />
          </Button>
        {:else}
          <Button
            variant="ghost"
            size="sm"
            title="Run now"
            onclick={() => onrun(program)}
            disabled={!program.enabled || !canRun}
          >
            <Play size={16} />
          </Button>
        {/if}
        <Button
          variant="ghost"
          size="sm"
          title="Delete program"
          onclick={() => ondelete(program)}
          disabled={program.status === 'running'}
          class="text-destructive hover:text-destructive"
        >
          <Trash2 size={16} />
        </Button>
      </div>
    </div>
  </Card.Header>

  <Card.Content class="space-y-4">
    <ProgramSchedule {program} totalDuration={getTotalDuration(program)} />

    <ProgramZones {program} {zones} {currentTime} />

    <div class="grid grid-cols-1 md:grid-cols-2 gap-4 text-sm">
      <div class="space-y-1">
        <div class="text-muted-foreground font-medium">Last Run</div>
        <DateLabel class="text-foreground font-medium" timestampSeconds={program.lastRun} />
      </div>
      <div class="space-y-1">
        <div class="text-muted-foreground font-medium">Next Run</div>
        <DateLabel class="text-foreground font-medium" timestampSeconds={program.nextRun} />
      </div>
    </div>
  </Card.Content>
</Card.Root>
