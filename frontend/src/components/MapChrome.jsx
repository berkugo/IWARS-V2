import { useEffect, useState } from 'react'
import { altToFL, mpsToKt } from '../radarTypes'

const TOOL_BANNER = {
  add_track: 'Click map to place air track',
  add_waypoint: 'Click map to append waypoint to selected track',
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

  const dateStr = now
    .toLocaleDateString('en-GB', { day: '2-digit', month: 'short', year: 'numeric' })
    .toUpperCase()
  const timeStr = now.toISOString().slice(11, 19) + 'Z'
  const kt = selected ? Math.round(mpsToKt(selected.speed_mps)) : null
  const fl = selected ? altToFL(selected.alt_m ?? selected.alt) : null
  const banner = mode === 'editor' ? TOOL_BANNER[tool] : null
  const wpCount = selected ? (selected.route || []).length : 0

  return (
    <>
      <div className="map-grid-overlay absolute inset-0 z-[400]" />

      <div
        className="pointer-events-none absolute left-3 top-3 z-[500] max-w-xs text-[10px] leading-relaxed text-[var(--muted)]"
        style={{ fontFamily: 'var(--font-mono)' }}
      >
        <div className="text-[var(--accent)]">
          IWARS v2 — {mode === 'editor' ? 'SCENARIO EDITOR' : 'AIR C4ISR'}
        </div>
        <div>
          SYSTEM STATUS:{' '}
          <span className={connected ? 'text-[var(--accent)]' : 'text-[var(--hostile)]'}>
            {connected ? 'ACTIVE' : 'OFFLINE'}
          </span>
        </div>
        <div>
          {state.name || 'untitled'} · {dateStr} · {timeStr}
        </div>
        <div>
          {mode === 'editor'
            ? `TOOL: ${String(tool).replaceAll('_', ' ').toUpperCase()}`
            : 'RUN MODE · READ-ONLY · PLAY / PAUSE / RESET'}
        </div>
      </div>

      {banner && (
        <div className="pointer-events-none absolute left-1/2 top-4 z-[500] -translate-x-1/2 border border-[var(--accent)] bg-black/80 px-4 py-2 text-[11px] font-bold uppercase tracking-[0.18em] text-[var(--accent)] shadow-[0_0_20px_var(--accent-glow)]">
          {banner}
        </div>
      )}

      {mode === 'editor' && tool === 'add_waypoint' && !selected && (
        <div className="pointer-events-none absolute left-1/2 top-16 z-[500] -translate-x-1/2 border border-[var(--unknown)] bg-black/80 px-4 py-2 text-[11px] uppercase tracking-[0.14em] text-[var(--unknown)]">
          Select a track first
        </div>
      )}

      <div
        className="pointer-events-none absolute bottom-10 right-3 z-[500] w-56 border border-[var(--line)] bg-[var(--bg-panel)]/95 p-3 backdrop-blur-sm"
        style={{ fontFamily: 'var(--font-mono)' }}
      >
        {selected ? (
          <>
            <div className="mb-2 flex items-center justify-between border-b border-[var(--line)] pb-2">
              <div>
                <div className="text-[11px] font-bold tracking-wide text-[var(--text)]">
                  {selected.name || selected.id}
                </div>
                <div className="text-[9px] text-[var(--muted)]">
                  {selected.ownship || selected.id === 'ownship'
                    ? 'OWNSHIP · AEW&C'
                    : `${(selected.platform || selected.entity_type || 'track').toUpperCase()} · ${selected.id}`}
                </div>
              </div>
              <div className="text-[9px] uppercase text-[var(--accent)]">
                {mode === 'editor' ? 'Edit' : 'Track'}
              </div>
            </div>
            <div className="space-y-1 text-[10px] text-[var(--muted)]">
              <Row k="GS" v={`${kt} kt`} />
              <Row
                k="FL / Alt"
                v={`FL${String(fl).padStart(3, '0')} / ${Number(selected.alt_m ?? selected.alt ?? 0).toFixed(0)} m`}
              />
              <Row
                k="Heading"
                v={`${String(Math.round(Number(selected.heading_deg) || 0)).padStart(3, '0')}° TRUE`}
              />
              <Row k="Waypoints" v={String(wpCount)} />
              <Row
                k="IFF"
                v={
                  selected.iff_enabled
                    ? `${selected.iff_mode || '3A'} / ${selected.squawk || '----'}`
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
