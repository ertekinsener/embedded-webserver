import { ApiError, api } from '../api/client';
import { session } from '../api/session';
import { h } from '../ui/dom';
import { icon } from '../ui/icons';

// Full-page sign-in form. `notice` explains why the user is here again
// (e.g. an expired session).
export function loginView(onLoggedIn: () => void, notice = ''): HTMLElement {
  const username = h('input', {
    class: 'field pl-10',
    attrs: { id: 'username', name: 'username', autocomplete: 'username', required: true },
  });
  const password = h('input', {
    class: 'field pl-10',
    attrs: {
      id: 'password',
      name: 'password',
      type: 'password',
      autocomplete: 'current-password',
      required: true,
    },
  });
  const message = h('p', {
    class: 'hidden rounded-lg border border-red-500/30 bg-red-500/10 px-3 py-2 text-sm text-red-300',
    attrs: { role: 'alert' },
  });
  const submit = h(
    'button',
    { class: 'btn btn-primary w-full', attrs: { type: 'submit' } },
    'Sign in',
  );

  const showMessage = (text: string, error: boolean): void => {
    message.textContent = text;
    message.className = error
      ? 'rounded-lg border border-red-500/30 bg-red-500/10 px-3 py-2 text-sm text-red-300'
      : 'rounded-lg border border-sky-500/30 bg-sky-500/10 px-3 py-2 text-sm text-sky-300';
  };
  if (notice) {
    showMessage(notice, false);
  }

  const form = h(
    'form',
    {
      class: 'space-y-5',
      on: {
        submit: async (event) => {
          event.preventDefault();
          submit.disabled = true;
          submit.replaceChildren(icon('spinner', 'h-4 w-4 animate-spin'), 'Signing in...');
          try {
            const result = await api.login(username.value, password.value);
            session.set(String(result.token ?? ''));
            onLoggedIn();
          } catch (error) {
            const reason =
              error instanceof ApiError ? error.message : 'Sign-in failed, please try again';
            showMessage(reason, true);
            password.value = '';
            password.focus();
          } finally {
            submit.disabled = false;
            submit.replaceChildren('Sign in');
          }
        },
      },
    },
    message,
    fieldWithIcon('Username', 'username', 'user', username),
    fieldWithIcon('Password', 'password', 'lock', password),
    submit,
  );

  const view = h(
    'div',
    { class: 'flex min-h-full items-center justify-center p-4' },
    h(
      'main',
      { class: 'card w-full max-w-sm p-8' },
      h(
        'div',
        { class: 'mb-8 flex flex-col items-center text-center' },
        h(
          'div',
          {
            class:
              'mb-4 flex h-14 w-14 items-center justify-center rounded-2xl bg-primary/20 text-primary',
          },
          icon('chip', 'h-7 w-7'),
        ),
        h('h1', { class: 'text-xl font-bold text-white' }, 'UART Bridge'),
        h('p', { class: 'mt-1 text-sm text-slate-500' }, 'Sign in to configure the device'),
      ),
      form,
    ),
  );
  queueMicrotask(() => username.focus());
  return view;
}

function fieldWithIcon(
  label: string,
  id: string,
  iconName: 'user' | 'lock',
  input: HTMLInputElement,
): HTMLElement {
  return h(
    'div',
    {},
    h('label', { class: 'field-label', attrs: { for: id } }, label),
    h(
      'div',
      { class: 'relative' },
      h(
        'div',
        { class: 'pointer-events-none absolute inset-y-0 left-0 flex items-center pl-3 text-slate-500' },
        icon(iconName),
      ),
      input,
    ),
  );
}
