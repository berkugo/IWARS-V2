import { normalizeUdp } from './UdpConfigPage'
import { altToFL, mpsToKt } from '../radarTypes'

const NAV = [
  { id: 'simulation', label: 'Simulation' },
  { id: 'editor', label: 'Scenario Editor' },
  { id: 'udp', label: 'UDP Config' },
]

function IconPlay() {
  return (
    <svg width="14" height="14" viewBox="0 0 14 14" fill="currentColor">
      <path d="M3 1.5v11l9-5.5L3 1.5z" />
    </svg>
  )
}

function IconPause() {
  return (
    <svg width="14" height="14" viewBox="0 0 14 14" fill="currentColor">
      <rect x="3" y="2" width="3" height="10" />
      <rect x="8" y="2" width="3" height="10" />
    </svg>
  )
}

function IconReset() {
  return (
    <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
      <path d="M3 12a9 9 0 1 0 3-6.7" />
      <path d="M3 4v5h5" />
    </svg>
  )
}

export default function TopBar({
  state,
  connected,
  nav,
  setNav,
  onPlay,
  onPause,
  onReset,
  selected,
}) {
  const udp = normalizeUdp(state.udp)
  const t = Number(state.sim_time || 0).toFixed(1)
  const playing = !!state.playing
  const own = !!(selected && (selected.ownship || selected.id === 'ownship'))
  const role = selected
    ? own
      ? 'OWNSHIP'
      : (selected.platform || 'track').toUpperCase()
    : null
  const kt = selected ? Math.round(mpsToKt(selected.speed_mps)) : null
  const fl = selected ? altToFL(selected.alt_m ?? selected.alt) : null
  const hdg = selected
    ? String(Math.round(Number(selected.heading_deg) || 0)).padStart(3, '0')
    : null
  const iff = selected?.iff_enabled
    ? `${selected.iff_mode || '3A'} ${selected.squawk || '----'}`
    : null

  return (
    <header className="topbar flex h-12 shrink-0 items-center gap-4 px-4">
      <div className="flex shrink-0 items-center">
        <span
          className="text-[15px] font-bold tracking-[0.12em] text-[var(--accent)]"
          style={{ fontFamily: 'var(--font-display)' }}
        >
          IWARS
          <span className="ml-1 text-[10px] font-semibold tracking-[0.18em] text-[var(--muted)]">
            v2
          </span>
        </span>
      </div>

      <nav className="ml-2 flex items-center gap-1">
        {NAV.map((item) => (
          <button
            key={item.id}
            type="button"
            className={`nav-link ${nav === item.id ? 'active' : ''}`}
            onClick={() => setNav(item.id)}
          >
            {item.label}
          </button>
        ))}
      </nav>

      <div className="ml-auto flex min-w-0 items-center gap-2">
        <div
          className="hidden items-center gap-1.5 text-[11px] sm:flex"
          style={{ fontFamily: 'var(--font-mono)' }}
        >
          <span
            className={`h-2 w-2 rounded-full ${udp.enabled ? 'bg-[var(--accent)] pulse' : 'bg-[var(--muted)]'}`}
          />
          <span className="text-[var(--muted)]">
            UDP{' '}
            <span className="text-[var(--text)]">
              {udp.entity_port ?? udp.port ?? 9000}/{udp.ownship_port ?? 9001}
            </span>{' '}
            <span className={udp.enabled ? 'text-[var(--accent)]' : ''}>
              {udp.enabled ? 'ON' : 'OFF'}
            </span>
          </span>
        </div>

        {nav === 'simulation' && (
          <>
            <div className="mx-1 h-5 w-px bg-[var(--line)]" />
            <button
              type="button"
              className={`icon-btn ${playing ? 'active' : ''}`}
              title={playing ? 'Pause (Space)' : 'Play (Space)'}
              onClick={playing ? onPause : onPlay}
            >
              {playing ? <IconPause /> : <IconPlay />}
            </button>
            <button type="button" className="icon-btn" title="Reset to scenario start" onClick={onReset}>
              <IconReset />
            </button>
          </>
        )}

        <div className="ml-1 flex shrink-0 items-center gap-2">
          {selected ? (
            <div
              className="hidden items-baseline gap-2 border-l border-[var(--line)] pl-3 sm:flex"
              style={{ fontFamily: 'var(--font-mono)' }}
              title={`${selected.name || selected.id} ${role}`}
            >
              <span className="max-w-[7rem] truncate text-[12px] font-semibold tracking-wide text-[var(--text)]">
                {selected.name || selected.id}
              </span>
              <span className="text-[10px] tracking-wider text-[var(--muted)]">{role}</span>
              <span className="text-[10px] text-[var(--muted)]">
                <span className="text-[var(--text)]">{kt}</span> kt
              </span>
              <span className="text-[10px] text-[var(--muted)]">
                FL<span className="text-[var(--text)]">{String(fl).padStart(3, '0')}</span>
              </span>
              <span className="text-[10px] text-[var(--muted)]">
                <span className="text-[var(--text)]">{hdg}</span>°
              </span>
              {iff ? (
                <span className="text-[10px] text-[var(--muted)]">
                  IFF <span className="text-[var(--text)]">{iff}</span>
                </span>
              ) : (
                <span className="text-[10px] text-[var(--muted)]">IFF OFF</span>
              )}
            </div>
          ) : null}
          <span
            className={`h-2.5 w-2.5 shrink-0 rounded-full ${
              connected ? 'bg-emerald-500' : 'bg-red-500'
            }`}
            title={connected ? 'Backend connected' : 'Backend offline'}
          />
          {nav === 'simulation' && (
            <span
              className="text-xs text-[var(--muted)]"
              style={{ fontFamily: 'var(--font-mono)' }}
            >
              T+{t}s
            </span>
          )}
        </div>
      </div>
    </header>
  )
}
