export default function UdpConfigPage({ udp, onUdpChange, connected }) {
  const cfg = udp || { host: '127.0.0.1', port: 9000, enabled: false }

  return (
    <div className="flex min-h-0 flex-1 items-center justify-center bg-[var(--bg-deep)] p-8">
      <div className="w-full max-w-lg border border-[var(--line)] bg-[var(--bg-panel)] p-6">
        <div
          className="text-xs font-bold uppercase tracking-[0.2em] text-[var(--accent)]"
          style={{ fontFamily: 'var(--font-display)' }}
        >
          UDP Config
        </div>
        <h1
          className="mt-1 text-2xl font-semibold text-[var(--text)]"
          style={{ fontFamily: 'var(--font-display)' }}
        >
          Outbound feed target
        </h1>
        <p className="mt-2 text-sm leading-relaxed text-[var(--muted)]">
          Entity truth packets are sent here. Radar / IFF blips are produced by the
          workplace simulator — not by IWARS v2.
        </p>

        <div className="mt-6 space-y-4">
          <label className="flex items-center justify-between gap-3 border border-[var(--line)] bg-black/30 px-3 py-3">
            <span className="text-sm text-[var(--text)]">Enable UDP publish</span>
            <input
              type="checkbox"
              className="h-4 w-4 accent-[var(--accent)]"
              checked={!!cfg.enabled}
              onChange={(e) => onUdpChange({ ...cfg, enabled: e.target.checked })}
            />
          </label>

          <label className="block">
            <span className="mb-1.5 block text-[10px] uppercase tracking-wider text-[var(--muted)]">
              Host / IP
            </span>
            <input
              className="stitch-input"
              value={cfg.host}
              onChange={(e) => onUdpChange({ ...cfg, host: e.target.value })}
              placeholder="127.0.0.1"
            />
          </label>

          <label className="block">
            <span className="mb-1.5 block text-[10px] uppercase tracking-wider text-[var(--muted)]">
              Port
            </span>
            <input
              type="number"
              className="stitch-input"
              value={cfg.port}
              onChange={(e) => onUdpChange({ ...cfg, port: Number(e.target.value) })}
              placeholder="9000"
            />
          </label>
        </div>

        <div
          className="mt-6 flex items-center justify-between border-t border-[var(--line)] pt-4 text-[11px]"
          style={{ fontFamily: 'var(--font-mono)' }}
        >
          <span className="text-[var(--muted)]">
            Target:{' '}
            <span className="text-[var(--text)]">
              {cfg.host}:{cfg.port}
            </span>
          </span>
          <span className={cfg.enabled ? 'text-[var(--accent)]' : 'text-[var(--muted)]'}>
            {cfg.enabled ? 'PUBLISH ON' : 'PUBLISH OFF'}
          </span>
        </div>
        <div
          className="mt-2 text-[11px] text-[var(--muted)]"
          style={{ fontFamily: 'var(--font-mono)' }}
        >
          WS:{' '}
          <span className={connected ? 'text-[var(--accent)]' : 'text-[var(--hostile)]'}>
            {connected ? 'LIVE' : 'OFFLINE'}
          </span>
        </div>
      </div>
    </div>
  )
}
