<!-- Copyright (c) 2025 David Bertet. Licensed under the MIT License. -->

<script>
  import { Button } from '$lib/components/ui/button'
  import { Upload } from 'lucide-svelte'
  import UploadDialog from 'src/components/ota/UploadDialog.svelte'

  let { ...restProps } = $props()

  let selectedFiles = $state(null)
  let isUploadBounce = $state(false)
  let fileInputRef = $state(null)

  function resetSelectedFile() {
    selectedFiles = null
  }

  function handleFileUpload(event) {
    console.log(event)
    selectedFiles = Array.from(event.target.files)
    event.target.value = ''
  }

  function handleDrop(event) {
    event.preventDefault()
    console.log(event)
    selectedFiles = Array.from(event.dataTransfer.files)
    if (fileInputRef) fileInputRef.value = ''
    isUploadBounce = false
  }

  function handleDragOver(event) {
    event.preventDefault()
  }

  function handleDragEnter(event) {
    event.preventDefault()
    console.log(event)
    isUploadBounce = true
  }

  function handleDragLeave(event) {
    event.preventDefault()
    console.log(event)
    isUploadBounce = false
  }

  function openFileDialog() {
    fileInputRef?.click()
  }
</script>

<div {...restProps}>
  <Button
    variant="ghost"
    size="icon"
    aria-label="Upload to SPIFF"
    onclick={openFileDialog}
    ondrop={handleDrop}
    ondragover={handleDragOver}
    ondragenter={handleDragEnter}
    ondragleave={handleDragLeave}
    class="relative overflow-hidden transition-all duration-200 {isUploadBounce
      ? 'bg-primary/20 scale-105'
      : ''}"
  >
    <input
      bind:this={fileInputRef}
      type="file"
      onchange={handleFileUpload}
      class="sr-only"
      multiple
    />
    <Upload
      class="h-4 w-4 text-muted-foreground transition-transform duration-200 {isUploadBounce
        ? 'animate-bounce'
        : ''}"
    />
  </Button>
</div>

{#if selectedFiles}
  <UploadDialog files={selectedFiles} onUploadComplete={resetSelectedFile} />
{/if}
