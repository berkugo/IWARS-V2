import { Fragment, memo, useEffect, useMemo, useRef, useState } from 'react'
import {
  MapContainer,
  Marker,
  Polyline,
  CircleMarker,
  GeoJSON,
  ZoomControl,
  useMap,
  useMapEvents,
} from 'react-leaflet'
import L from 'leaflet'
import { isOwnship } from '../radarTypes'
import { buildOwnshipIconSvg, buildTrackIconSvg } from '../trackIcons'

// Offline: never fetch Leaflet's default PNG markers (would 404 on air-gap).
delete L.Icon.Default.prototype._getIconUrl

const AFF_COLOR = {
  friend: '#3d9eff',
  blue: '#3d9eff',
  assumed_friend: '#5cb8ff',
  hostile: '#ff4d5e',
  red: '#ff4d5e',
  neutral: '#8b9bab',
  suspect: '#f59e0b',
  unknown: '#ffb020',
}

const BORDER_STYLE = {
  color: '#7af5dc',
  weight: 1.75,
  opacity: 0.95,
  fillColor: '#1a4a42',
  fillOpacity: 0.72,
}

/** Reuse Leaflet Icon instances — recreating every WS tick leaks / GC stalls. */
const iconCache = new Map()
const ICON_CACHE_MAX = 300

function cacheIcon(key, factory) {
  let icon = iconCache.get(key)
  if (icon) return icon
  icon = factory()
  iconCache.set(key, icon)
  if (iconCache.size > ICON_CACHE_MAX) {
    const oldest = iconCache.keys().next().value
    iconCache.delete(oldest)
  }
  return icon
}

function quantizeHeading(heading) {
  return Math.round(Number(heading) / 5) * 5
}

function entityIcon(platform, affiliation, selected, heading) {
  const color = AFF_COLOR[affiliation] || AFF_COLOR.unknown
  const h = quantizeHeading(heading)
  const key = `t:${platform}:${affiliation}:${selected ? 1 : 0}:${h}`
  return cacheIcon(key, () =>
    L.icon({
      iconUrl: `data:image/svg+xml,${encodeURIComponent(
        buildTrackIconSvg(platform, color, selected, h),
      )}`,
      iconSize: [48, 48],
      iconAnchor: [24, 24],
    }),
  )
}

function ownshipIcon(selected, heading) {
  const h = quantizeHeading(heading)
  const key = `o:${selected ? 1 : 0}:${h}`
  return cacheIcon(key, () =>
    L.icon({
      iconUrl: `data:image/svg+xml,${encodeURIComponent(buildOwnshipIconSvg(selected, h))}`,
      iconSize: [64, 64],
      iconAnchor: [32, 32],
    }),
  )
}

function MapSync({ center, zoom, scenarioKey }) {
  const map = useMap()
  useEffect(() => {
    if (!center) return
    map.setView(center, zoom ?? map.getZoom(), { animate: false })
  }, [scenarioKey]) // eslint-disable-line react-hooks/exhaustive-deps
  return null
}

function MapClick({ onMapClick, onFinishRoute, placeWaypoints }) {
  const clickCb = useRef(onMapClick)
  const finishCb = useRef(onFinishRoute)
  clickCb.current = onMapClick
  finishCb.current = onFinishRoute
  const map = useMap()

  useEffect(() => {
    if (placeWaypoints) map.doubleClickZoom.disable()
    else map.doubleClickZoom.enable()
    return () => map.doubleClickZoom.enable()
  }, [map, placeWaypoints])

  useMapEvents({
    click(e) {
      clickCb.current?.(e.latlng.lat, e.latlng.lng)
    },
    dblclick(e) {
      if (!placeWaypoints) return
      L.DomEvent.stop(e)
      finishCb.current?.()
    },
  })
  return null
}

function RouteGhost({ selected, tool }) {
  const [cursor, setCursor] = useState(null)
  const placing = tool === 'add_waypoint' && !!selected

  useMapEvents({
    mousemove(e) {
      if (placing) setCursor(e.latlng)
    },
    mouseout() {
      setCursor(null)
    },
  })

  if (!placing || !cursor) return null
  const route = selected.route || []
  const last = route.length
    ? route[route.length - 1]
    : { lat: selected.lat, lon: selected.lon }
  const color = isOwnship(selected) ? '#7af5dc' : AFF_COLOR[selected.affiliation || selected.side] || AFF_COLOR.unknown
  return (
    <Polyline
      positions={[
        [last.lat, last.lon],
        [cursor.lat, cursor.lng],
      ]}
      interactive={false}
      pathOptions={{
        color,
        weight: 1.5,
        opacity: 0.55,
        dashArray: '2 6',
      }}
    />
  )
}

function waypointIcon(n, color) {
  return L.divIcon({
    className: 'wp-marker',
    html: `<span style="background:${color}">${n}</span>`,
    iconSize: [18, 18],
    iconAnchor: [9, 9],
  })
}

