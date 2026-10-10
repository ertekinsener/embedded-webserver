// Tiny DOM builder: the UI has a handful of views, so no framework is
// bundled into the firmware.

export type Child = Node | string | number | null | undefined | false;

export interface Props {
  class?: string;
  attrs?: Record<string, string | boolean | undefined>;
  on?: { [K in keyof HTMLElementEventMap]?: (event: HTMLElementEventMap[K]) => void };
}

export function h<K extends keyof HTMLElementTagNameMap>(
  tag: K,
  props: Props = {},
  ...children: Child[]
): HTMLElementTagNameMap[K] {
  const element = document.createElement(tag);
  if (props.class) {
    element.className = props.class;
  }
  for (const [name, value] of Object.entries(props.attrs ?? {})) {
    if (value === true) {
      element.setAttribute(name, '');
    } else if (typeof value === 'string') {
      element.setAttribute(name, value);
    }
  }
  for (const [event, handler] of Object.entries(props.on ?? {})) {
    element.addEventListener(event, handler as EventListener);
  }
  append(element, ...children);
  return element;
}

export function append(parent: Node, ...children: Child[]): void {
  for (const child of children) {
    if (child === null || child === undefined || child === false) {
      continue;
    }
    parent.appendChild(child instanceof Node ? child : document.createTextNode(String(child)));
  }
}

export function replaceChildren(parent: Element, ...children: Child[]): void {
  parent.replaceChildren();
  append(parent, ...children);
}
