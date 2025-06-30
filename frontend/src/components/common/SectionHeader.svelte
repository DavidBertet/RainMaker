<!-- Copyright (c) 2025 David Bertet. Licensed under the MIT License. -->

<script>
  let { title, subtitle } = $props()
</script>

<div class="space-y-2 mb-6">
  <h2 class="text-3xl font-bold tracking-tight">{title}</h2>
  {#if subtitle}
    <p class="text-muted-foreground">{subtitle}</p>
  {/if}
</div>