function CountryBorders() {
  const [data, setData] = useState(null)
  useEffect(() => {
    let cancelled = false
    fetch('/countries.geojson')
      .then((r) => r.json())
      .then((g) => {
        if (!cancelled) setData(g)
      })
      .catch(() => {})
    return () => {
      cancelled = true
    }
  }, [])
  if (!data) return null
  return <GeoJSON data={data} style={() => BORDER_STYLE} interactive={false} />
}

const EntityLayer = memo(function EntityLayer({
  entity: e,
  selected,
  readOnly,
  tool,
  onSelect,
  onMapClick,
  onDragEntity,
}) {
  const aff = e.affiliation || e.side || 'unknown'
  const own = isOwnship(e)
  const color = own ? '#7af5dc' : AFF_COLOR[aff] || AFF_COLOR.unknown
  const placingWp = tool === 'add_waypoint'
  const route = useMemo(
    () => (e.route || []).map((w) => [w.lat, w.lon]),
    [e.route],
  )

  const icon = useMemo(
    () =>
      own
        ? ownshipIcon(selected, e.heading_deg)
        : entityIcon(e.platform || e.entity_type, aff, selected, e.heading_deg),
    [own, selected, e.heading_deg, e.platform, e.entity_type, aff],
  )

  const handlers = useMemo(
    () => ({
      click: (ev) => {
        if (placingWp) {
          L.DomEvent.stopPropagation(ev.originalEvent)
          if (!selected) {
            onSelect?.(e.id)
            return
          }
          const map = ev.target._map
          if (map && onMapClick) {
            const ll = map.mouseEventToLatLng(ev.originalEvent)
            onMapClick(ll.lat, ll.lng)
          }
          return
        }
        if (tool === 'add_track') return
        onSelect?.(e.id)
      },
      dragend: (ev) => {
        if (readOnly) return
        const { lat, lng } = ev.target.getLatLng()
        onDragEntity?.(e.id, lat, lng)
      },
    }),
    [e.id, onSelect, onDragEntity, onMapClick, readOnly, placingWp, tool, selected],
  )

  const linePositions = useMemo(() => {
    if (route.length === 0) return null
    const loop = route.length >= 2 ? [...route, route[0]] : route
    return [[e.lat, e.lon], ...loop]
  }, [route, e.lat, e.lon])

  const wpIcons = useMemo(
    () => route.map((_, i) => waypointIcon(i + 1, color)),
    [route.length, color],
  )

  return (
    <Fragment>
      {linePositions && (
        <Polyline
          positions={linePositions}
          interactive={false}
          pathOptions={{
            color,
            weight: selected ? 2.5 : 1.5,
            opacity: selected ? 0.9 : 0.4,
            dashArray: selected ? '6 8' : '4 8',
          }}
        />
      )}
      {route.map((pos, i) =>
        selected ? (
          <Marker
            key={`${e.id}-wp-${i}`}
            position={pos}
            icon={wpIcons[i]}
            interactive={false}
            keyboard={false}
            zIndexOffset={-200}
          />
        ) : (
          <CircleMarker
            key={`${e.id}-wp-${i}`}
            center={pos}
            radius={3}
            interactive={false}
            pathOptions={{
              color,
              fillColor: color,
              fillOpacity: 0.65,
              weight: 1,
              opacity: 0.7,
            }}
          />
        ),
      )}
      <Marker
        position={[e.lat, e.lon]}
        icon={icon}
        draggable={!readOnly && !placingWp}
        eventHandlers={handlers}
      />
    </Fragment>
  )
})

function MapView({
  entities,
  center,
  zoom,
  selectedId,
  scenarioKey,
  readOnly = false,
  cursor = 'default',
  tool = 'select',
  onSelect,
  onMapClick,
  onFinishRoute,
  onDragEntity,
}) {
  const mapCenter = useMemo(
    () => [center?.[0] ?? 38.9637, center?.[1] ?? 35.2433],
    [center],
  )
  const selected = useMemo(
    () => entities.find((e) => e.id === selectedId) || null,
    [entities, selectedId],
  )

  return (
    <MapContainer
      center={mapCenter}
      zoom={zoom ?? 6}
      minZoom={2}
      maxZoom={12}
      className={`h-full w-full ${cursor === 'place' ? 'map-cursor-place' : ''}`}
      zoomControl={false}
      preferCanvas
    >
      <ZoomControl position="bottomleft" />
      <CountryBorders />
      <MapSync center={mapCenter} zoom={zoom} scenarioKey={scenarioKey} />
      {!readOnly && (
        <MapClick
          onMapClick={onMapClick}
          onFinishRoute={onFinishRoute}
          placeWaypoints={tool === 'add_waypoint'}
        />
      )}
      {!readOnly && tool === 'add_waypoint' && (
        <RouteGhost selected={selected} tool={tool} />
      )}
      {entities.map((e) => (
        <EntityLayer
          key={e.id}
          entity={e}
          selected={e.id === selectedId}
          readOnly={readOnly}
          tool={tool}
          onSelect={onSelect}
          onMapClick={onMapClick}
          onDragEntity={onDragEntity}
        />
      ))}
    </MapContainer>
  )
}

export default memo(MapView)
