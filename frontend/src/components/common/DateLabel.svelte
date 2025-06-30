<!-- Copyright (c) 2025 David Bertet. Licensed under the MIT License. -->

<script>
  let { timestampSeconds, ...restProps } = $props()
</script>

<span {...restProps}>
  {timestampSeconds
    ? new Date(timestampSeconds * 1000).toLocaleString(undefined, {
        month: 'short',
        day: 'numeric',
        hour: 'numeric',
        minute: '2-digit',
      })
    : 'Never'}
</span>
