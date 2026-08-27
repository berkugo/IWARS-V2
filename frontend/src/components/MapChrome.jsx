import { useEffect, useState } from 'react'
import { altToFL, mpsToKt } from '../radarTypes'

const TOOL_HINT = {
  add_track: 'Click map to place a track',
  add_waypoint: 'Click map to add a waypoint',
  select: null,
}

export default function MapChrome({
  state,
  selected,
  connected,
  mode = 'simulation',
  tool = 'select',
}) {
  const [now, setNow] = useState(() => new Date())
  useEffect(() => {
    const id = setInterval(() => setNow(new Date()), 1000)
    return () => clearInterval(id)
  }, [])

  const timeStr = now.toISOString().slice(11, 19) + 'Z'
  const kt = selected ? Math.round(mpsToKt(selected.speed_mps)) : null
  const fl = selected ? altToFL(selected.alt_m ?? selected.alt) : null
  const hint = mode === 'editor' ? TOOL_HINT[tool] : null
  const wpCount = selected ? (selected.route || []).length : 0
  const playing = !!state.playing

  return (
    <>
      <div className="map-grid-overlay absolute inset-0 z-[400]" />

      <div
        className="pointer-events-none absolute left-3 top-3 z-[500] max-w-sm text-[10px] leading-snug text-[var(--muted)]"
        style={{ fontFamily: 'var(--font-mono)' }}
      >
        <div className="flex flex-wrap items-center gap-x-2 gap-y-0.5">
          <span className="text-[var(--accent)]">
            {mode === 'editor' ? 'EDITOR' : 'SIM'}
          </span>
          <span className="text-[var(--line-strong)]">·</span>
          <span className="text-[var(--text)]">{state.name || 'untitled'}</span>
          <span className="text-[var(--line-strong)]">·</span>
          <span>{timeStr}</span>
        </div>
        <div className="mt-1 flex flex-wrap items-center gap-x-2">
          <span className={connected ? 'text-emerald-400' : 'text-[var(--hostile)]'}>
            {connected ? 'LINK' : 'NO LINK'}
          </span>
          {mode === 'simulation' && (
            <span className={playing ? 'text-[var(--accent)]' : 'text-[var(--muted)]'}>
              {playing ? 'PLAYING' : 'PAUSED'}
            </span>
          )}
          {mode === 'editor' && (
            <span className="uppercase text-[var(--text)]">
              {String(tool).replaceAll('_', ' ')}
            </span>
          )}
        </div>
      </div>

      {hint && (
        <div className="pointer-events-none absolute left-1/2 top-3 z-[500] -translate-x-1/2 border border-[var(--line)] bg-black/75 px-3 py-1.5 text-[10px] uppercase tracking-[0.14em] text-[var(--accent)]">
          {hint}
          {tool === 'add_waypoint' && !selected ? ' — select a track' : ''}
        </div>
      )}

      <div
        className="pointer-events-none absolute bottom-3 right-3 z-[500] w-52 border border-[var(--line)] bg-[var(--bg-panel)]/92 p-2.5 backdrop-blur-sm"
        style={{ fontFamily: 'var(--font-mono)' }}
      >
        {selected ? (
          <>
            <div className="mb-1.5 flex items-baseline justify-between gap-2 border-b border-[var(--line)] pb-1.5">
              <div className="min-w-0">
                <div className="truncate text-[11px] font-bold tracking-wide text-[var(--text)]">
                  {selected.name || selected.id}
                </div>
                <div className="text-[9px] text-[var(--muted)]">
                  {selected.ownship || selected.id === 'ownship'
                    ? 'OWNSHIP'
                    : (selected.platform || 'track').toUpperCase()}
                </div>
              </div>
              <div className="shrink-0 text-[9px] uppercase text-[var(--accent)]">
                {playing && mode === 'simulation' ? 'LIVE' : mode === 'editor' ? 'EDIT' : 'HOLD'}
              </div>
            </div>
            <div className="space-y-0.5 text-[10px] text-[var(--muted)]">
              <Row k="GS" v={`${kt} kt`} />
              <Row
                k="FL"
                v={`FL${String(fl).padStart(3, '0')} / ${Number(selected.alt_m ?? selected.alt ?? 0).toFixed(0)} m`}
              />
              <Row
                k="HDG"
                v={`${String(Math.round(Number(selected.heading_deg) || 0)).padStart(3, '0')}°`}
              />
              <Row k="WP" v={String(wpCount)} />
              <Row
                k="IFF"
                v={
                  selected.iff_enabled
                    ? `${selected.iff_mode || '3A'} ${selected.squawk || '----'}`
                    : 'OFF'
                }
              />
            </div>
          </>
        ) : (
          <div className="text-[10px] uppercase tracking-wider text-[var(--muted)]">
            No track selected
          </div>
        )}
      </div>
    </>
  )
}

function Row({ k, v }) {
  return (
    <div className="flex justify-between gap-3">
      <span>{k}</span>
      <span className="text-[var(--text)]">{v}</span>
    </div>
  )
}
