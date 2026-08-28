const TOOL_HINT = {
  add_track: 'Click map to place a track',
  add_waypoint: 'Click to add WP · double-click finishes · Esc select',
  select: null,
}

export default function MapChrome({
  selected,
  mode = 'simulation',
  tool = 'select',
}) {
  const hint = mode === 'editor' ? TOOL_HINT[tool] : null
  const wpCount = selected ? (selected.route || []).length : 0

  return (
    <>
      <div className="map-grid-overlay absolute inset-0 z-[400]" />
      {hint && (
        <div className="hud-chip pointer-events-none absolute left-1/2 top-3 z-[500] -translate-x-1/2 px-3 py-1.5 text-[10px] uppercase tracking-[0.14em] text-[var(--accent)]">
          {hint}
          {tool === 'add_waypoint' && !selected
            ? ' — select a track'
            : tool === 'add_waypoint' && selected
              ? ` · ${wpCount} placed`
              : ''}
        </div>
      )}
    </>
  )
}
