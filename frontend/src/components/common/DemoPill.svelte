<script>
  import { onMount } from 'svelte'
  import { Github } from 'lucide-svelte'

  let stars = $state(null)
  let loading = $state(false)

  onMount(async () => {
    try {
      const response = await fetch('https://api.github.com/repos/davidbertet/RainMaker')
      const data = await response.json()
      stars = data.stargazers_count
    } catch (error) {
      console.error('Failed to fetch stars:', error)
    } finally {
      loading = false
    }
  })

  function openGithub() {
    window.location = 'https://github.com/davidbertet/RainMaker'
  }
</script>

<div
  class="fixed bottom-4 right-4 z-50 flex items-center gap-2 bg-black/80 dark:bg-white/80 backdrop-blur-sm rounded-full px-4 py-2 shadow-2xl text-white dark:text-gray-900"
>
  <div class="flex items-center gap-2">
    <div class="w-2 h-2 bg-red-500 rounded-full animate-ping"></div>
    <span class="text-sm font-medium">DEMO</span>
  </div>

  <div class="w-px h-6 bg-gray-600"></div>

  <button
    class=" hover:bg-white/20 rounded-full px-3 py-1 h-8 flex items-center gap-1 cursor-pointer"
    onclick={openGithub}
  >
    <Github class="w-4 h-4" />
    {#if loading}
      <div class="w-4 h-4 border border-white/30 border-t-white rounded-full animate-spin"></div>
    {:else if stars !== null}
      <span class="text-xs font-medium">{stars}</span>
    {:else}
      <span class="text-xs opacity-50">?</span>
    {/if}
  </button>
</div>
