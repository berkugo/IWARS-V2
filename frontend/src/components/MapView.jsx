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

function MapClick({ onMapClick }) {
  const cb = useRef(onMapClick)
  cb.current = onMapClick
  useMapEvents({
    click(e) {
      cb.current?.(e.latlng.lat, e.latlng.lng)
    },
  })
  return null
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
  onSelect,
  onDragEntity,
}) {
  const aff = e.affiliation || e.side || 'unknown'
  const own = isOwnship(e)
  const color = own ? '#7af5dc' : AFF_COLOR[aff] || AFF_COLOR.unknown
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
      click: () => onSelect?.(e.id),
      dragend: (ev) => {
        if (readOnly) return
        const { lat, lng } = ev.target.getLatLng()
        onDragEntity?.(e.id, lat, lng)
      },
    }),
    [e.id, onSelect, onDragEntity, readOnly],
  )

  const linePositions = useMemo(() => {
    if (route.length === 0) return null
    if (route.length === 1) return [[e.lat, e.lon], route[0]]
    return route
  }, [route, e.lat, e.lon])

  return (
    <Fragment>
      {linePositions && (
        <>
          <Polyline
            positions={linePositions}
            pathOptions={{
              color,
              weight: selected ? 2.5 : 1.5,
              opacity: selected ? 0.85 : 0.45,
              dashArray: '4 8',
            }}
          />
          {route.map((pos, i) => (
            <CircleMarker
              key={`${e.id}-wp-${i}`}
              center={pos}
              radius={3}
              pathOptions={{
                color,
                fillColor: color,
                fillOpacity: 0.7,
                weight: 1,
                opacity: 0.8,
              }}
            />
          ))}
        </>
      )}
      <Marker
        position={[e.lat, e.lon]}
        icon={icon}
        draggable={!readOnly}
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
  onSelect,
  onMapClick,
  onDragEntity,
}) {
  const mapCenter = useMemo(
    () => [center?.[0] ?? 39.9334, center?.[1] ?? 32.8597],
    [center],
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
      <ZoomControl position="bottomright" />
      <CountryBorders />
      <MapSync center={mapCenter} zoom={zoom} scenarioKey={scenarioKey} />
      {!readOnly && <MapClick onMapClick={onMapClick} />}
      {entities.map((e) => (
        <EntityLayer
          key={e.id}
          entity={e}
          selected={e.id === selectedId}
          readOnly={readOnly}
          onSelect={onSelect}
          onDragEntity={onDragEntity}
        />
      ))}
    </MapContainer>
  )
}

export default memo(MapView)
