const TOOLS = [
  {
    id: 'select',
    label: 'Select',
    key: '1',
    hint: 'Select tracks on the map or list',
  },
  {
    id: 'add_track',
    label: 'Place Track',
    key: '2',
    hint: 'Click the map to add an air track',
  },
  {
    id: 'add_waypoint',
    label: 'Place Waypoint',
    key: '3',
    hint: 'Select a track, click the map to append waypoints. Double-click finishes.',
  },
]

export function EditorDomainTabs({ tab = 'track' }) {
  return (
    <div className="editor-tabs" role="tablist" aria-label="Scenario editor domain">
      <button
        type="button"
        role="tab"
        aria-selected={tab === 'track'}
        className={`editor-tab ${tab === 'track' ? 'active' : ''}`}
      >
        Track
      </button>
      <button
        type="button"
        role="tab"
        aria-selected={false}
        disabled
        className="editor-tab"
        title="LOB / ESM authoring is planned — not available yet"
      >
        <span>LOB / ESM</span>
        <span className="editor-tab-soon">Soon</span>
      </button>
    </div>
  )
}

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
  onDelete,
}) {
  const trackCount = (state.entities || []).filter((e) => !e.ownship).length
  const wpCount = (state.entities || []).reduce(
    (n, e) => n + (e.route || []).length,
    0,
  )
  const files = [...(scenarios || [])].sort((a, b) => a.localeCompare(b))

  return (
    <div className="panel-rail flex w-72 shrink-0 flex-col bg-[var(--bg-panel)]">
      <div className="border-b border-[var(--line)] px-3 py-3">
        <div className="section-kicker text-[var(--accent)]">Scenario Editor</div>
        <p className="mt-1.5 text-xs leading-relaxed text-[var(--muted)]">
          Place tracks, draw routes, save. Run from Simulation.
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
          <button type="button" className="btn-ghost flex-1" onClick={onNew}>
            New
          </button>
        </div>
      </div>

      <div className="space-y-2 border-b border-[var(--line)] p-3">
        <div className="section-kicker">Authoring tools</div>
        <div className="flex flex-col gap-1.5">
          {TOOLS.map((t) => (
            <button
              key={t.id}
              type="button"
              onClick={() => setTool(t.id)}
              className={`flex w-full items-center justify-between border px-2 py-2 text-left text-[11px] font-semibold uppercase tracking-wider ${
                tool === t.id
                  ? 'border-[var(--accent)] bg-[var(--accent-dim)] text-[var(--accent)]'
                  : 'border-[var(--line)] text-[var(--muted)] hover:text-[var(--text)]'
              }`}
            >
              <span>{t.label}</span>
              <span
                className="text-[10px] font-normal tracking-normal opacity-70"
                style={{ fontFamily: 'var(--font-mono)' }}
              >
                {t.key}
              </span>
            </button>
          ))}
        </div>
        <p className="text-[10px] leading-relaxed text-[var(--muted)]">
          {TOOLS.find((t) => t.id === tool)?.hint} Esc returns to Select.
        </p>
        <div
          className="text-[10px] text-[var(--muted)]"
          style={{ fontFamily: 'var(--font-mono)' }}
        >
          Tracks {trackCount} · Waypoints {wpCount}
        </div>
      </div>

      <div className="min-h-0 flex-1 overflow-y-auto p-2">
        <div className="section-kicker mb-2 px-1">Saved files</div>
        {files.length === 0 && (
          <p className="px-2 py-6 text-center text-xs text-[var(--muted)]">
            No saved scenarios yet
          </p>
        )}
        <ul className="space-y-1">
          {files.map((f) => {
            const stem = f.replace(/\.json$/i, '')
            const active = f === saveName || stem === state.name
            return (
              <li key={f} className="flex items-stretch gap-1">
                <button
                  type="button"
                  title={`Load ${f}`}
                  onClick={() => {
                    if (
                      window.confirm(
                        `Load ${f}? Unsaved editor changes will be replaced.`,
                      )
                    ) {
                      onLoad(f)
                    }
                  }}
                  className={`min-w-0 flex-1 truncate rounded-sm border px-2 py-2 text-left text-xs ${
                    active
                      ? 'border-[var(--accent)] bg-[var(--accent-dim)] text-[var(--accent)]'
                      : 'border-transparent bg-[var(--bg-row)] text-[var(--text)] hover:border-[var(--line)]'
                  }`}
                  style={{ fontFamily: 'var(--font-mono)' }}
                >
                  {f}
                </button>
                <button
                  type="button"
                  title={`Delete ${f}`}
                  aria-label={`Delete ${f}`}
                  onClick={() => {
                    if (
                      window.confirm(
                        `Delete ${f} from disk? This cannot be undone. The live picture stays until you load or New.`,
                      )
                    ) {
                      onDelete?.(f)
                    }
                  }}
                  className="shrink-0 border border-[rgba(255,77,94,0.28)] px-2 text-[13px] leading-none text-[var(--hostile)] hover:border-[var(--hostile)] hover:bg-[rgba(255,77,94,0.12)]"
                >
                  ×
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
