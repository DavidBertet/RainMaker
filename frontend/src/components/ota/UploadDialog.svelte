<!-- Copyright (c) 2025 David Bertet. Licensed under the MIT License. -->

<script>
  import { onMount } from 'svelte'

  import * as Dialog from '$lib/components/ui/dialog'
  import { Button } from '$lib/components/ui/button'
  import { Progress } from '$lib/components/ui/progress'
  import * as Card from '$lib/components/ui/card'

  import { CheckCircle, AlertCircle, Clock, Upload, Lock } from 'lucide-svelte'

  import { wsState, onMessageType } from 'src/lib/ws.svelte.js'
  import { settingsState } from 'src/lib/settings.svelte.js'
  import PasswordDialog from './PasswordDialog.svelte'

  let { files, onUploadComplete = () => {} } = $props()

  let uploadProgress = $state(0)
  let uploading = $state(false)
  let done = $state(false)
  let showPasswordDialog = $state(false)
  let password = $state('')
  let passwordDialogRef

  // Disable ping pong during upload
  $effect(() => (wsState.pingPaused = uploading))

  var fileStatuses = $state([])

  async function uploadFile() {
    if (!files) return

    onMessageType('upload_progress', (data) => {
      uploadProgress = Math.round((data.loaded * 100) / data.total)
    })

    uploading = true
    for (const index in fileStatuses) {
      fileStatuses[index].pending = true
      try {
        const response = await new Promise((resolve, reject) => {
          if (fileStatuses[index].file.size > 1000 * 1024) {
            reject('Must be less than 1MB!')
          } else {
            var xhttp = new XMLHttpRequest()
            xhttp.onreadystatechange = function () {
              if (xhttp.readyState == 4) {
                if (xhttp.status == 200) {
                  resolve(xhttp.responseText)
                } else if (xhttp.status == 401) {
                  reject('Authentication failed - invalid password')
                } else {
                  reject('Upload failed')
                }
              }
            }
            xhttp.open('POST', '/upload/' + fileStatuses[index].name, true)
            xhttp.setRequestHeader('X-OTA-Password', password)
            xhttp.send(fileStatuses[index].file)
          }
        })
        fileStatuses[index].message = response
      } catch (error) {
        fileStatuses[index].message = error
        // If it's an auth error, show password dialog again
        if (error.includes('Authentication failed')) {
          uploading = false
          showPasswordDialog = true
          if (passwordDialogRef) {
            passwordDialogRef.setError(error)
          }
          return
        }
      }
      uploadProgress = 0
    }
    done = true
  }

  function handlePasswordSubmit(submittedPassword) {
    password = submittedPassword
    showPasswordDialog = false
    // Clear any previous error messages from file statuses
    fileStatuses.forEach((file) => {
      if (file.message && file.message.includes('Authentication failed')) {
        file.message = undefined
        file.pending = false
      }
    })
    uploadFile()
  }

  function handlePasswordCancel() {
    showPasswordDialog = false
    password = ''
    onUploadComplete()
  }

  function resetState() {
    uploading = false
    showPasswordDialog = false
    password = ''
    onUploadComplete()
    window.location.reload()
  }

  onMount(() => {
    fileStatuses = files.map((file) => {
      return {
        name: file.name,
        pending: false,
        message: undefined,
        file: file,
      }
    })
    if (settingsState.ota.requiresPassword) {
      showPasswordDialog = true
    } else {
      uploadFile()
    }
  })
</script>

<!-- Password Dialog for OTA uploads -->
<PasswordDialog
  bind:this={passwordDialogRef}
  open={showPasswordDialog}
  onSubmit={handlePasswordSubmit}
  onCancel={handlePasswordCancel}
/>

<!-- Upload Progress Dialog -->
<Dialog.Root open={uploading}>
  <Dialog.Content
    class="sm:max-w-md [&_[data-dialog-close]]:hidden"
    escapeKeydownBehavior="ignore"
    interactOutsideBehavior="ignore"
  >
    <Dialog.Header>
      <Dialog.Title class="flex items-center gap-2">
        <Upload class="h-5 w-5" />
        Uploading Files...
      </Dialog.Title>
    </Dialog.Header>

    <div class="space-y-4">
      {#each fileStatuses as file}
        <Card.Root class="p-0">
          <Card.Content class="py-3 px-4">
            <div class="flex items-start justify-between">
              <div class="flex-1 min-w-0">
                <p class="text-sm font-medium text-gray-900 dark:text-gray-100 truncate">
                  {file.name}
                  <span
                    class="ml-2 inline-flex items-center px-2 py-0.5 rounded text-xs font-medium bg-orange-100 text-orange-800 dark:bg-orange-900 dark:text-orange-200"
                  >
                    <Lock class="h-3 w-3 mr-1" />
                  </span>
                </p>

                {#if file.message != undefined}
                  <div class="mt-2 flex items-center gap-2">
                    {#if file.message.includes('failed') || file.message.includes('Must be') || file.message.includes('Authentication failed')}
                      <AlertCircle class="h-4 w-4 text-red-500" />
                      <p class="text-sm text-red-600">{file.message}</p>
                    {:else}
                      <CheckCircle class="h-4 w-4 text-green-500" />
                      <p class="text-sm text-green-600">{file.message}</p>
                    {/if}
                  </div>
                {:else if !file.pending}
                  <div class="mt-2 flex items-center gap-2">
                    <Clock class="h-4 w-4 text-gray-400" />
                    <p class="text-sm text-gray-500">Waiting...</p>
                  </div>
                {:else}
                  <div class="mt-3 space-y-2">
                    <Progress value={uploadProgress} class="w-full" />
                    <p class="text-sm text-gray-600 text-center">{uploadProgress}%</p>
                  </div>
                {/if}
              </div>
            </div>
          </Card.Content>
        </Card.Root>
      {/each}

      <div class="flex justify-center">
        <Button onclick={resetState} class="w-full sm:w-auto" disabled={!done}>OK</Button>
      </div>
    </div>
  </Dialog.Content>
</Dialog.Root>
