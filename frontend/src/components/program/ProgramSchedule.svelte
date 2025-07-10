<!-- Copyright (c) 2025 David Bertet. Licensed under the MIT License. -->

<script>
  import { Badge } from '$lib/components/ui/badge'
  import { Clock, Calendar } from 'lucide-svelte'

  let { program, totalDuration } = $props()

  const dayOptions = ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun']

  function formatScheduleDays(days) {
    if (days.length === 7) return 'Daily'
    if (days.length === 0) return 'No days selected'

    return days
      .map((d) => (7 + d - 1) % 7) // Shift days from US based (sun is 0)
      .toSorted()
      .map((d) => dayOptions[d])
      .join(', ')
  }
</script>

<div class="flex items-center justify-between p-3 bg-muted/50 rounded-lg">
  <div class="flex items-center gap-4">
    <div class="flex items-center gap-2 text-sm">
      <Calendar class="w-4 h-4 text-muted-foreground" />
      <span class="font-medium">{formatScheduleDays(program.schedule.days)}</span>
    </div>
    <div class="flex items-center gap-2 text-sm text-muted-foreground">
      <Clock class="w-4 h-4" />
      <span>at {program.schedule.startTime}</span>
    </div>
  </div>
  <Badge variant="secondary" class="hidden sm:inline text-primary">
    Total: {totalDuration} min
  </Badge>
</div>
