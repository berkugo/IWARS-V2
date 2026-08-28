import { useEffect, useRef, useState } from 'react'

export function normalizeUdp(udp) {
  const entity =
    Number(udp?.entity_port ?? udp?.port ?? 9000) || 9000
  const ownship = Number(udp?.ownship_port ?? 9001) || 9001
  return {
    host: '127.0.0.1',
    port: entity,
    entity_port: entity,
    ownship_port: ownship,
    enabled: !!udp?.enabled,
  }
}

export default function UdpConfigPage({ udp, onUdpChange, connected }) {
  const cfg = normalizeUdp(udp)
  const [entityPort, setEntityPort] = useState(String(cfg.entity_port))
  const [ownshipPort, setOwnshipPort] = useState(String(cfg.ownship_port))
  const timer = useRef(null)

  useEffect(() => {
    setEntityPort(String(cfg.entity_port))
    setOwnshipPort(String(cfg.ownship_port))
  }, [cfg.entity_port, cfg.ownship_port])

  function push(next) {
    onUdpChange(normalizeUdp({ ...cfg, ...next }))
  }

  function schedule(nextEntity, nextOwnship) {
    if (timer.current) clearTimeout(timer.current)
    timer.current = setTimeout(() => {
      const ep = Number(nextEntity)
      const op = Number(nextOwnship)
      push({
        entity_port: Number.isFinite(ep) && ep > 0 ? ep : 9000,
        ownship_port: Number.isFinite(op) && op > 0 ? op : 9001,
      })
    }, 350)
  }

  useEffect(() => () => clearTimeout(timer.current), [])

  return (
    <div className="flex min-h-0 flex-1 items-center justify-center bg-[var(--bg-deep)] p-8">
      <div className="udp-card w-full max-w-lg overflow-hidden">
        <div className="udp-card-bar" />
        <div className="p-6">
          <div className="section-kicker text-[var(--accent)]">UDP Config</div>
          <h1
            className="mt-2 text-2xl font-semibold tracking-tight text-[var(--text)]"
            style={{ fontFamily: 'var(--font-display)' }}
          >
            Local truth feeds
          </h1>
          <p className="mt-2 text-sm leading-relaxed text-[var(--muted)]">
            Two UDP streams on this machine while the scenario is playing. Host
            is locked to localhost for now. Packet layout is the IWP2 placeholder
            until the DSS encoder is swapped in.
          </p>

          <div className="mt-6 space-y-4">
            <label className="flex items-center justify-between gap-3 border border-[var(--line)] bg-black/30 px-3 py-3">
              <span className="text-sm text-[var(--text)]">Enable UDP publish</span>
              <input
                type="checkbox"
                className="h-4 w-4 accent-[var(--accent)]"
                checked={!!cfg.enabled}
                onChange={(e) => push({ enabled: e.target.checked })}
              />
            </label>

            <label className="block">
              <span className="mb-1.5 block text-[10px] uppercase tracking-wider text-[var(--muted)]">
                Host / IP
              </span>
              <input
                className="stitch-input"
                value="localhost"
                readOnly
                disabled
              />
            </label>

            <label className="block">
              <span className="mb-1.5 block text-[10px] uppercase tracking-wider text-[var(--muted)]">
                Entity Truth Data
              </span>
              <input
                type="number"
                className="stitch-input"
                value={entityPort}
                onChange={(e) => {
                  setEntityPort(e.target.value)
                  schedule(e.target.value, ownshipPort)
                }}
                placeholder="9000"
              />
            </label>

            <label className="block">
              <span className="mb-1.5 block text-[10px] uppercase tracking-wider text-[var(--muted)]">
                Ownship Truth Data
              </span>
              <input
                type="number"
                className="stitch-input"
                value={ownshipPort}
                onChange={(e) => {
                  setOwnshipPort(e.target.value)
                  schedule(entityPort, e.target.value)
                }}
                placeholder="9001"
              />
            </label>
          </div>

          <div
            className="mt-6 flex items-center justify-between border-t border-[var(--line)] pt-4 text-[11px]"
            style={{ fontFamily: 'var(--font-mono)' }}
          >
            <span className="text-[var(--muted)]">
              localhost:{cfg.entity_port}
              <span className="text-[var(--line-strong)]"> / </span>
              {cfg.ownship_port}
            </span>
            <span className={cfg.enabled ? 'text-[var(--accent)]' : 'text-[var(--muted)]'}>
              {cfg.enabled ? 'PUBLISH ON' : 'PUBLISH OFF'}
            </span>
          </div>
          <div
            className="mt-2 text-[11px] text-[var(--muted)]"
            style={{ fontFamily: 'var(--font-mono)' }}
          >
            Backend:{' '}
            <span className={connected ? 'text-emerald-400' : 'text-[var(--hostile)]'}>
              {connected ? 'connected' : 'offline'}
            </span>
          </div>
        </div>
      </div>
    </div>
  )
}
