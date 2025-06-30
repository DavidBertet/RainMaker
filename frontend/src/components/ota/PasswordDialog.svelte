<!-- Copyright (c) 2025 David Bertet. Licensed under the MIT License. -->

<script>
  import * as Dialog from '$lib/components/ui/dialog'
  import { Button } from '$lib/components/ui/button'
  import { Input } from '$lib/components/ui/input'
  import { Label } from '$lib/components/ui/label'
  import { Lock, Eye, EyeOff } from 'lucide-svelte'

  let { open = false, onSubmit = () => {}, onCancel = () => {} } = $props()

  let password = $state('')
  let showPassword = $state(false)
  let passwordError = $state('')
  // Manage internal state to see closing animation, before forwarding to parent
  let internalOpen = $state(false)

  // Sync internal state with prop
  $effect(() => {
    internalOpen = open
  })

  function submitPassword() {
    passwordError = ''
    onSubmit(password)
  }

  function cancelPassword() {
    password = ''
    passwordError = ''
    // Close dialog with animation, then call onCancel
    internalOpen = false
    setTimeout(onCancel, 150)
  }

  function handleOpenChange(isOpen) {
    if (!isOpen) {
      // User clicked outside or pressed ESC
      internalOpen = false
      setTimeout(onCancel, 150)
    }
  }

  function handlePasswordKeydown(event) {
    if (event.key === 'Enter') {
      submitPassword()
    }
  }

  // Reset state when dialog opens/closes
  $effect(() => {
    if (!open) {
      password = ''
      showPassword = false
      passwordError = ''
    }
  })

  // Expose a method to set error from parent
  export function setError(error) {
    passwordError = error
  }
</script>

<Dialog.Root open={internalOpen} onOpenChange={handleOpenChange}>
  <Dialog.Content class="sm:max-w-md">
    <Dialog.Header>
      <Dialog.Title class="flex items-center gap-2">
        <Lock class="h-5 w-5" />
        Upload Authentication
      </Dialog.Title>
      <Dialog.Description>This upload requires authentication.</Dialog.Description>
    </Dialog.Header>

    <div class="space-y-4">
      <div class="space-y-2">
        <Label for="password">Password</Label>
        <div class="relative">
          <Input
            id="password"
            type={showPassword ? 'text' : 'password'}
            bind:value={password}
            placeholder="Enter OTA password"
            onkeydown={handlePasswordKeydown}
            class="pr-10"
          />
          <Button
            type="button"
            variant="ghost"
            size="sm"
            class="absolute right-0 top-0 h-full px-3 py-2 hover:bg-transparent"
            onclick={() => (showPassword = !showPassword)}
          >
            {#if showPassword}
              <EyeOff class="h-4 w-4" />
            {:else}
              <Eye class="h-4 w-4" />
            {/if}
          </Button>
        </div>
        {#if passwordError}
          <p class="text-sm text-red-600">{passwordError}</p>
        {/if}
      </div>

      <div class="flex justify-end space-x-2">
        <Button variant="outline" onclick={cancelPassword}>Cancel</Button>
        <Button onclick={submitPassword} disabled={!password}>Start Upload</Button>
      </div>
    </div>
  </Dialog.Content>
</Dialog.Root>
