import { h } from './dom';
import { icon } from './icons';

export type ToastKind = 'success' | 'error';

const kDurationMs = 4000;

export function showToast(kind: ToastKind, title: string, message = ''): void {
  const container = document.getElementById('toasts');
  if (!container) {
    return;
  }
  const success = kind === 'success';
  const toast = h(
    'div',
    {
      class:
        'flex w-80 max-w-[calc(100vw-2rem)] items-start gap-3 rounded-lg border bg-slate-900 px-4 py-3 ' +
        'text-white shadow-2xl transition duration-300 ' +
        (success ? 'border-emerald-500/30' : 'border-red-500/30'),
      attrs: { role: success ? 'status' : 'alert' },
    },
    h(
      'div',
      {
        class:
          'flex h-8 w-8 shrink-0 items-center justify-center rounded-full ' +
          (success ? 'bg-emerald-500/20 text-emerald-400' : 'bg-red-500/20 text-red-400'),
      },
      icon(success ? 'check' : 'warning'),
    ),
    h(
      'div',
      { class: 'min-w-0' },
      h('p', { class: 'text-sm font-bold' }, title),
      message && h('p', { class: 'mt-0.5 break-words text-xs text-slate-400' }, message),
    ),
  );
  container.appendChild(toast);
  setTimeout(() => {
    toast.classList.add('opacity-0', '-translate-y-2');
    setTimeout(() => toast.remove(), 300);
  }, kDurationMs);
}
