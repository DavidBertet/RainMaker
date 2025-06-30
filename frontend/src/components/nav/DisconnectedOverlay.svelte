<!-- Copyright (c) 2025 David Bertet. Licensed under the MIT License. -->

<script>
  import { CloudOff, RefreshCw, AlertTriangle } from 'lucide-svelte'
</script>

<div
  class="fixed inset-0 z-[100] bg-black/50 backdrop-blur-sm flex items-center justify-center p-4"
>
  <div
    class="bg-background border border-border rounded-xl shadow-2xl max-w-md w-full p-8 text-center space-y-6 animate-in fade-in-0 zoom-in-95 duration-300"
  >
    <div class="flex justify-center">
      <div class="w-16 h-16 rounded-full bg-destructive/10 flex items-center justify-center">
        <CloudOff size={32} class="text-destructive" />
      </div>
    </div>

    <!-- Title and Message -->
    <div class="space-y-2">
      <h2 class="text-xl font-semibold text-foreground">Connection Lost</h2>
      <p class="text-sm text-muted-foreground">
        Unable to connect to RainMaker device. Please check your connection and try again.
      </p>
    </div>

    <!-- Connection Status -->
    <div class="flex items-center justify-center gap-2 text-sm text-muted-foreground">
      <div class="w-2 h-2 rounded-full bg-destructive animate-pulse"></div>
      <span>Disconnected</span>
    </div>

    <div class="flex items-center justify-center gap-2">
      <RefreshCw size={16} class="animate-spin" /> Reconnecting...
    </div>

    <!-- Additional Help -->
    <div class="text-xs text-muted-foreground space-y-1">
      <p class="flex items-center justify-center gap-1">
        <AlertTriangle size={12} />
        Check device power and network connection
      </p>
    </div>
  </div>
</div>
