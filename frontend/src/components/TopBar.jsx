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
}) {
  const udp = state.udp || { host: '127.0.0.1', port: 9000, enabled: false }
  const t = Number(state.sim_time || 0).toFixed(1)

  return (
    <header className="flex h-12 shrink-0 items-center gap-4 border-b border-[var(--line)] bg-[var(--bg-panel)] px-4">
      <div className="flex shrink-0 items-center gap-2">
        <span
          className="text-lg font-bold tracking-[0.06em] text-[var(--accent)]"
          style={{ fontFamily: 'var(--font-display)' }}
        >
          IWARS v2
        </span>
      </div>

      <nav className="ml-4 flex items-center gap-5">
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
            UDP:{' '}
            <span className="text-[var(--text)]">
              {udp.host}:{udp.port}
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
              className={`icon-btn ${state.playing ? 'active' : ''}`}
              title={state.playing ? 'Pause' : 'Play'}
              onClick={state.playing ? onPause : onPlay}
            >
              {state.playing ? <IconPause /> : <IconPlay />}
            </button>
            <button type="button" className="icon-btn" title="Reset" onClick={onReset}>
              <IconReset />
            </button>
          </>
        )}

        <div className="ml-1 flex items-center gap-2">
          <span
            className={`h-2.5 w-2.5 shrink-0 rounded-full ${
              connected ? 'bg-emerald-500' : 'bg-red-500'
            }`}
            title={connected ? 'Live' : 'Offline'}
          />
          {nav === 'simulation' && (
            <span
              className="text-xs text-[var(--muted)]"
              style={{ fontFamily: 'var(--font-mono)' }}
            >
              T+{t}s
            </span>
          )}
          {nav === 'editor' && (
            <span
              className="text-xs uppercase tracking-wider text-[var(--accent)]"
              style={{ fontFamily: 'var(--font-mono)' }}
            >
              Edit
            </span>
          )}
        </div>
      </div>
    </header>
  )
}
