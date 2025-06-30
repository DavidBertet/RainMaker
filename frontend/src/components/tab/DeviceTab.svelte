<!-- Copyright (c) 2025 David Bertet. Licensed under the MIT License. -->

<script>
  import SectionHeader from 'src/components/common/SectionHeader.svelte'
</script>

<SectionHeader title="Device Settings" subtitle="Configure device preferences and options" />

<div class="settings-grid">
  <div class="setting-item">
    <label>
      <input type="checkbox" checked />
      Auto-connect to known networks
    </label>
  </div>
  <div class="setting-item">
    <label>
      <input type="checkbox" />
      Enable debug logging
    </label>
  </div>
  <div class="setting-item">
    <label>
      Device Name:
      <input type="text" value="My Device" />
    </label>
  </div>
  <div class="setting-item">
    <button class="secondary-btn">Restart Device</button>
    <button class="danger-btn">Factory Reset</button>
  </div>
</div>

<style>
  .settings-grid {
    background: white;
    border-radius: 12px;
    padding: 2rem;
    box-shadow: 0 1px 3px rgba(0, 0, 0, 0.1);
    border: 1px solid #e2e8f0;
  }

  .setting-item {
    margin-bottom: 1.5rem;
  }

  .setting-item:last-child {
    margin-bottom: 0;
  }

  .setting-item label {
    display: flex;
    align-items: center;
    gap: 0.5rem;
    color: #374151;
    font-weight: 500;
  }

  .setting-item input[type='checkbox'] {
    margin-right: 0.5rem;
  }

  .setting-item input[type='text'] {
    margin-left: 0.5rem;
    padding: 0.5rem;
    border: 1px solid #d1d5db;
    border-radius: 4px;
    font-size: 0.9rem;
  }

  .secondary-btn,
  .danger-btn {
    padding: 0.5rem 1rem;
    border-radius: 6px;
    font-size: 0.9rem;
    cursor: pointer;
    margin-right: 0.5rem;
    border: 1px solid;
    transition: all 0.2s ease;
  }

  .secondary-btn {
    background: white;
    color: #374151;
    border-color: #d1d5db;
  }

  .secondary-btn:hover {
    background: #f9fafb;
  }

  .danger-btn {
    background: #dc2625;
    color: white;
    border-color: #dc2625;
  }

  .danger-btn:hover {
    background: #b91c1c;
  }
</style>
