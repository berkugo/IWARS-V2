const API = '/api'

async function request(path, options = {}) {
  const res = await fetch(`${API}${path}`, {
    headers: { 'Content-Type': 'application/json', ...(options.headers || {}) },
    ...options,
  })
  const text = await res.text()
  let data = null
  try {
    data = text ? JSON.parse(text) : null
  } catch {
    data = text
  }
  if (!res.ok) {
    const msg = data?.error || res.statusText
    throw new Error(msg)
  }
  return data
}

export const api = {
  health: () => request('/health'),
  state: () => request('/state'),
  play: () => request('/control/play', { method: 'POST' }),
  pause: () => request('/control/pause', { method: 'POST' }),
  reset: () => request('/control/reset', { method: 'POST' }),
  setUdp: (udp) => request('/udp', { method: 'PUT', body: JSON.stringify(udp) }),
  addEntity: (entity) =>
    request('/entities', { method: 'POST', body: JSON.stringify(entity) }),
  updateEntity: (id, entity) =>
    request(`/entities/${encodeURIComponent(id)}`, {
      method: 'PUT',
      body: JSON.stringify(entity),
    }),
  removeEntity: (id) =>
    request(`/entities/${encodeURIComponent(id)}`, { method: 'DELETE' }),
  replaceScenario: (scenario) =>
    request('/scenario', { method: 'PUT', body: JSON.stringify(scenario) }),
  listScenarios: () => request('/scenarios'),
  loadScenario: (filename) =>
    request(`/scenarios/${encodeURIComponent(filename)}`),
  saveScenario: (filename, name) =>
    request('/scenarios', {
      method: 'POST',
      body: JSON.stringify({ filename, name }),
    }),
}

export function wsUrl() {
  const proto = window.location.protocol === 'https:' ? 'wss' : 'ws'
  // Proxy through Vite in dev so host matches the page (remote browsers too)
  return `${proto}://${window.location.host}/ws`
}
