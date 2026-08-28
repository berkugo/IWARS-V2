/**
 * Top-down aircraft silhouettes for air-picture markers.
 * Coordinate space: 48×48, nose toward +Y-up (then rotated by heading).
 */

const INK = '#05070a'

function selRing(color, selected) {
  if (!selected) return ''
  return `<circle cx="24" cy="24" r="22" fill="none" stroke="${color}" stroke-width="1.4" opacity="0.5"/>`
}

/** Fighter / attack — swept wing, twin fins, pointed nose */
function silhouetteFighter(c) {
  return `
    <path d="M24 6 L26.2 16 L27 22 L40 28 L40 30 L27 28 L27.5 36 L32 40 L32 41.5 L24 38.5 L16 41.5 L16 40 L20.5 36 L21 28 L8 30 L8 28 L21 22 L21.8 16 Z"
      fill="${c}" stroke="${INK}" stroke-width="1.1" stroke-linejoin="round"/>
    <ellipse cx="24" cy="20" rx="1.6" ry="5" fill="${INK}" opacity="0.35"/>
  `
}

/** Bomber — large swept planform */
function silhouetteBomber(c) {
  return `
    <path d="M24 5 L27 14 L28 20 L44 30 L44 32.5 L28 29 L29 38 L34 42 L24 39 L14 42 L19 38 L20 29 L4 32.5 L4 30 L20 20 L21 14 Z"
      fill="${c}" stroke="${INK}" stroke-width="1.1" stroke-linejoin="round"/>
    <ellipse cx="24" cy="22" rx="2" ry="7" fill="${INK}" opacity="0.25"/>
  `
}

/** Transport / tanker / civil — airliner planform */
function silhouetteAirliner(c) {
  return `
    <path d="M8 26 L24 22 L40 26 L40 28.5 L24 26.5 L24 36 L30 40 L30 41.5 L24 39 L18 41.5 L18 40 L24 36 L24 26.5 L8 28.5 Z"
      fill="${c}" stroke="${INK}" stroke-width="1.1" stroke-linejoin="round"/>
    <ellipse cx="24" cy="24" rx="2.4" ry="12" fill="${c}" stroke="${INK}" stroke-width="1.1"/>
    <path d="M24 12 L26 16 L22 16 Z" fill="${INK}"/>
    <path d="M22.5 35 L24 42 L25.5 35 Z" fill="${c}" stroke="${INK}" stroke-width="0.9"/>
  `
}

/** AEW&C — airliner; rotodome on aft fuselage (behind wings) */
function silhouetteAew(c) {
  return `
    <path d="M8 24 L24 20 L40 24 L40 26.5 L24 24 L24 34 L30 39 L30 40.5 L24 38 L18 40.5 L18 39 L24 34 L24 24 L8 26.5 Z"
      fill="${c}" stroke="${INK}" stroke-width="1.1" stroke-linejoin="round"/>
    <ellipse cx="24" cy="24" rx="2.4" ry="13" fill="${c}" stroke="${INK}" stroke-width="1.1"/>
    <path d="M24 10 L26.4 15 L21.6 15 Z" fill="${INK}"/>
    <!-- rotodome aft of wing -->
    <ellipse cx="24" cy="32.5" rx="8.5" ry="3.6" fill="${INK}" stroke="${c}" stroke-width="1.55"/>
    <ellipse cx="24" cy="32.5" rx="4.5" ry="1.6" fill="${c}" opacity="0.4"/>
    <path d="M22.5 36 L24 44 L25.5 36 Z" fill="${c}" stroke="${INK}" stroke-width="0.9"/>
  `
}

/** ISR / recce — bizjet-like */
function silhouetteIsr(c) {
  return `
    <path d="M11 27 L24 23 L37 27 L37 29 L24 27 L24 36 L28 40 L24 38.5 L20 40 L24 36 L24 27 L11 29 Z"
      fill="${c}" stroke="${INK}" stroke-width="1.1" stroke-linejoin="round"/>
    <ellipse cx="24" cy="25" rx="2" ry="10" fill="${c}" stroke="${INK}" stroke-width="1.1"/>
    <path d="M24 14 L25.8 18 L22.2 18 Z" fill="${INK}"/>
    <rect x="22.5" y="19" width="3" height="2" fill="${INK}" opacity="0.5"/>
  `
}

