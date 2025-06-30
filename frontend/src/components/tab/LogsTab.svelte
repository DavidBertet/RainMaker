<!-- Copyright (c) 2025 David Bertet. Licensed under the MIT License. -->

<script>
  import SectionHeader from 'src/components/common/SectionHeader.svelte'
</script>

<SectionHeader title="System Logs" subtitle="Recent system events and messages" />

<div class="logs-container">
  <div class="log-entry">
    <span class="log-time">2025-05-28 14:32:15</span>
    <span class="log-level info">INFO</span>
    <span class="log-message">WiFi connection established</span>
  </div>
  <div class="log-entry">
    <span class="log-time">2025-05-28 14:30:42</span>
    <span class="log-level info">INFO</span>
    <span class="log-message">System startup completed</span>
  </div>
  <div class="log-entry">
    <span class="log-time">2025-05-28 14:30:15</span>
    <span class="log-level warn">WARN</span>
    <span class="log-message">Low disk space warning</span>
  </div>
</div>

<style>
  .logs-container {
    background: white;
    border-radius: 12px;
    padding: 1.5rem;
    box-shadow: 0 1px 3px rgba(0, 0, 0, 0.1);
    border: 1px solid #e2e8f0;
    font-family: 'Monaco', 'Menlo', monospace;
  }

  .log-entry {
    display: flex;
    gap: 1rem;
    padding: 0.5rem 0;
    border-bottom: 1px solid #f1f5f9;
    font-size: 0.85rem;
  }

  .log-entry:last-child {
    border-bottom: none;
  }

  .log-time {
    color: #64748b;
    min-width: 140px;
  }

  .log-level {
    min-width: 50px;
    font-weight: 600;
  }

  .log-level.info {
    color: #0ea5e9;
  }

  .log-level.warn {
    color: #f59e0b;
  }

  .log-message {
    color: #374151;
  }
</style>
