// Font Awesome Free solid icons (CC BY 4.0, https://fontawesome.com), as
// inline SVG: only the icons imported here are bundled, no icon font.

import { faArrowRightFromBracket } from '@fortawesome/free-solid-svg-icons/faArrowRightFromBracket';
import { faBars } from '@fortawesome/free-solid-svg-icons/faBars';
import { faCheck } from '@fortawesome/free-solid-svg-icons/faCheck';
import { faChevronRight } from '@fortawesome/free-solid-svg-icons/faChevronRight';
import { faCircleNotch } from '@fortawesome/free-solid-svg-icons/faCircleNotch';
import { faClock } from '@fortawesome/free-solid-svg-icons/faClock';
import { faGaugeHigh } from '@fortawesome/free-solid-svg-icons/faGaugeHigh';
import { faLock } from '@fortawesome/free-solid-svg-icons/faLock';
import { faMicrochip } from '@fortawesome/free-solid-svg-icons/faMicrochip';
import { faNetworkWired } from '@fortawesome/free-solid-svg-icons/faNetworkWired';
import { faPaperPlane } from '@fortawesome/free-solid-svg-icons/faPaperPlane';
import { faPlug } from '@fortawesome/free-solid-svg-icons/faPlug';
import { faRotateLeft } from '@fortawesome/free-solid-svg-icons/faRotateLeft';
import { faRotateRight } from '@fortawesome/free-solid-svg-icons/faRotateRight';
import { faServer } from '@fortawesome/free-solid-svg-icons/faServer';
import { faTriangleExclamation } from '@fortawesome/free-solid-svg-icons/faTriangleExclamation';
import { faUser } from '@fortawesome/free-solid-svg-icons/faUser';
import { faXmark } from '@fortawesome/free-solid-svg-icons/faXmark';

interface IconDefinition {
  icon: [number, number, string[], string, string | string[]];
}

export const icons = {
  logout: faArrowRightFromBracket,
  menu: faBars,
  check: faCheck,
  chevron: faChevronRight,
  spinner: faCircleNotch,
  clock: faClock,
  overview: faGaugeHigh,
  lock: faLock,
  chip: faMicrochip,
  network: faNetworkWired,
  send: faPaperPlane,
  port: faPlug,
  revert: faRotateLeft,
  refresh: faRotateRight,
  device: faServer,
  warning: faTriangleExclamation,
  user: faUser,
  close: faXmark,
} satisfies Record<string, IconDefinition>;

export type IconName = keyof typeof icons;

const kSvgNamespace = 'http://www.w3.org/2000/svg';

export function icon(name: IconName, cls = 'h-4 w-4'): SVGSVGElement {
  const [width, height, , , pathData] = (icons[name] as IconDefinition).icon;
  const svg = document.createElementNS(kSvgNamespace, 'svg');
  svg.setAttribute('viewBox', `0 0 ${width} ${height}`);
  svg.setAttribute('class', `${cls} shrink-0 fill-current`);
  svg.setAttribute('aria-hidden', 'true');
  for (const d of Array.isArray(pathData) ? pathData : [pathData]) {
    const path = document.createElementNS(kSvgNamespace, 'path');
    path.setAttribute('d', d);
    svg.appendChild(path);
  }
  return svg;
}
