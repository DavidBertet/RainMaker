<!-- Copyright (c) 2025 David Bertet. Licensed under the MIT License. -->

<script>
  import * as Card from '$lib/components/ui/card'
  import { Skeleton } from '$lib/components/ui/skeleton'
  import { Separator } from '$lib/components/ui/separator'
</script>

<Card.Root>
  <Card.Header class="pb-3">
    <div class="flex justify-between pb-1">
      <!-- Zone name -->
      <Skeleton class="h-6 w-32" />
      <!-- Toggle switch -->
      <Skeleton class="h-6 w-8 rounded-full" />
    </div>
    <div class="flex items-center justify-between">
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
  <Card.Content class="space-y-3">
    <!-- Output label -->
    <div class="flex justify-between items-center">
      <Skeleton class="h-5 w-16" />
      <Skeleton class="h-5 w-12" />
    </div>
    <Separator />
    <!-- Last run label -->
    <div class="flex justify-between items-center">
      <Skeleton class="h-5 w-16" />
      <Skeleton class="h-5 w-24" />
    </div>
  </Card.Content>
</Card.Root>
