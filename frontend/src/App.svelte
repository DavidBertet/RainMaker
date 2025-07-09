<!-- Copyright (c) 2025 David Bertet. Licensed under the MIT License. -->

<script>
  import DeviceTab from 'src/components/tab/DeviceTab.svelte'
  import LogsTab from 'src/components/tab/LogsTab.svelte'
  import SystemTab from 'src/components/tab/SystemTab.svelte'
  import ZonesTab from 'src/components/tab/ZonesTab.svelte'
  import ProgramsTab from 'src/components/tab/ProgramsTab.svelte'
  import WifiTab from 'src/components/tab/WifiTab.svelte'
  import WebsocketStatus from 'src/components/common/WebsocketStatus.svelte'
  import SystemStatus from 'src/components/nav/SystemStatus.svelte'
  import UploadButton from 'src/components/ota/UploadButton.svelte'
  import SidebarNav from 'src/components/nav/SidebarNav.svelte'
  import DarkModeToggle from 'src/components/common/DarkModeToggle.svelte'
  import DisconnectedOverlay from 'src/components/nav/DisconnectedOverlay.svelte'
  import DemoPill from 'src/components/common/DemoPill.svelte'

  import { onMount, onDestroy } from 'svelte'
  import {
    wsState,
    connectWebSocket,
    closeWebSocket,
    sendMessage,
    onMessageType,
  } from 'src/lib/ws.svelte.js'
  import { settingsState, initializeSettings } from 'src/lib/settings.svelte.js'

  import * as Sidebar from '$lib/components/ui/sidebar'
  import * as AlertDialog from '$lib/components/ui/alert-dialog'
  import { cn } from '$lib/utils'
  import {
    Target,
    Calendar,
    Wifi,
    Info,
    FileText,
    Settings,
    Menu,
    Droplets,
    Heart,
  } from 'lucide-svelte'

  const isDev = import.meta.env.DEV

  let activeTab = $state(new URLSearchParams(window.location.search).get('tab') || 'zones')
  let errorMessage = $state(null)
  let errorUnsub = $state(null)
  let settingsUnsub = $state(null)

  let delayDisconnectedState = $state(true)
  const shouldShowDisconnected = $derived(!wsState.isConnected && !delayDisconnectedState)

  let systemBannerVisible = $state(false)

  onMount(() => {
    let wsUrl
    if (isDev) {
      wsUrl = `ws://localhost:8080`
    } else {
      const wsHost = window.location.hostname
      const wsPort = window.location.port || '80'
      wsUrl = `ws://${wsHost}:${wsPort}/ws`
    }
    connectWebSocket(wsUrl)

    // Slight delay to avoid showing blurry animation on opening
    setTimeout(() => {
      delayDisconnectedState = false
    }, 500)

    sendMessage({ type: 'time_update', time: (Date.now() / 1000) | 0 })

    // Initialize settings
    settingsUnsub = initializeSettings()

    errorUnsub = onMessageType('error', (data) => {
      errorMessage = data.message || 'Unknow error'
    })

    return () => {
      if (errorUnsub) errorUnsub()
      if (settingsUnsub) settingsUnsub()
    }
  })

  onDestroy(() => {
    closeWebSocket()
    if (errorUnsub) errorUnsub()
    if (settingsUnsub) settingsUnsub()
  })

  // Retrieve current tab from URL
  $effect(() => {
    const url = new URL(window.location)
    url.searchParams.set('tab', activeTab)
    window.history.replaceState({}, '', url)
  })

  const tabs = [
    {
      id: 'zones',
      label: 'Zones',
      icon: Target,
      component: ZonesTab,
    },
    {
      id: 'programs',
      label: 'Programs',
      icon: Calendar,
      component: ProgramsTab,
    },
    {
      id: 'wifi',
      label: 'WiFi Settings',
      icon: Wifi,
      component: WifiTab,
    },
    {
      id: 'system',
      label: 'System Info',
      icon: Info,
      component: SystemTab,
    },
  ]

  function setActiveTab(tab) {
    activeTab = tab
  }

  const activeTabData = $derived(tabs.find((tab) => tab.id === activeTab))
</script>

{#if shouldShowDisconnected}
  <DisconnectedOverlay />
{/if}

<div
  class={cn(
    'transition-all duration-300',
    shouldShowDisconnected && 'blur-sm pointer-events-none select-none',
  )}
>
  <Sidebar.Provider>
    <Sidebar.Root class="border-r border-border/40">
      <Sidebar.Content
        class="bg-gradient-to-b from-slate-50 to-white dark:from-slate-900 dark:to-slate-800"
      >
        <!-- Sidebar Header -->
        <div class="flex items-center gap-3 px-6 py-4 border-b border-border/40">
          <div class="flex items-center justify-center w-8 h-8 rounded-lg bg-blue-500 text-white">
            <Droplets class="size-5" />
          </div>
          <div class="flex flex-col">
            <span class="font-semibold text-sm text-foreground">RainMaker</span>
            <span class="text-xs text-muted-foreground">Control Panel</span>
          </div>
        </div>

        <SidebarNav {tabs} {activeTab} onTabSelect={setActiveTab} />
      </Sidebar.Content>
    </Sidebar.Root>

    <main class="flex-1 flex flex-col min-h-screen">
      <SystemStatus />

      <div class="bg-background">
        <!-- Header -->
        <header
          class="sticky top-0 z-50 w-full border-b bg-background/95 backdrop-blur supports-[backdrop-filter]:bg-background/60"
        >
          <div class="flex h-14 items-center px-6">
            <div class="md:hidden flex items-center gap-4 mr-4">
              <Sidebar.Trigger />
              <div class="flex items-center gap-2">
                <div class="size-6 rounded bg-blue-500 flex items-center justify-center">
                  <Droplets class="size-4 text-white" />
                </div>
                <h1 class="text-lg font-semibold">RainMaker</h1>
              </div>
            </div>

            <div class="flex flex-1 justify-end items-center gap-3">
              <DarkModeToggle />
              <UploadButton />
              <WebsocketStatus />
            </div>
          </div>
        </header>

        <!-- Main Content -->
        <div class="flex-1 p-6">
          <div class="max-w-7xl mx-auto">
            {#if activeTabData}
              <activeTabData.component />
            {/if}
          </div>
        </div>

        <!-- Footer -->
        <footer>
          <div class="max-w-7xl mx-auto px-6 py-4">
            <div class="flex items-center justify-center text-sm text-muted-foreground">
              <span class="flex items-center gap-1">Built with <Heart class="size-3" /> by</span>
              <a
                href="https://david.bertet.fr"
                target="_blank"
                rel="noopener"
                class="ml-1 inline-flex items-center gap-1 text-blue-600 dark:text-blue-400 hover:text-blue-700 dark:hover:text-blue-300 transition-colors duration-200 hover:underline"
              >
                David Bertet
              </a>
            </div>
          </div>
        </footer>
      </div>
    </main>
  </Sidebar.Provider>

  <AlertDialog.Root bind:open={errorMessage}>
    <AlertDialog.Content>
      <AlertDialog.Header>
        <AlertDialog.Title>An error occured</AlertDialog.Title>
        <AlertDialog.Description>
          {errorMessage}
        </AlertDialog.Description>
      </AlertDialog.Header>
      <AlertDialog.Footer>
        <AlertDialog.Action
          onclick={() => {
            errorMessage = null
          }}
        >
          OK
        </AlertDialog.Action>
      </AlertDialog.Footer>
    </AlertDialog.Content>
  </AlertDialog.Root>
</div>

{#if import.meta.env.MODE === 'github'}
  <DemoPill />
{/if}
