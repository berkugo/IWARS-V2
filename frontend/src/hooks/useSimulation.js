import { useCallback, useEffect, useRef, useState } from 'react'
import { api, wsUrl } from '../api'

const emptyState = {
  name: 'untitled',
  description: '',
  center_lat: 39.9334,
  center_lon: 32.8597,
  zoom: 11,
  udp: { host: '127.0.0.1', port: 9000, enabled: false },
  entities: [],
  playing: false,
  sim_time: 0,
}

function entitiesFingerprint(entities) {
  if (!Array.isArray(entities)) return ''
  // Coarse — enough to skip identical pause heartbeats
  return entities
    .map(
      (e) =>
        `${e.id}:${Number(e.lat).toFixed(5)},${Number(e.lon).toFixed(5)},${Number(
          e.heading_deg,
        ).toFixed(0)},${Number(e.speed_mps).toFixed(1)},${Number(e.alt_m).toFixed(0)},${
          (e.route || []).length
        }`,
    )
    .join('|')
}

export function useSimulation() {
  const [state, setState] = useState(emptyState)
  const [connected, setConnected] = useState(false)
  const [error, setError] = useState(null)
  const [scenarios, setScenarios] = useState([])
  const wsRef = useRef(null)
  const lastFp = useRef('')
  const lastSeq = useRef(0)

  function acceptSeq(data) {
    const seq = Number(data?.seq)
    if (!Number.isFinite(seq) || seq <= 0) return true
    if (seq <= lastSeq.current) return false
    lastSeq.current = seq
    return true
  }

  const refreshScenarios = useCallback(async () => {
    try {
      const list = await api.listScenarios()
      setScenarios(list)
    } catch (e) {
      console.warn(e)
    }
  }, [])

  useEffect(() => {
    let cancelled = false
    let retry
    let poll
    let scenarioPoll

    async function bootstrap() {
      let wsPort = 8081
      try {
        const h = await api.health()
        if (typeof h?.ws_port === 'number') wsPort = h.ws_port
        const s = await api.state()
        if (!cancelled) {
          acceptSeq(s)
          setState((prev) => ({ ...prev, ...s }))
          lastFp.current = entitiesFingerprint(s.entities)
          setError(null)
        }
      } catch (e) {
        if (!cancelled) setError(e.message)
      }
      refreshScenarios()
      if (!cancelled) connect(wsPort)
    }

    function applyIncoming(data) {
      if (!acceptSeq(data)) return
      setState((prev) => {
        const playing = !!data.playing
        if (!playing && !prev.playing) {
          const fp = entitiesFingerprint(data.entities)
          if (
            fp === lastFp.current &&
            prev.name === data.name &&
            prev.udp?.host === data.udp?.host &&
            prev.udp?.port === data.udp?.port &&
            !!prev.udp?.enabled === !!data.udp?.enabled
          ) {
            if (prev.sim_time === data.sim_time) return prev
            return { ...prev, sim_time: data.sim_time, playing: false }
          }
          lastFp.current = fp
        } else if (playing) {
          lastFp.current = entitiesFingerprint(data.entities)
        }
        return { ...prev, ...data }
      })
    }

    function connect(wsPort) {
      const ws = new WebSocket(wsUrl(wsPort))
      wsRef.current = ws
      ws.onopen = () => {
        if (!cancelled) setConnected(true)
      }
      ws.onclose = () => {
        if (!cancelled) {
          setConnected(false)
          retry = setTimeout(() => connect(wsPort), 1500)
        }
      }
      ws.onerror = () => ws.close()
      ws.onmessage = (ev) => {
        try {
          applyIncoming(JSON.parse(ev.data))
          if (!cancelled) setError(null)
        } catch {
          /* ignore */
        }
      }
    }

    bootstrap()

    // Only poll HTTP when WS is down (avoids duplicate 10Hz + 0.5Hz churn)
    poll = setInterval(async () => {
      if (wsRef.current?.readyState === WebSocket.OPEN) return
      try {
        const s = await api.state()
        if (!cancelled) {
          applyIncoming(s)
          setError(null)
        }
      } catch (e) {
        if (!cancelled) setError(e.message || 'Backend unreachable')
      }
    }, 3000)

    scenarioPoll = setInterval(() => {
      if (!cancelled) refreshScenarios()
    }, 15000)

    return () => {
      cancelled = true
      clearTimeout(retry)
      clearInterval(poll)
      clearInterval(scenarioPoll)
      wsRef.current?.close()
    }
  }, [refreshScenarios])

  const applyState = (data) => {
    if (!acceptSeq(data)) return
    lastFp.current = entitiesFingerprint(data.entities)
    setState((prev) => ({ ...prev, ...data }))
  }

  return {
    state,
    connected,
    error,
    scenarios,
    refreshScenarios,
    play: async () => {
      setState((prev) => ({ ...prev, playing: true }))
      try {
        applyState(await api.play())
      } catch (e) {
        setState((prev) => ({ ...prev, playing: false }))
        throw e
      }
    },
    pause: async () => {
      setState((prev) => ({ ...prev, playing: false }))
      try {
        applyState(await api.pause())
      } catch (e) {
        setState((prev) => ({ ...prev, playing: true }))
        throw e
      }
    },
    reset: async () => {
      applyState(await api.reset())
    },
    setUdp: async (udp) => {
      const next = await api.setUdp(udp)
      setState((prev) => ({ ...prev, udp: next }))
    },
    addEntity: async (entity) => {
      applyState(await api.addEntity(entity))
    },
    updateEntity: async (id, entity) => {
      applyState(await api.updateEntity(id, entity))
    },
    removeEntity: async (id) => {
      applyState(await api.removeEntity(id))
    },
    replaceScenario: async (scenario) => {
      applyState(await api.replaceScenario(scenario))
    },
    loadScenario: async (filename) => {
      applyState(await api.loadScenario(filename))
      await refreshScenarios()
    },
    saveScenario: async (filename, nameOverride) => {
      await api.saveScenario(filename, nameOverride ?? state.name)
      await refreshScenarios()
    },
  }
}
