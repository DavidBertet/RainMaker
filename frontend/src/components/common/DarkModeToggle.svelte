<!-- Copyright (c) 2025 David Bertet. Licensed under the MIT License. -->

<script>
  import { onMount } from 'svelte'
  import { Button } from '$lib/components/ui/button'
  import { Sun, Moon } from 'lucide-svelte'

  let isDarkMode = $state(false)

  // Initialize dark mode from system preference or localStorage
  onMount(() => {
    // Check if user has a saved preference
    const savedTheme = localStorage.getItem('theme')
    if (savedTheme) {
      isDarkMode = savedTheme === 'dark'
    } else {
      // Check system preference
      isDarkMode = window.matchMedia('(prefers-color-scheme: dark)').matches
    }

    // Apply the theme
    if (isDarkMode) {
      document.documentElement.classList.add('dark')
    }
  })

  // Save theme preference when it changes
  $effect(() => {
    localStorage.setItem('theme', isDarkMode ? 'dark' : 'light')
  })

  // Dark mode toggle function
  function toggleDarkMode() {
    isDarkMode = !isDarkMode
    if (isDarkMode) {
      document.documentElement.classList.add('dark')
    } else {
      document.documentElement.classList.remove('dark')
    }
  }
</script>

<Button
  variant="ghost"
  size="icon"
  onclick={toggleDarkMode}
  class="h-9 w-9 p-0"
  aria-label="Toggle dark mode"
>
  {#if isDarkMode}
    <Sun size={16} class="text-muted-foreground hover:text-foreground transition-colors" />
  {:else}
    <Moon size={16} class="text-muted-foreground hover:text-foreground transition-colors" />
  {/if}
</Button>
