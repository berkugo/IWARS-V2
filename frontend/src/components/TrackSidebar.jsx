import { useEffect, useRef, useState } from 'react'
import {
  AFFILIATIONS,
  IFF_MODES,
  OWNSHIP_CALLSIGN,
  OWNSHIP_ID,
  PLATFORMS,
  altToFL,
  isOwnship,
  ktToMps,
  mpsToKt,
} from '../radarTypes'

const TRACK_PLATFORMS = PLATFORMS.filter((p) => p.id !== 'aircraft')

const TABS = [
  { id: 'tracks', label: 'Air Tracks', icon: '▣' },
  { id: 'logs', label: 'Logs', icon: '☰' },
]

const AUTO_FIELDS = new Set([
  'lat',
  'lon',
  'alt_m',
  'heading_deg',
  'speed_mps',
  'climb_mps',
])

function affTag(aff) {
  if (aff === 'friend' || aff === 'blue' || aff === 'assumed_friend') return 'tag-friend'
  if (aff === 'hostile' || aff === 'red') return 'tag-hostile'
  if (aff === 'neutral') return 'tag-neutral'
  if (aff === 'suspect') return 'tag-suspect'
  return 'tag-unknown'
}

function affLabel(aff) {
  const a = AFFILIATIONS.find((x) => x.id === aff)
  return a?.short || aff?.toUpperCase() || 'UNK'
}

function platShort(id) {
  const p = TRACK_PLATFORMS.find((x) => x.id === id) || PLATFORMS.find((x) => x.id === id)
  if (!p) return id || '?'
  return p.label.split(' ')[0]
}

function cloneEntity(e) {
  return e ? { ...e, route: Array.isArray(e.route) ? [...e.route] : [] } : null
}

function finiteNum(v, fallback = 0) {
  const n = Number(v)
  return Number.isFinite(n) ? n : fallback
}

