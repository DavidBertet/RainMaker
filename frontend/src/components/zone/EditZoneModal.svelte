<!-- Copyright (c) 2025 David Bertet. Licensed under the MIT License. -->

<script>
  import * as Dialog from '$lib/components/ui/dialog'
  import { Input } from '$lib/components/ui/input'
  import { Label } from '$lib/components/ui/label'
  import { Button } from '$lib/components/ui/button'

  // Props
  let {
    open = $bindable(false),
    zone = null, // null for add mode, zone object for edit mode
    onsave,
    ...restProps
  } = $props()

  // Reactive state
  let formData = $state({
    name: '',
    output: 0,
  })

  // Computed properties
  let isEditMode = $derived(zone !== null)
  let dialogTitle = $derived(isEditMode ? `Edit Zone: ${zone?.name}` : 'Create New Zone')
  let submitButtonText = $derived(isEditMode ? 'Save Changes' : 'Create Zone')
  let isFormValid = $derived(
    formData.name.trim().length > 0 && formData.output && formData.output != 0,
  )

  // Watch for zone changes to update form data
  $effect(() => {
    if (zone) {
      formData.name = zone.name
      formData.output = zone.output
    } else {
      formData.name = ''
      formData.output = 0
    }
  })

  function handleCancel() {
    console.log('handleCancel')
    open = false
    resetForm()
  }

  function handleSubmit() {
    if (!isFormValid) return

    var newZone = { name: formData.name, output: formData.output }
    if (zone) {
      newZone.zone_id = zone.id
    }
    onsave(newZone)

    open = false
    resetForm()
  }

  function resetForm() {
    formData.name = ''
    formData.output = 0
  }
</script>

<Dialog.Root bind:open {...restProps}>
  <Dialog.Content class="sm:max-w-md">
    <Dialog.Header>
      <Dialog.Title>{dialogTitle}</Dialog.Title>
    </Dialog.Header>

    <div class="space-y-4">
      <div class="space-y-2">
        <Label for="zone-name">Zone Name</Label>
        <Input id="zone-name" bind:value={formData.name} placeholder="Enter zone name" />
      </div>

      <div class="space-y-2">
        <Label for="zone-output">Output Pin</Label>
        <Input
          id="zone-output"
          type="number"
          bind:value={formData.output}
          placeholder="Enter output pin number"
        />
      </div>
    </div>

    <Dialog.Footer>
      <Button variant="outline" onclick={handleCancel}>Cancel</Button>
      <Button onclick={handleSubmit} disabled={!isFormValid}>
        {submitButtonText}
      </Button>
    </Dialog.Footer>
  </Dialog.Content>
</Dialog.Root>
