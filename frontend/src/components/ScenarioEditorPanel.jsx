const TOOLS = [
  {
    id: 'select',
    label: 'Select',
    hint: 'Select tracks on the map or list',
  },
  {
    id: 'add_track',
    label: 'Place Track',
    hint: 'Click the map to add an air track',
  },
  {
    id: 'add_waypoint',
    label: 'Place Waypoint',
    hint: 'Select a track, then click the map to append route waypoints',
  },
]

export default function ScenarioEditorPanel({
  state,
  scenarios,
  saveName,
  setSaveName,
  scenarioName,
  setScenarioName,
  tool,
  setTool,
  onLoad,
  onSave,
  onNew,
}) {
  const trackCount = (state.entities || []).filter((e) => !e.ownship).length
  const wpCount = (state.entities || []).reduce(
    (n, e) => n + (e.ownship ? 0 : (e.route || []).length),
    0,
  )

  return (
    <div className="flex w-72 shrink-0 flex-col border-r border-[var(--line)] bg-[var(--bg-panel)]">
      <div className="border-b border-[var(--line)] px-3 py-3">
        <div
          className="text-[10px] font-bold uppercase tracking-[0.18em] text-[var(--accent)]"
          style={{ fontFamily: 'var(--font-display)' }}
        >
          Scenario Editor
        </div>
        <p className="mt-1 text-xs leading-relaxed text-[var(--muted)]">
          Create a scenario, place air tracks, then draw waypoints. Run it from
          Simulation.
        </p>
      </div>

      <div className="space-y-3 border-b border-[var(--line)] p-3">
        <label className="block">
          <span className="mb-1 block text-[10px] uppercase tracking-wider text-[var(--muted)]">
            Scenario name
          </span>
          <input
            className="stitch-input"
            value={scenarioName}
            onChange={(e) => setScenarioName(e.target.value)}
            placeholder="my_air_picture"
          />
        </label>
        <label className="block">
          <span className="mb-1 block text-[10px] uppercase tracking-wider text-[var(--muted)]">
            Save as file
          </span>
          <input
            className="stitch-input"
            value={saveName}
            onChange={(e) => setSaveName(e.target.value)}
            placeholder="myscenario.json"
          />
        </label>
        <div className="flex gap-2">
          <button type="button" className="btn-update flex-1" onClick={onSave}>
            Save
          </button>
          <button
            type="button"
            className="flex-1 border border-[var(--line)] py-2 text-[11px] font-semibold uppercase tracking-wider text-[var(--muted)] hover:text-[var(--accent)]"
            onClick={onNew}
          >
            New
          </button>
        </div>
      </div>

      <div className="space-y-2 border-b border-[var(--line)] p-3">
        <div
          className="text-[10px] font-bold uppercase tracking-[0.16em] text-[var(--muted)]"
          style={{ fontFamily: 'var(--font-display)' }}
        >
          Authoring tools
        </div>
        <div className="flex flex-col gap-1.5">
          {TOOLS.map((t) => (
            <button
              key={t.id}
              type="button"
              onClick={() => setTool(t.id)}
              className={`w-full border px-2 py-2 text-left text-[11px] font-semibold uppercase tracking-wider ${
                tool === t.id
                  ? 'border-[var(--accent)] bg-[var(--accent-dim)] text-[var(--accent)]'
                  : 'border-[var(--line)] text-[var(--muted)] hover:text-[var(--text)]'
              }`}
            >
              {t.label}
            </button>
          ))}
        </div>
        <p className="text-[10px] leading-relaxed text-[var(--muted)]">
          {TOOLS.find((t) => t.id === tool)?.hint}
        </p>
        <div
          className="text-[10px] text-[var(--muted)]"
          style={{ fontFamily: 'var(--font-mono)' }}
        >
          Tracks {trackCount} · Waypoints {wpCount}
        </div>
      </div>

      <div className="min-h-0 flex-1 overflow-y-auto p-2">
        <div
          className="mb-2 px-1 text-[10px] font-bold uppercase tracking-[0.16em] text-[var(--muted)]"
          style={{ fontFamily: 'var(--font-display)' }}
        >
          Saved files
        </div>
        {scenarios.length === 0 && (
          <p className="px-2 py-6 text-center text-xs text-[var(--muted)]">
            No saved scenarios yet
          </p>
        )}
        <ul className="space-y-1">
          {scenarios.map((f) => {
            const active = state.name && f.startsWith(state.name)
            return (
              <li key={f}>
                <button
                  type="button"
                  onClick={() => onLoad(f)}
                  className={`w-full truncate rounded-sm border px-2 py-2 text-left text-xs ${
                    active
                      ? 'border-[var(--accent)] bg-[var(--accent-dim)] text-[var(--accent)]'
                      : 'border-transparent bg-[var(--bg-row)] text-[var(--text)] hover:border-[var(--line)]'
                  }`}
                  style={{ fontFamily: 'var(--font-mono)' }}
                >
                  {f}
                </button>
              </li>
            )
          })}
        </ul>
      </div>

      <div
        className="border-t border-[var(--line)] px-3 py-2 text-[10px] text-[var(--muted)]"
        style={{ fontFamily: 'var(--font-mono)' }}
      >
        Active: {state.name || 'untitled'} · OWN + {trackCount} tracks
      </div>
    </div>
  )
}
