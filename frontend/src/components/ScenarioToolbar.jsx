export default function ScenarioToolbar({
  state,
  connected,
  scenarios,
  onPlay,
  onPause,
  onReset,
  onUdpChange,
  onLoad,
  onSave,
  saveName,
  setSaveName,
}) {
  const udp = state.udp || { host: '127.0.0.1', port: 9000, enabled: false }
  const t = Number(state.sim_time || 0).toFixed(1)

  return (
    <header className="flex h-14 shrink-0 items-center gap-3 border-b border-[var(--line)] bg-[var(--bg-panel)] px-4">
      <div className="flex shrink-0 items-baseline gap-2">
        <span
          className="text-xl font-bold leading-none tracking-[0.08em]"
          style={{ fontFamily: 'var(--font-display)' }}
        >
          IWARS
        </span>
        <span
          className="text-[10px] font-medium uppercase tracking-[0.2em] text-[var(--muted)]"
          style={{ fontFamily: 'var(--font-display)' }}
        >
          Entity Feed
        </span>
      </div>

      <div className="h-6 w-px shrink-0 bg-[var(--line)]" />

      <div className="flex shrink-0 items-center gap-1.5">
        {!state.playing ? (
          <button type="button" className="hud-btn hud-btn-play" onClick={onPlay}>
            Play
          </button>
        ) : (
          <button type="button" className="hud-btn hud-btn-pause" onClick={onPause}>
            Pause
          </button>
        )}
        <button type="button" className="hud-btn" onClick={onReset}>
          Reset
        </button>
      </div>

      <div className="h-6 w-px shrink-0 bg-[var(--line)]" />

      <div className="flex min-w-0 flex-1 items-center gap-2 overflow-x-auto">
        <label className="flex shrink-0 cursor-pointer items-center gap-2 text-[11px] font-semibold uppercase tracking-wider text-[var(--muted)]">
          <span
            className={`relative inline-flex h-4 w-7 items-center rounded-full border transition ${
              udp.enabled
                ? 'border-teal-400/50 bg-teal-500/30'
                : 'border-[var(--line)] bg-black/40'
            }`}
          >
            <input
              type="checkbox"
              className="sr-only"
              checked={!!udp.enabled}
              onChange={(e) => onUdpChange({ ...udp, enabled: e.target.checked })}
            />
            <span
              className={`absolute left-0.5 h-3 w-3 rounded-full transition ${
                udp.enabled ? 'translate-x-3 bg-[var(--accent)]' : 'bg-[var(--muted)]'
              }`}
            />
          </span>
          UDP
        </label>
        <input
          className="hud-input w-28 shrink-0"
          value={udp.host}
          onChange={(e) => onUdpChange({ ...udp, host: e.target.value })}
          title="UDP host"
        />
        <input
          type="number"
          className="hud-input w-16 shrink-0"
          value={udp.port}
          onChange={(e) => onUdpChange({ ...udp, port: Number(e.target.value) })}
          title="UDP port"
        />

        <div className="mx-1 h-5 w-px shrink-0 bg-[var(--line)]" />

        <select
          className="hud-input w-40 shrink-0"
          defaultValue=""
          onChange={(e) => {
            if (e.target.value) onLoad(e.target.value)
            e.target.value = ''
          }}
        >
          <option value="" disabled>
            Load scenario…
          </option>
          {scenarios.map((f) => (
            <option key={f} value={f}>
              {f}
            </option>
          ))}
        </select>
        <input
          className="hud-input w-32 shrink-0"
          placeholder="save as…"
          value={saveName}
          onChange={(e) => setSaveName(e.target.value)}
        />
        <button type="button" className="hud-btn shrink-0" onClick={onSave}>
          Save
        </button>
      </div>

      <div
        className="ml-2 flex shrink-0 items-center gap-3 text-[11px] text-[var(--muted)]"
        style={{ fontFamily: 'var(--font-mono)' }}
      >
        <span>
          T+<span className="text-[var(--accent)]">{t}</span>s
        </span>
        <span className="flex items-center gap-1.5">
          <span
            className={`inline-block h-1.5 w-1.5 rounded-full ${
              connected ? 'live-dot bg-[var(--accent)]' : 'bg-red-400'
            }`}
          />
          {connected ? 'LIVE' : 'OFFLINE'}
        </span>
      </div>
    </header>
  )
}
