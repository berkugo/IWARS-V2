import { useCallback, useEffect, useMemo, useRef, useState } from 'react'
import MapView from './components/MapView'
import TopBar from './components/TopBar'
import TrackSidebar from './components/TrackSidebar'
import MapChrome from './components/MapChrome'
import UdpConfigPage from './components/UdpConfigPage'
import ScenarioEditorPanel, { EditorDomainTabs } from './components/ScenarioEditorPanel'
import { useSimulation } from './hooks/useSimulation'
import { defaultEntity, makeOwnship, nextFighterIdentity, ANKARA, TURKEY_VIEW } from './radarTypes'

export default function App() {
  const sim = useSimulation()
  const [selectedId, setSelectedId] = useState(null)
  const [saveName, setSaveName] = useState('myscenario.json')
  const [scenarioName, setScenarioName] = useState('untitled')
  const [nav, setNav] = useState('simulation')
  const [tool, setTool] = useState('select')
  const [toast, setToast] = useState(null)
  const toastTimer = useRef(null)
  const lastWpClick = useRef({ t: 0, lat: 0, lon: 0 })
  const pendingRoutes = useRef(new Map())
  const [routeTick, setRouteTick] = useState(0)

  const flash = useCallback((msg) => {
    setToast(msg)
    if (toastTimer.current) clearTimeout(toastTimer.current)
    toastTimer.current = setTimeout(() => setToast(null), 2200)
  }, [])

  const setPendingRoute = useCallback((id, route) => {
    pendingRoutes.current.set(id, Array.isArray(route) ? route : [])
    setRouteTick((n) => n + 1)
  }, [])

  const entities = useMemo(() => {
    const list = sim.state.entities || []
    if (pendingRoutes.current.size === 0) return list
    return list.map((e) => {
      if (!pendingRoutes.current.has(e.id)) return e
      return {
        ...e,
        route: pendingRoutes.current.get(e.id),
        route_index: 0,
      }
    })
  }, [sim.state.entities, routeTick])

  const selected = entities.find((e) => e.id === selectedId)
  const isEditor = nav === 'editor'

  const scenarioKey = useMemo(
    () =>
      `${sim.state.name}|${sim.state.center_lat}|${sim.state.center_lon}|${sim.state.zoom}`,
    [sim.state.name, sim.state.center_lat, sim.state.center_lon, sim.state.zoom],
  )

  useEffect(() => {
    if (sim.state.name) setScenarioName(sim.state.name)
  }, [sim.state.name])

  useEffect(() => {
    lastWpClick.current = { t: 0, lat: 0, lon: 0 }
  }, [tool, selectedId])

  useEffect(() => {
    const server = sim.state.entities || []
    let changed = false
    for (const [id, route] of [...pendingRoutes.current.entries()]) {
      const e = server.find((x) => x.id === id)
      if (!e) {
        pendingRoutes.current.delete(id)
        changed = true
        continue
      }
      const sr = e.route || []
      if (
        sr.length === route.length &&
        sr.every(
          (w, i) =>
            Math.abs(w.lat - route[i].lat) < 1e-7 &&
            Math.abs(w.lon - route[i].lon) < 1e-7,
        )
      ) {
        pendingRoutes.current.delete(id)
        changed = true
      }
    }
    if (changed) setRouteTick((n) => n + 1)
  }, [sim.state.entities])
  useEffect(() => {
    if (isEditor && sim.state.playing) {
      sim.pause()
    }
    if (!isEditor) setTool('select')
  }, [isEditor]) // eslint-disable-line react-hooks/exhaustive-deps

  const handleMapClick = useCallback(
    async (lat, lon) => {
      if (isEditor && tool === 'add_track') {
        const { id, name } = nextFighterIdentity(entities)
        await sim.addEntity(defaultEntity(id, lat, lon, name))
        setSelectedId(id)
        setTool('select')
        flash(name)
        return
      }
      if (isEditor && tool === 'add_waypoint') {
        if (!selectedId) {
          flash('Select a track first')
          return
        }
        const e = entities.find((x) => x.id === selectedId)
        if (!e) return
        const last = lastWpClick.current
        const dt = Date.now() - last.t
        const distM =
          Math.sqrt((lat - last.lat) ** 2 + (lon - last.lon) ** 2) * 111320
        if (last.t && dt < 400 && distM < 800) {
          setTool('select')
          return
        }
        lastWpClick.current = { t: Date.now(), lat, lon }
        const route = [...(e.route || []), { lat, lon }]
        setPendingRoute(selectedId, route)
        await sim.updateEntity(selectedId, { ...e, route, route_index: 0 })
        return
      }
      // Simulation: optional quick-add via tool not used; ignore
    },
    [isEditor, tool, entities, sim, selectedId, flash, setPendingRoute],
  )

  const handleDrag = useCallback(
    async (id, lat, lon) => {
      const e = entities.find((x) => x.id === id)
      if (!e) return
      await sim.updateEntity(id, { ...e, lat, lon })
    },
    [entities, sim],
  )

  const handleChange = useCallback(
    async (entity) => {
      await sim.updateEntity(entity.id, entity)
    },
    [sim],
  )

  const handleDelete = useCallback(
    async (id) => {
      await sim.removeEntity(id)
      setSelectedId((cur) => (cur === id ? null : cur))
      flash('Track deleted')
    },
    [sim, flash],
  )

  const handleClearRoute = useCallback(
    async (id) => {
      const e = entities.find((x) => x.id === id)
      if (!e) return
      setPendingRoute(id, [])
      await sim.updateEntity(id, { ...e, route: [], route_index: 0 })
    },
    [entities, sim, setPendingRoute],
  )

  const handleRemoveWaypoint = useCallback(
    async (id, index) => {
      const e = entities.find((x) => x.id === id)
      if (!e) return
      const route = (e.route || []).filter((_, i) => i !== index)
      setPendingRoute(id, route)
      await sim.updateEntity(id, { ...e, route, route_index: 0 })
    },
    [entities, sim, setPendingRoute],
  )

  const handleSave = useCallback(async () => {
    let name = saveName.trim() || 'myscenario.json'
    if (!name.endsWith('.json')) name += '.json'
    const scenarioLabel = scenarioName.trim() || 'untitled'
    await sim.replaceScenario({ ...sim.state, name: scenarioLabel })
    await sim.saveScenario(name, scenarioLabel)
    setSaveName(name)
    flash(`Saved ${name}`)
  }, [saveName, scenarioName, sim, flash])

  const handleNew = useCallback(async () => {
    if (!window.confirm('Start a blank scenario? Unsaved map edits will be lost.')) return
    await sim.pause()
    await sim.replaceScenario({
      name: 'untitled',
      description: '',
      center_lat: TURKEY_VIEW.lat,
      center_lon: TURKEY_VIEW.lon,
      zoom: TURKEY_VIEW.zoom,
      udp: sim.state.udp || {
        host: '127.0.0.1',
        port: 9000,
        entity_port: 9000,
        ownship_port: 9001,
        enabled: false,
      },
      entities: [makeOwnship(ANKARA.lat, ANKARA.lon)],
    })
    setSelectedId('ownship')
    setScenarioName('untitled')
    setSaveName('untitled.json')
    setTool('add_track')
    flash('New scenario')
  }, [sim, flash])

  const handleLoad = useCallback(
    async (filename) => {
      await sim.loadScenario(filename)
      setSaveName(filename)
      setTool('select')
      flash(`Loaded ${filename}`)
    },
    [sim, flash],
  )

  const handleDeleteScenario = useCallback(
    async (filename) => {
      try {
        await sim.deleteScenario(filename)
        flash(`Deleted ${filename}`)
      } catch (e) {
        flash(e.message || `Could not delete ${filename}`)
      }
    },
    [sim, flash],
  )

  const handleNav = useCallback(
    (id) => {
      setNav(id)
      if (id === 'editor') setTool('select')
    },
    [],
  )

  useEffect(() => {
    if (!selectedId && entities.length > 0) {
      setSelectedId(entities[0].id)
    }
  }, [entities, selectedId])

  useEffect(() => {
    function onKey(e) {
      const el = e.target
      const typing =
        el instanceof HTMLElement &&
        (el.tagName === 'INPUT' ||
          el.tagName === 'TEXTAREA' ||
          el.tagName === 'SELECT' ||
          el.isContentEditable)
      if (e.key === 'Escape') {
        setTool('select')
        return
      }
      if (typing) return
      if (e.key === ' ' && nav === 'simulation') {
        e.preventDefault()
        if (sim.state.playing) sim.pause()
        else sim.play()
        return
      }
      if (nav !== 'editor') return
      if (e.key === '1') setTool('select')
      if (e.key === '2') setTool('add_track')
      if (e.key === '3') setTool('add_waypoint')
    }
    window.addEventListener('keydown', onKey)
    return () => window.removeEventListener('keydown', onKey)
  }, [nav, sim])

  return (
    <div className="flex h-full flex-col bg-[var(--bg-deep)]">
      <TopBar
        state={sim.state}
        connected={sim.connected}
        nav={nav}
        setNav={handleNav}
        onPlay={sim.play}
        onPause={sim.pause}
        onReset={sim.reset}
        selected={selected}
      />

      {sim.error && (
        <div className="shrink-0 border-b border-red-500/30 bg-red-950/80 px-4 py-1.5 text-xs text-red-200">
          Backend offline — start <code className="font-mono">iwars_sim</code>
          <span className="ml-2 opacity-70">{sim.error}</span>
        </div>
      )}

      {nav === 'udp' && (
        <UdpConfigPage
          udp={sim.state.udp}
          onUdpChange={sim.setUdp}
          connected={sim.connected}
        />
      )}

      {(nav === 'simulation' || nav === 'editor') && (
        <div className="flex min-h-0 flex-1">
          {isEditor ? (
            <div className="flex h-full min-h-0 shrink-0 flex-col">
              <EditorDomainTabs tab="track" />
              <div className="flex min-h-0 flex-1">
                <ScenarioEditorPanel
                  state={sim.state}
                  scenarios={sim.scenarios}
                  saveName={saveName}
                  setSaveName={setSaveName}
                  scenarioName={scenarioName}
                  setScenarioName={setScenarioName}
                  tool={tool}
                  setTool={setTool}
                  onLoad={handleLoad}
                  onSave={handleSave}
                  onNew={handleNew}
                  onDelete={handleDeleteScenario}
                />

                <TrackSidebar
                  entities={entities}
                  selectedId={selectedId}
                  onSelect={setSelectedId}
                  onChange={handleChange}
                  onDelete={handleDelete}
                  editorMode
                  readOnly={false}
                  onClearRoute={handleClearRoute}
                  onRemoveWaypoint={handleRemoveWaypoint}
                  scenarioKey={scenarioKey}
                />
              </div>
            </div>
          ) : (
            <TrackSidebar
              entities={entities}
              selectedId={selectedId}
              onSelect={setSelectedId}
              onChange={handleChange}
              onDelete={handleDelete}
              editorMode={false}
              readOnly
              onClearRoute={handleClearRoute}
              onRemoveWaypoint={handleRemoveWaypoint}
              scenarioKey={scenarioKey}
            />
          )}

          <main className="relative min-h-0 min-w-0 flex-1 overflow-hidden">
            <MapView
              entities={entities}
              center={[sim.state.center_lat, sim.state.center_lon]}
              zoom={sim.state.zoom}
              selectedId={selectedId}
              scenarioKey={scenarioKey}
              readOnly={!isEditor}
              cursor={
                isEditor && (tool === 'add_track' || tool === 'add_waypoint')
                  ? 'place'
                  : 'default'
              }
              tool={isEditor ? tool : 'select'}
              onSelect={(id) => {
                setSelectedId(id)
                if (isEditor && tool === 'add_track') setTool('select')
              }}
              onMapClick={isEditor ? handleMapClick : undefined}
              onFinishRoute={isEditor ? () => setTool('select') : undefined}
              onDragEntity={isEditor ? handleDrag : undefined}
            />
            <MapChrome
              selected={selected}
              mode={isEditor ? 'editor' : 'simulation'}
              tool={isEditor ? tool : 'select'}
            />
          </main>
        </div>
      )}

      {toast && <div className="app-toast">{toast}</div>}

      <footer className="app-footer flex h-8 shrink-0 items-center gap-4 px-4 text-[10px] text-[var(--muted)]">
        <span style={{ fontFamily: 'var(--font-mono)' }}>
          {sim.state.name || 'untitled'} · {entities.length} tracks
        </span>
        <span style={{ fontFamily: 'var(--font-mono)' }}>
          {nav === 'simulation'
            ? sim.state.playing
              ? 'PLAYING'
              : 'PAUSED'
            : nav === 'editor'
              ? 'EDITOR'
              : 'UDP'}
        </span>
        <span className="ml-auto" style={{ fontFamily: 'var(--font-mono)' }}>
          {nav === 'editor' ? 'Esc select · 1/2/3 tools' : 'Space play/pause'}
        </span>
      </footer>
    </div>
  )
}
