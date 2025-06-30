<!-- Copyright (c) 2025 David Bertet. Licensed under the MIT License. -->

<script>
  import { Badge } from '$lib/components/ui/badge'
  import { Progress } from '$lib/components/ui/progress'

  let { program, zones, currentTime } = $props()

  function getZoneProgression(program, zones, programZone, currentTime) {
    if (!programZone) return 0

    const runningZone = zones.find((z) => z.status === 'running')
    if (!runningZone) return 0

    const runningProgramZoneOrder = program.zones.find((z) => z.id === runningZone.id)?.order || 0

    // Early return if the zone is not running
    if (programZone.order < runningProgramZoneOrder) {
      return 100
    } else if (programZone.order > runningProgramZoneOrder) {
      return 0
    }

    const currentTimeInSeconds = currentTime / 1000
    const progressPercentage =
      ((currentTimeInSeconds - runningZone.lastRun) / (programZone.duration * 60)) * 100
    return Math.min(100, Math.max(0, progressPercentage))
  }

  function getZoneVariant(zone, programZone, program) {
    if (!zone?.enabled) return 'secondary'
    if (program.status === 'running' && zone.status === 'running') return 'default'
    return 'outline'
  }
</script>

<div class="space-y-3">
  <h4 class="text-sm font-medium text-muted-foreground">Zone Sequence</h4>
  <div class="flex flex-col sm:flex-row sm:flex-wrap gap-2">
    {#each program.zones.toSorted((a, b) => a.order - b.order) as programZone (programZone.id)}
      {@const zone = zones.find((z) => z.id === programZone.id)}
      {@const isRunning = program.status === 'running' && zone?.status === 'running'}
      {@const progression = getZoneProgression(program, zones, programZone, currentTime)}

      <div class="relative w-full sm:min-w-0 sm:flex-1">
        <div
          class={`
          relative overflow-hidden flex items-center justify-between p-2 rounded-md border
          ${!zone?.enabled ? 'opacity-50 bg-muted' : ''}
          ${isRunning ? 'border-primary' : 'border-border'}
        `}
        >
          {#if program.status === 'running' && zone?.enabled}
            <div
              class="absolute top-0 left-0 h-full bg-indigo-100 dark:bg-gray-700 transition-all duration-300 ease-in-out rounded-md z-0"
              style="width: {progression}%"
            ></div>
          {/if}

          <div class="relative z-10 flex items-center justify-between w-full">
            <div class="flex items-center gap-2 min-w-0">
              <Badge
                variant="secondary"
                class="flex-shrink-0 w-6 h-6 p-0 text-xs font-bold rounded-full"
              >
                {programZone.order}
              </Badge>
              <span class="text-sm font-medium truncate">
                {zone?.name || `Zone ${programZone.id}`}
              </span>
            </div>
            <Badge
              variant={getZoneVariant(zone, programZone, program)}
              class="text-xs flex-shrink-0"
            >
              {programZone.duration}min
            </Badge>
          </div>
        </div>
      </div>
    {/each}
  </div>
</div>