/** UAV / MALE — long slender wing + boom */
function silhouetteUav(c) {
  return `
    <path d="M6 24 L24 22 L42 24 L42 26 L24 25 L24 34 L28 38 L20 38 L24 34 L24 25 L6 26 Z"
      fill="${c}" stroke="${INK}" stroke-width="1" stroke-linejoin="round"/>
    <ellipse cx="24" cy="24" rx="1.5" ry="8" fill="${c}" stroke="${INK}" stroke-width="1"/>
    <circle cx="24" cy="17" r="2.2" fill="${INK}" stroke="${c}" stroke-width="1"/>
    <path d="M20 36 L24 42 L28 36" fill="none" stroke="${c}" stroke-width="1.4" stroke-linejoin="round"/>
  `
}

/** UCAV — angular wing + V-tail */
function silhouetteUcav(c) {
  return `
    <path d="M8 25 L24 20 L40 25 L38 28 L24 25.5 L26 34 L32 38 L24 35 L16 38 L22 34 L24 25.5 L10 28 Z"
      fill="${c}" stroke="${INK}" stroke-width="1.1" stroke-linejoin="round"/>
    <ellipse cx="24" cy="24" rx="1.8" ry="7" fill="${c}" stroke="${INK}" stroke-width="1"/>
    <path d="M24 16 L26 20 L22 20 Z" fill="${INK}"/>
  `
}

/** Helicopter — rotor disc + boom */
function silhouetteHeli(c) {
  return `
    <circle cx="24" cy="22" r="14" fill="none" stroke="${c}" stroke-width="1.35" opacity="0.85" stroke-dasharray="2.5 2"/>
    <circle cx="24" cy="22" r="1.8" fill="${c}" stroke="${INK}" stroke-width="0.8"/>
    <ellipse cx="24" cy="26" rx="3.5" ry="7" fill="${c}" stroke="${INK}" stroke-width="1.1"/>
    <path d="M24 32 L24 42" stroke="${c}" stroke-width="1.6" stroke-linecap="round"/>
    <path d="M24 42 L28 40 M24 42 L28 44" stroke="${c}" stroke-width="1.3" stroke-linecap="round"/>
    <path d="M21 20 L27 20" stroke="${INK}" stroke-width="1.2" opacity="0.5"/>
  `
}

/** Unknown — generic light aircraft */
function silhouetteUnknown(c) {
  return `
    <path d="M10 26 L24 23 L38 26 L38 28.5 L24 27 L24 35 L29 39 L24 37 L19 39 L24 35 L24 27 L10 28.5 Z"
      fill="${c}" stroke="${INK}" stroke-width="1.1" stroke-linejoin="round"/>
    <ellipse cx="24" cy="25" rx="2" ry="9" fill="${c}" stroke="${INK}" stroke-width="1"/>
    <circle cx="24" cy="24" r="2.5" fill="none" stroke="${INK}" stroke-width="1" opacity="0.45"/>
  `
}

function bodyForPlatform(platform, color) {
  const p = (platform || 'unknown').toLowerCase()
  switch (p) {
    case 'fighter':
    case 'attack':
    case 'aircraft':
      return silhouetteFighter(color)
    case 'bomber':
      return silhouetteBomber(color)
    case 'transport':
    case 'tanker':
    case 'civil':
      return silhouetteAirliner(color)
    case 'aew':
      return silhouetteAew(color)
    case 'isr':
      return silhouetteIsr(color)
    case 'uav':
      return silhouetteUav(color)
    case 'ucav':
      return silhouetteUcav(color)
    case 'helicopter':
      return silhouetteHeli(color)
    default:
      return silhouetteUnknown(color)
  }
}

export function buildTrackIconSvg(platform, color, selected, heading) {
  const h = Number(heading) || 0
  return `<svg xmlns="http://www.w3.org/2000/svg" width="48" height="48" viewBox="0 0 48 48">
    ${selRing(color, selected)}
    <g transform="rotate(${h} 24 24)">
      ${bodyForPlatform(platform, color)}
    </g>
  </svg>`
}

export function buildOwnshipIconSvg(selected, heading) {
  const c = '#7af5dc'
  const glow = selected ? '#fbbf24' : c
  const h = Number(heading) || 0
  return `<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64" viewBox="0 0 64 64">
    ${selected ? `<circle cx="32" cy="32" r="30" fill="none" stroke="${glow}" stroke-width="1.8" opacity="0.85"/>` : ''}
    <g transform="translate(8 8) rotate(${h} 24 24)">
      ${silhouetteAew(c)}
    </g>
    <path d="M32 3 L35 6 L32 9 L29 6 Z" fill="${glow}" stroke="${INK}" stroke-width="0.8"/>
  </svg>`
}
