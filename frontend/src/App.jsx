import { useCallback, useEffect, useMemo, useRef, useState } from 'react'
import MapView from './components/MapView'
import TopBar from './components/TopBar'
import TrackSidebar from './components/TrackSidebar'
import MapChrome from './components/MapChrome'
import UdpConfigPage from './components/UdpConfigPage'
import ScenarioEditorPanel from './components/ScenarioEditorPanel'
import { useSimulation } from './hooks/useSimulation'
import { defaultEntity, makeOwnship } from './radarTypes'

function newEntityId(entities) {
  let n = entities.length + 1
  let id = `tgt-${n}`
  const ids = new Set(entities.map((e) => e.id))
  while (ids.has(id)) {
    n += 1
    id = `tgt-${n}`
  }
  return id
}

export default function App() {
  const sim = useSimulation()
  const [selectedId, setSelectedId] = useState(null)
  const [saveName, setSaveName] = useState('myscenario.json')
  const [scenarioName, setScenarioName] = useState('untitled')
  const [nav, setNav] = useState('simulation')
  const [tool, setTool] = useState('select')
  const [toast, setToast] = useState(null)
  const toastTimer = useRef(null)

  const flash = useCallback((msg) => {
    setToast(msg)
    if (toastTimer.current) clearTimeout(toastTimer.current)
    toastTimer.current = setTimeout(() => setToast(null), 2200)
  }, [])

  const entities = sim.state.entities || []
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

  // Pause playback while authoring
  useEffect(() => {
    if (isEditor && sim.state.playing) {
      sim.pause()
    }
    if (!isEditor) setTool('select')
  }, [isEditor]) // eslint-disable-line react-hooks/exhaustive-deps

  const handleMapClick = useCallback(
    async (lat, lon) => {
      if (isEditor && tool === 'add_track') {
        const id = newEntityId(entities)
        await sim.addEntity(defaultEntity(id, lat, lon))
        setSelectedId(id)
        setTool('select')
        return
      }
      if (isEditor && tool === 'add_waypoint') {
        if (!selectedId) return
        const e = entities.find((x) => x.id === selectedId)
        if (!e) return
        const route = [...(e.route || []), { lat, lon }]
        await sim.updateEntity(selectedId, { ...e, route, route_index: 0 })
        return
      }
      // Simulation: optional quick-add via tool not used; ignore
    },
    [isEditor, tool, entities, sim, selectedId],
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
      await sim.updateEntity(id, { ...e, route: [], route_index: 0 })
    },
    [entities, sim],
  )

  const handleRemoveWaypoint = useCallback(
    async (id, index) => {
      const e = entities.find((x) => x.id === id)
      if (!e) return
      const route = (e.route || []).filter((_, i) => i !== index)
      await sim.updateEntity(id, { ...e, route, route_index: 0 })
    },
    [entities, sim],
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
    const centerLat = 39.9334
    const centerLon = 32.8597
    await sim.replaceScenario({
      name: 'untitled',
      description: '',
      center_lat: centerLat,
      center_lon: centerLon,
      zoom: 6,
      udp: sim.state.udp || { host: '127.0.0.1', port: 9000, enabled: false },
      entities: [makeOwnship(centerLat, centerLon)],
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
            />
          ) : null}

          <TrackSidebar
            entities={entities}
            selectedId={selectedId}
            onSelect={setSelectedId}
            onChange={handleChange}
            onDelete={handleDelete}
            editorMode={isEditor}
            readOnly={!isEditor}
            onClearRoute={handleClearRoute}
            onRemoveWaypoint={handleRemoveWaypoint}
            scenarioKey={scenarioKey}
          />

          <main className="relative min-w-0 flex-1">
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
              onSelect={(id) => {
                setSelectedId(id)
                if (isEditor && tool === 'add_track') setTool('select')
              }}
              onMapClick={isEditor ? handleMapClick : undefined}
              onDragEntity={isEditor ? handleDrag : undefined}
            />
            <MapChrome
              state={sim.state}
              selected={selected}
              connected={sim.connected}
              mode={isEditor ? 'editor' : 'simulation'}
              tool={isEditor ? tool : 'select'}
            />
          </main>
        </div>
      )}

      {toast && <div className="app-toast">{toast}</div>}

      <footer className="flex h-8 shrink-0 items-center gap-4 border-t border-[var(--line)] bg-[var(--bg-panel)] px-4 text-[10px] text-[var(--muted)]">
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
