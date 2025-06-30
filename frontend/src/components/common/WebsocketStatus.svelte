<!-- Copyright (c) 2025 David Bertet. Licensed under the MIT License. -->

<script>
  import { Badge } from '$lib/components/ui/badge'
  import { cn } from '$lib/utils'

  import { Cloud, CloudOff } from 'lucide-svelte'

  import { wsState } from 'src/lib/ws.svelte.js'
</script>

<Badge
  variant="outline"
  class={cn(
    'flex items-center gap-2 px-3 py-1 text-sm font-medium transition-all duration-200',
    wsState.isConnected
      ? 'bg-green-100 dark:bg-green-950 text-green-800 dark:text-green-500 border-green-200 hover:bg-green-100'
      : 'bg-red-50 dark:bg-red-950 text-red-800 dark:text-red-500 border-red-200 hover:bg-red-100',
  )}
>
  <div
    class={cn(
      'h-2 w-2 rounded-full transition-all duration-200',
      wsState.isConnected ? 'bg-green-500 animate-pulse' : 'bg-red-500',
    )}
  ></div>
  <span class="text-s">
    {#if wsState.isConnected}<Cloud />{:else}<CloudOff />{/if}
  </span>
</Badge>

<style>
  @keyframes pulse {
    0%,
    100% {
      opacity: 1;
    }
    50% {
      opacity: 0.5;
    }
  }

  .animate-pulse {
    animation: pulse 2s infinite;
  }
</style>
