<!-- Copyright (c) 2025 David Bertet. Licensed under the MIT License. -->

<script>
  import * as Card from '$lib/components/ui/card'
  import { Skeleton } from '$lib/components/ui/skeleton'
</script>

<Card.Root class="transition-all duration-200">
  <Card.Header>
    <div class="flex items-start justify-between pb-1">
      <!-- Program name -->
      <Skeleton class="h-6 w-48" />
      <!-- Toggle switch -->
      <Skeleton class="h-6 w-8 rounded-full" />
    </div>
    <div class="flex items-start justify-between">
      <!-- Status badge -->
      <Skeleton class="h-5 w-16" />
      <!-- Action buttons -->
      <div class="flex gap-3">
        <Skeleton class="h-8 w-8" />
        <Skeleton class="h-8 w-8" />
        <Skeleton class="h-8 w-8" />
      </div>
    </div>
  </Card.Header>

  <Card.Content class="space-y-4">
    <!-- Schedule section -->
    <div class="flex items-center justify-between p-3 bg-muted/50 rounded-lg">
      <div class="flex items-center gap-4">
        <div class="flex items-center gap-2">
          <Skeleton class="h-4 w-4" />
          <Skeleton class="h-4 w-24" />
        </div>
        <div class="flex items-center gap-2">
          <Skeleton class="h-4 w-4" />
          <Skeleton class="h-4 w-16" />
        </div>
      </div>
      <Skeleton class="h-5 w-16" />
    </div>

    <!-- Zones section -->
    <div class="space-y-3">
      <Skeleton class="h-4 w-24" />
      <div class="flex flex-col sm:flex-row sm:flex-wrap gap-2">
        <!-- Zone items -->
        {#each Array(3) as _, i}
          <div class="relative w-full sm:min-w-0 sm:flex-1">
            <div
              class="relative overflow-hidden flex items-center justify-between p-2 rounded-md border"
            >
              <div class="relative z-10 flex items-center justify-between w-full">
                <div class="flex items-center gap-2 min-w-0">
                  <!-- Zone order badge -->
                  <Skeleton class="w-6 h-6 rounded-full" />
                  <!-- Zone name -->
                  <Skeleton class="h-4 w-20" />
                </div>
                <!-- Duration badge -->
                <Skeleton class="h-5 w-12" />
              </div>
            </div>
          </div>
        {/each}
      </div>
    </div>

    <!-- Times section -->
    <div class="grid grid-cols-1 md:grid-cols-2 gap-4">
      <div class="space-y-3">
        <Skeleton class="h-5 w-16" />
        <Skeleton class="h-5 w-28" />
      </div>
      <div class="space-y-3">
        <Skeleton class="h-5 w-16" />
        <Skeleton class="h-5 w-28" />
      </div>
    </div>
  </Card.Content>
</Card.Root>
