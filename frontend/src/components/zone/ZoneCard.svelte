<!-- Copyright (c) 2025 David Bertet. Licensed under the MIT License. -->

<script>
  import * as Card from '$lib/components/ui/card'
  import { Button } from '$lib/components/ui/button'
  import { Switch } from '$lib/components/ui/switch'
  import { Badge } from '$lib/components/ui/badge'
  import { Separator } from '$lib/components/ui/separator'

  import { Edit, Trash2, Square, Droplets } from 'lucide-svelte'

  import DateLabel from 'src/components/common/DateLabel.svelte'

  let { zone, oneIsRunning, onenable, onedit, ondelete, onrun, onstop } = $props()

  function getBadgeVariant(status) {
    switch (status) {
      case 'running':
        return 'default'
      case 'testing':
        return 'secondary'
      case 'disabled':
        return 'outline'
      default:
        return 'outline'
    }
  }

  function getStatusText(status) {
    switch (status) {
      case 'running':
        return 'Running'
      case 'testing':
        return 'Testing'
      case 'disabled':
        return 'Disabled'
      default:
        return 'Idle'
    }
  }
</script>

<Card.Root
  class="transition-all duration-200 hover:shadow-md 
  {zone.status === 'running' || zone.status === 'testing'
    ? 'border-blue-500 bg-blue-50 dark:bg-gray-800'
    : zone.status === 'disabled'
      ? 'opacity-60'
      : ''}"
>
  <Card.Header class="pb-3">
    <div class="flex justify-between items-center">
      <Card.Title class="text-xl">{zone.name}</Card.Title>

      <Switch
        checked={zone.status != 'disabled'}
        onCheckedChange={() => onenable(zone)}
        disabled={zone.status === 'running' || zone.status === 'testing'}
      />
    </div>

    <div class="flex justify-between items-center">
      <Badge variant={getBadgeVariant(zone.status)}>
        {getStatusText(zone.status)}
      </Badge>

      <div class="flex items-center space-x-2">
        <Button
          variant="ghost"
          size="sm"
          title="Edit zone"
          onclick={() => onedit(zone)}
          disabled={zone.status === 'running' || zone.status === 'testing'}
        >
          <Edit size={16} />
        </Button>
        {#if zone.status === 'testing'}
          <Button variant="destructive" size="sm" title="Stop test" onclick={() => onstop(zone)}>
            <Square size={16} />
          </Button>
        {:else}
          <Button
            variant="ghost"
            size="sm"
            title="Start testing zone (30s)"
            onclick={() => onrun(zone)}
            disabled={zone.status === 'disabled' || oneIsRunning}
          >
            <Droplets size={16} />
          </Button>
        {/if}
        <Button
          variant="ghost"
          size="sm"
          title="Delete zone"
          onclick={() => ondelete(zone)}
          disabled={zone.status === 'running' || zone.status === 'testing'}
          class="text-destructive hover:text-destructive"
        >
          <Trash2 size={16} />
        </Button>
      </div>
    </div>
  </Card.Header>

  <Card.Content class="space-y-3">
    <div class="flex justify-between items-center">
      <span class="text-sm text-muted-foreground">Output:</span>
      <span class="text-sm font-medium">Pin {zone.output}</span>
    </div>
    <Separator />
    <div class="flex justify-between items-center">
      <span class="text-sm text-muted-foreground">Last Run:</span>
      <DateLabel timestampSeconds={zone.lastRun} class="text-sm font-medium" />
    </div>
  </Card.Content>
</Card.Root>