export default function TrackSidebar({
  entities,
  selectedId,
  onSelect,
  onChange,
  onDelete,
  editorMode = false,
  readOnly = false,
  onClearRoute,
  onRemoveWaypoint,
}) {
  const [tab, setTab] = useState('tracks')
  const [draft, setDraft] = useState(null)
  const [dirty, setDirty] = useState(false)
  const dirtyRef = useRef(false)
  const draftRef = useRef(null)
  const flushTimer = useRef(null)
  const skipSyncUntil = useRef(0)

  useEffect(() => {
    draftRef.current = draft
  }, [draft])

  useEffect(() => {
    dirtyRef.current = false
    setDirty(false)
    skipSyncUntil.current = 0
    if (flushTimer.current) clearTimeout(flushTimer.current)
    const e = entities.find((x) => x.id === selectedId) || null
    setDraft(cloneEntity(e))
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [selectedId])

  useEffect(() => {
    if (!selectedId || dirtyRef.current) return
    if (Date.now() < skipSyncUntil.current) return
    const e = entities.find((x) => x.id === selectedId) || null
    setDraft(cloneEntity(e))
  }, [entities, selectedId])

  async function flushEntity(entity) {
    if (readOnly || !entity) return
    const payload = {
      ...entity,
      lat: finiteNum(entity.lat),
      lon: finiteNum(entity.lon),
      alt_m: finiteNum(entity.alt_m ?? entity.alt),
      heading_deg: finiteNum(entity.heading_deg),
      speed_mps: finiteNum(entity.speed_mps),
      climb_mps: finiteNum(entity.climb_mps),
      route: Array.isArray(entity.route) ? entity.route : [],
      route_index: Math.max(0, finiteNum(entity.route_index, 0)),
    }
    if (isOwnship(payload)) {
      payload.id = OWNSHIP_ID
      payload.name = OWNSHIP_CALLSIGN
      payload.affiliation = 'friend'
      payload.platform = 'aew'
      payload.ownship = true
    }
    skipSyncUntil.current = Date.now() + 2000
    try {
      await onChange(payload)
      draftRef.current = payload
      setDraft(cloneEntity(payload))
      dirtyRef.current = false
      setDirty(false)
    } catch (err) {
      console.warn(err)
    }
  }

  function scheduleFlush(entity, immediate = false) {
    if (flushTimer.current) clearTimeout(flushTimer.current)
    const run = () => flushEntity(entity)
    if (immediate) run()
    else flushTimer.current = setTimeout(run, 250)
  }

  function patchDraft(field, value, immediate = false) {
    if (readOnly) return
    dirtyRef.current = true
    setDirty(true)
    let nextEntity = null
    setDraft((prev) => {
      if (!prev) return prev
      const next = { ...prev, [field]: value }
      if (field === 'platform') {
        const p = PLATFORMS.find((x) => x.id === value)
        if (p) {
          next.speed_mps = p.speed
          next.alt_m = p.alt
        }
      }
      if (field === 'heading_deg') {
        next.route = []
        next.route_index = 0
      }
      draftRef.current = next
      nextEntity = next
      return next
    })
    if (nextEntity && (AUTO_FIELDS.has(field) || field === 'platform')) {
      const asap = immediate || field === 'speed_mps' || field === 'climb_mps'
      scheduleFlush(nextEntity, asap)
    }
  }

  async function commit() {
    if (flushTimer.current) clearTimeout(flushTimer.current)
    await flushEntity(draftRef.current)
  }

  useEffect(() => {
    return () => {
      if (flushTimer.current) clearTimeout(flushTimer.current)
    }
  }, [])

  function affOf(e) {
    return e.affiliation || e.side || 'unknown'
  }

  return (
    <aside className="flex h-full w-[300px] shrink-0 flex-col border-r border-[var(--line)] bg-[var(--bg-panel)]">
      <div className="border-b border-[var(--line)] px-3 py-3">
        <div
          className="text-[10px] font-bold uppercase tracking-[0.16em] text-[var(--muted)]"
          style={{ fontFamily: 'var(--font-display)' }}
        >
          {editorMode ? 'Tracks in scenario' : 'Live tracks'}
        </div>
        {editorMode ? (
          <p className="mt-1 text-[10px] leading-relaxed text-[var(--muted)]">
            Use Place Track / Place Waypoint in the editor panel.
          </p>
        ) : (
          <p className="mt-1 text-[10px] leading-relaxed text-[var(--muted)]">
            Read-only run view — Play / Pause / Reset only.
          </p>
        )}
      </div>

      <div className="flex border-b border-[var(--line)]">
        {TABS.map((t) => (
          <button
            key={t.id}
            type="button"
            className={`side-tab ${tab === t.id ? 'active' : ''}`}
            onClick={() => setTab(t.id)}
          >
            <span className="text-sm leading-none">{t.icon}</span>
            {t.label}
          </button>
        ))}
      </div>

      {tab === 'logs' ? (
        <div className="flex flex-1 items-center justify-center p-6 text-center text-xs text-[var(--muted)]">
          Logs — UDP publish / air picture events (soon)
        </div>
      ) : (
        <>
          <ul className="max-h-[38%] space-y-1.5 overflow-y-auto p-2">
            {entities.length === 0 && (
              <li className="px-2 py-8 text-center text-xs text-[var(--muted)]">
                {editorMode
                  ? 'No air tracks — use Place Track, then click the map'
                  : 'No air tracks in this scenario'}
              </li>
            )}
            {entities.map((e) => {
              const aff = affOf(e)
              const isSelected = e.id === selectedId
              const hostile = aff === 'hostile' || aff === 'red'
              const own = isOwnship(e)
              const fl = altToFL(e.alt_m ?? e.alt)
              const kt = Math.round(mpsToKt(e.speed_mps))
              return (
                <li key={e.id}>
                  <button
                    type="button"
                    onClick={() => onSelect(e.id)}
                    className={`track-card ${
                      isSelected ? (hostile ? 'selected-hostile' : 'selected') : ''
                    }`}
                  >
                    <div className="flex items-center justify-between gap-2">
                      <span className="truncate text-sm font-semibold tracking-wide">
                        {e.name || e.id}
                      </span>
                      <span className={`tag ${own ? 'tag-ownship' : affTag(aff)}`}>
                        {own ? 'OWN' : affLabel(aff)}
                      </span>
                    </div>
                    <div
                      className="mt-1.5 flex flex-wrap gap-x-3 gap-y-0.5 text-[10px] text-[var(--muted)]"
                      style={{ fontFamily: 'var(--font-mono)' }}
                    >
                      <span>{own ? 'AEW&C' : platShort(e.platform || e.entity_type)}</span>
                      <span>
                        FL
                        <span className="text-[var(--text)]">{String(fl).padStart(3, '0')}</span>
                      </span>
                      {own && kt < 1 ? (
                        <span className="text-[var(--accent)]">STATIC</span>
                      ) : (
                        <span>
                          <span className="text-[var(--text)]">{kt}</span> kt
                        </span>
                      )}
                      <span>
                        HDG{' '}
                        <span className="text-[var(--text)]">
                          {String(Math.round(Number(e.heading_deg) || 0)).padStart(3, '0')}°
                        </span>
                      </span>
                    </div>
                  </button>
                </li>
              )
            })}
          </ul>

          <div className="flex min-h-0 flex-1 flex-col border-t border-[var(--line)]">
            <div className="border-b border-[var(--line)] px-3 py-2">
              <div
                className="text-[10px] font-bold uppercase tracking-[0.16em] text-[var(--muted)]"
                style={{ fontFamily: 'var(--font-display)' }}
              >
                {readOnly ? 'Track Monitor' : 'Track Detail'}
              </div>
            </div>

            {!draft ? (
              <p className="p-4 text-center text-xs leading-relaxed text-[var(--muted)]">
                {readOnly
                  ? 'Select a track to inspect live truth data.'
                  : 'Select an air track to edit callsign, identity, and kinematics.'}
              </p>
            ) : readOnly ? (
              <div
                className="flex min-h-0 flex-1 flex-col gap-2 overflow-y-auto p-3 text-[11px]"
                style={{ fontFamily: 'var(--font-mono)' }}
              >
                {isOwnship(draft) && (
                  <div
                    className="border border-[var(--accent)] bg-[var(--accent-dim)] px-2 py-1.5 text-[10px] font-bold uppercase tracking-[0.14em] text-[var(--accent)]"
                    style={{ fontFamily: 'var(--font-display)' }}
                  >
                    OWNSHIP · AEWC737
                  </div>
                )}
                <Row k="Callsign" v={draft.name || draft.id} />
                <Row k="Identity" v={affLabel(affOf(draft))} />
                <Row
                  k="Platform"
                  v={(draft.platform || draft.entity_type || '—').toUpperCase()}
                />
                <Row k="Lat" v={Number(draft.lat).toFixed(5)} />
                <Row k="Lon" v={Number(draft.lon).toFixed(5)} />
                <Row
                  k="Alt"
                  v={`${Number(draft.alt_m ?? draft.alt ?? 0).toFixed(0)} m · FL${String(altToFL(draft.alt_m ?? draft.alt)).padStart(3, '0')}`}
                />
                <Row
                  k="Heading"
                  v={`${String(Math.round(Number(draft.heading_deg) || 0)).padStart(3, '0')}°`}
                />
                <Row
                  k="Speed"
                  v={`${Math.round(mpsToKt(draft.speed_mps))} kt · ${finiteNum(draft.speed_mps).toFixed(1)} m/s`}
                />
                <Row k="Climb" v={`${finiteNum(draft.climb_mps).toFixed(1)} m/s`} />
                <Row
                  k="IFF"
                  v={
                    draft.iff_enabled
                      ? `${draft.iff_mode || '3A'} / ${draft.squawk || '----'}`
                      : 'OFF'
                  }
                />
                <Row k="Waypoints" v={String((draft.route || []).length)} />
                <p className="mt-3 text-[10px] leading-relaxed text-[var(--muted)]">
                  Simulation is read-only. Edit tracks in Scenario Editor.
                </p>
              </div>
            ) : (
              <div className="flex min-h-0 flex-1 flex-col gap-2.5 overflow-y-auto p-3">
                {isOwnship(draft) && (
                  <div
                    className="border border-[var(--accent)] bg-[var(--accent-dim)] px-2 py-1.5 text-[10px] font-bold uppercase tracking-[0.14em] text-[var(--accent)]"
                    style={{ fontFamily: 'var(--font-display)' }}
                  >
                    OWNSHIP · AEWC737
                  </div>
                )}

                <label className="block">
                  <span className="mb-1 block text-[10px] uppercase tracking-wider text-[var(--muted)]">
                    Callsign
                  </span>
                  <input
                    className="stitch-input"
                    value={draft.name || ''}
                    disabled={isOwnship(draft)}
                    onChange={(e) => patchDraft('name', e.target.value)}
                  />
                </label>

                <label className="block">
                  <span className="mb-1 block text-[10px] uppercase tracking-wider text-[var(--muted)]">
                    Identity
                  </span>
                  <select
                    className="stitch-input stitch-select"
                    value={affOf(draft)}
                    disabled={isOwnship(draft)}
                    onChange={(e) => patchDraft('affiliation', e.target.value)}
                  >
                    {AFFILIATIONS.map((a) => (
                      <option key={a.id} value={a.id}>
                        {a.label}
                      </option>
                    ))}
                  </select>
                </label>

                <label className="block">
                  <span className="mb-1 block text-[10px] uppercase tracking-wider text-[var(--muted)]">
                    Platform type
                  </span>
                  <select
                    className="stitch-input stitch-select"
                    value={
                      isOwnship(draft)
                        ? 'aew'
                        : draft.platform === 'aircraft'
                          ? 'fighter'
                          : draft.platform || draft.entity_type || 'fighter'
                    }
                    disabled={isOwnship(draft)}
                    onChange={(e) => patchDraft('platform', e.target.value)}
                  >
                    {TRACK_PLATFORMS.map((p) => (
                      <option key={p.id} value={p.id}>
                        {p.label}
                      </option>
                    ))}
                  </select>
                </label>

                <div
                  className="pt-1 text-[10px] font-bold uppercase tracking-[0.16em] text-[var(--muted)]"
                  style={{ fontFamily: 'var(--font-display)' }}
                >
                  Kinematics
                </div>
                <div className="grid grid-cols-2 gap-2">
                  {[
                    ['lat', 'Lat', draft.lat, false],
                    ['lon', 'Lon', draft.lon, false],
                    ['alt_m', 'Alt m (MSL)', draft.alt_m ?? draft.alt ?? 0, false],
                    ['heading_deg', 'Hdg °', draft.heading_deg, false],
                  ].map(([key, label, val, locked]) => (
                    <label key={key} className="block">
                      <span className="mb-1 block text-[10px] uppercase tracking-wider text-[var(--muted)]">
                        {label}
                      </span>
                      <input
                        type="number"
                        step="any"
                        className="stitch-input"
                        value={val}
                        disabled={!!locked}
                        onChange={(e) => patchDraft(key, finiteNum(e.target.value))}
                      />
                    </label>
                  ))}
                  <label className="block">
                    <span className="mb-1 block text-[10px] uppercase tracking-wider text-[var(--muted)]">
                      Speed kt
                    </span>
                    <input
                      type="number"
                      step="1"
                      className="stitch-input"
                      value={Math.round(mpsToKt(draft.speed_mps) * 10) / 10}
                      onChange={(e) =>
                        patchDraft('speed_mps', ktToMps(finiteNum(e.target.value)), true)
                      }
                    />
                  </label>
                  <label className="block">
                    <span className="mb-1 block text-[10px] uppercase tracking-wider text-[var(--muted)]">
                      Climb m/s
                    </span>
                    <input
                      type="number"
                      step="0.1"
                      className="stitch-input"
                      value={draft.climb_mps ?? 0}
                      onChange={(e) =>
                        patchDraft('climb_mps', finiteNum(e.target.value), true)
                      }
                    />
                  </label>
                </div>
                <div
                  className="text-[10px] text-[var(--muted)]"
                  style={{ fontFamily: 'var(--font-mono)' }}
                >
                  ≈ FL{String(altToFL(draft.alt_m ?? draft.alt)).padStart(3, '0')}
                  {` · ${finiteNum(draft.speed_mps).toFixed(1)} m/s`}
                </div>

                {editorMode && (
                  <>
                    <div
                      className="pt-1 text-[10px] font-bold uppercase tracking-[0.16em] text-[var(--muted)]"
                      style={{ fontFamily: 'var(--font-display)' }}
                    >
                      Route waypoints
                    </div>
                    {(draft.route || []).length === 0 ? (
                      <p className="text-[10px] leading-relaxed text-[var(--muted)]">
                        No waypoints. Use Place Waypoint and click the map.
                      </p>
                    ) : (
                      <ul className="max-h-28 space-y-1 overflow-y-auto">
                        {(draft.route || []).map((wp, i) => (
                          <li
                            key={`wp-${i}`}
                            className="flex items-center justify-between gap-2 border border-[var(--line)] bg-black/20 px-2 py-1 text-[10px]"
                            style={{ fontFamily: 'var(--font-mono)' }}
                          >
                            <span className="text-[var(--muted)]">
                              WP{i + 1}{' '}
                              <span className="text-[var(--text)]">
                                {Number(wp.lat).toFixed(3)}, {Number(wp.lon).toFixed(3)}
                              </span>
                            </span>
                            <button
                              type="button"
                              className="text-[var(--hostile)] hover:underline"
                              onClick={() => {
                                onRemoveWaypoint?.(draft.id, i)
                                setDraft((prev) => {
                                  if (!prev) return prev
                                  const route = (prev.route || []).filter((_, idx) => idx !== i)
                                  return { ...prev, route, route_index: 0 }
                                })
                              }}
                            >
                              ×
                            </button>
                          </li>
                        ))}
                      </ul>
                    )}
                    <button
                      type="button"
                      className="w-full border border-[var(--line)] py-1.5 text-[10px] font-semibold uppercase tracking-wider text-[var(--muted)] hover:text-[var(--accent)]"
                      onClick={() => {
                        onClearRoute?.(draft.id)
                        setDraft((prev) =>
                          prev ? { ...prev, route: [], route_index: 0 } : prev,
                        )
                      }}
                      disabled={(draft.route || []).length === 0}
                    >
                      Clear route
                    </button>
                  </>
                )}

                <div
                  className="pt-1 text-[10px] font-bold uppercase tracking-[0.16em] text-[var(--muted)]"
                  style={{ fontFamily: 'var(--font-display)' }}
                >
                  IFF / SIF
                </div>
                <label className="flex items-center gap-2 text-xs text-[var(--muted)]">
                  <input
                    type="checkbox"
                    checked={!!draft.iff_enabled}
                    onChange={(e) => patchDraft('iff_enabled', e.target.checked)}
                  />
                  IFF enabled
                </label>
                <div className="grid grid-cols-2 gap-2">
                  <label className="block">
                    <span className="mb-1 block text-[10px] uppercase tracking-wider text-[var(--muted)]">
                      Mode
                    </span>
                    <select
                      className="stitch-input stitch-select"
                      value={draft.iff_mode || '3A'}
                      disabled={!draft.iff_enabled}
                      onChange={(e) => patchDraft('iff_mode', e.target.value)}
                    >
                      {IFF_MODES.map((m) => (
                        <option key={m} value={m}>
                          {m}
                        </option>
                      ))}
                    </select>
                  </label>
                  <label className="block">
                    <span className="mb-1 block text-[10px] uppercase tracking-wider text-[var(--muted)]">
                      Squawk (3/A)
                    </span>
                    <input
                      className="stitch-input"
                      value={draft.squawk || ''}
                      maxLength={4}
                      disabled={!draft.iff_enabled}
                      onChange={(e) =>
                        patchDraft(
                          'squawk',
                          e.target.value.replace(/[^0-7]/g, '').slice(0, 4),
                        )
                      }
                    />
                  </label>
                </div>
                <div className="flex flex-wrap gap-3 text-xs text-[var(--muted)]">
                  <label className="flex items-center gap-2">
                    <input
                      type="checkbox"
                      checked={!!draft.mode_c}
                      disabled={!draft.iff_enabled}
                      onChange={(e) => patchDraft('mode_c', e.target.checked)}
                    />
                    Mode C
                  </label>
                  <label className="flex items-center gap-2">
                    <input
                      type="checkbox"
                      checked={!!draft.mode_4}
                      disabled={!draft.iff_enabled}
                      onChange={(e) => patchDraft('mode_4', e.target.checked)}
                    />
                    Mode 4
                  </label>
                  <label className="flex items-center gap-2">
                    <input
                      type="checkbox"
                      checked={!!draft.mode_5}
                      disabled={!draft.iff_enabled}
                      onChange={(e) => patchDraft('mode_5', e.target.checked)}
                    />
                    Mode 5
                  </label>
                </div>

                <div className="mt-auto space-y-2 pt-2">
                  {dirty && (
                    <div
                      className="text-[10px] text-[var(--accent)]"
                      style={{ fontFamily: 'var(--font-mono)' }}
                    >
                      Saving…
                    </div>
                  )}
                  <button type="button" className="btn-update" onClick={commit}>
                    UPDATE TRACK
                  </button>
                  {!isOwnship(draft) && (
                    <button
                      type="button"
                      className="w-full border border-[rgba(255,77,94,0.35)] py-2 text-[11px] font-semibold uppercase tracking-wider text-[var(--hostile)]"
                      onClick={() => onDelete(draft.id)}
                    >
                      Delete track
                    </button>
                  )}
                </div>
              </div>
            )}
          </div>
        </>
      )}
    </aside>
  )
}

function Row({ k, v }) {
  return (
    <div className="flex justify-between gap-2 border-b border-[var(--line)]/60 py-1.5">
      <span className="text-[var(--muted)]">{k}</span>
      <span className="text-right text-[var(--text)]">{v}</span>
    </div>
  )
}
