<!-- Copyright (c) 2025 David Bertet. Licensed under the MIT License. -->

<script>
  import * as Sidebar from '$lib/components/ui/sidebar'
  import { cn } from '$lib/utils'

  let { tabs, activeTab, onTabSelect } = $props()

  let sidebar = Sidebar.useSidebar()

  function setActiveTab(tab) {
    sidebar.setOpenMobile(false)
    onTabSelect(tab)
  }
</script>

<Sidebar.Group class="px-3 py-2">
  <Sidebar.GroupContent>
    <Sidebar.Menu class="space-y-1">
      {#each tabs as tab (tab.label)}
        <Sidebar.MenuItem onclick={() => setActiveTab(tab.id)}>
          <Sidebar.MenuButton
            isActive={tab.id == activeTab}
            class="w-full flex items-center gap-3 px-3 py-5 rounded-lg text-sm font-medium transition-all duration-200 cursor-pointer hover:bg-accent/50 data-[active=true]:bg-blue-500 data-[active=true]:text-white data-[active=true]:shadow-md data-[active=true]:shadow-blue-500/20"
          >
            <tab.icon
              size={18}
              class={cn(
                'shrink-0 transition-colors duration-200',
                tab.id === activeTab ? 'text-white' : 'text-muted-foreground',
              )}
            />
            <span
              class={cn(
                'transition-colors duration-200',
                tab.id === activeTab ? 'text-white' : 'text-foreground',
              )}
            >
              {tab.label}
            </span>
          </Sidebar.MenuButton>
        </Sidebar.MenuItem>
      {/each}
    </Sidebar.Menu>
  </Sidebar.GroupContent>
</Sidebar.Group>
