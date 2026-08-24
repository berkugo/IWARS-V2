export const OWNSHIP_ID = 'ownship'
export const OWNSHIP_CALLSIGN = 'AEWC737'

export function isOwnship(e) {
  return !!(e && (e.ownship || e.id === OWNSHIP_ID || e.name === OWNSHIP_CALLSIGN))
}

export function makeOwnship(lat = 39.9334, lon = 32.8597) {
  return {
    id: OWNSHIP_ID,
    name: OWNSHIP_CALLSIGN,
    affiliation: 'friend',
    platform: 'aew',
    lat,
    lon,
    alt_m: 9500,
    heading_deg: 90,
    speed_mps: 0,
    climb_mps: 0,
    iff_enabled: true,
    iff_mode: '3A',
    squawk: '0001',
    mode_c: true,
    mode_4: true,
    mode_5: true,
    ownship: true,
    route: [],
    route_index: 0,
  }
}

/**
 * Air C4ISR track types for the UDP entity-truth feed.
 * Downstream radar/IFF stim produces blips from these kinematics + IFF fields.
 */

/** Platform / track category — air picture only */
export const PLATFORMS = [
  { id: 'fighter', label: 'Fighter', speed: 250, alt: 8000 },
  { id: 'attack', label: 'Attack', speed: 200, alt: 3000 },
  { id: 'bomber', label: 'Bomber', speed: 220, alt: 10000 },
  { id: 'transport', label: 'Transport', speed: 180, alt: 9000 },
  { id: 'tanker', label: 'Tanker', speed: 170, alt: 8500 },
  { id: 'aew', label: 'AEW&C', speed: 160, alt: 9500 },
  { id: 'isr', label: 'ISR / Recce', speed: 150, alt: 11000 },
  { id: 'uav', label: 'UAV', speed: 45, alt: 4500 },
  { id: 'ucav', label: 'UCAV', speed: 55, alt: 5000 },
  { id: 'helicopter', label: 'Helicopter', speed: 55, alt: 800 },
  { id: 'civil', label: 'Civil / Airliner', speed: 230, alt: 11000 },
  { id: 'unknown', label: 'Unknown', speed: 180, alt: 5000 },
  // Legacy aliases still accepted from older scenarios
  { id: 'aircraft', label: 'Aircraft (legacy)', speed: 200, alt: 6000 },
]

/** NATO air picture identity */
export const AFFILIATIONS = [
  { id: 'friend', label: 'Friend', short: 'FRD' },
  { id: 'assumed_friend', label: 'Assumed Friend', short: 'AFR' },
  { id: 'neutral', label: 'Neutral', short: 'NEU' },
  { id: 'suspect', label: 'Suspect', short: 'SUS' },
  { id: 'hostile', label: 'Hostile', short: 'HST' },
  { id: 'unknown', label: 'Unknown', short: 'UNK' },
]

/** Military IFF / SIF modes common in air C4ISR */
export const IFF_MODES = ['off', '1', '2', '3A', 'C', 'S', '4', '5']

export function platformOf(entity) {
  const id = entity?.platform || entity?.entity_type || 'fighter'
  return PLATFORMS.find((p) => p.id === id) || PLATFORMS.find((p) => p.id === 'unknown')
}

export function defaultEntity(id, lat, lon) {
  const p = PLATFORMS[0]
  return {
    id,
    name: id.toUpperCase(),
    affiliation: 'friend',
    platform: p.id,
    lat,
    lon,
    alt_m: p.alt,
    heading_deg: 0,
    speed_mps: p.speed,
    climb_mps: 0,
    iff_enabled: true,
    iff_mode: '3A',
    squawk: '1200',
    mode_c: true,
    mode_4: false,
    mode_5: false,
    route: [],
    route_index: 0,
  }
}

export function affiliationColor(aff) {
  if (aff === 'friend' || aff === 'blue' || aff === 'assumed_friend') return 'var(--friendly)'
  if (aff === 'hostile' || aff === 'red') return 'var(--hostile)'
  if (aff === 'neutral') return 'var(--neutral)'
  if (aff === 'suspect') return '#f59e0b'
  return '#fbbf24' // unknown
}

/** Altitude as Flight Level (FL = hundreds of feet, from meters) */
export function altToFL(alt_m) {
  const ft = Number(alt_m || 0) * 3.28084
  return Math.round(ft / 100)
}

/** Speed m/s → knots */
export function mpsToKt(mps) {
  return Number(mps || 0) * 1.94384
}

/** Knots → m/s */
export function ktToMps(kt) {
  return Number(kt || 0) / 1.94384
}
