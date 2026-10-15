import { ApiError, api } from '../api/client';
import { h } from '../ui/dom';
import { icon } from '../ui/icons';
import { showToast } from '../ui/toast';
import type { AppData } from './shell';

// Limits the device enforces (webserver/schema/auth.fbs), in UTF-8 bytes.
const kMinPasswordBytes = 8;
const kMaxPasswordBytes = 64;

// Password change and the saved settings.
export function accountView(data: AppData): HTMLElement {
  const saved = data.system.settingsSaved;
  return h(
    'div',
    { class: 'grid grid-cols-1 gap-6 lg:grid-cols-2' },
    passwordCard(saved),
    savedSettingsCard(saved),
  );
}

function passwordField(id: string, label: string, autocomplete: string): [HTMLElement, HTMLInputElement] {
  const input = h('input', {
    class: 'field',
    attrs: { id, name: id, type: 'password', autocomplete, required: true },
  });
  return [h('div', {}, h('label', { class: 'field-label', attrs: { for: id } }, label), input), input];
}

function passwordCard(saved: boolean): HTMLElement {
  const [currentField, current] = passwordField('current_password', 'Current password', 'current-password');
  const [nextField, next] = passwordField('new_password', 'New password', 'new-password');
  const [confirmField, confirm] = passwordField('confirm_password', 'Repeat new password', 'new-password');
  const submit = h(
    'button',
    { class: 'btn btn-primary', attrs: { type: 'submit' } },
    icon('lock'),
    'Change password',
  );

  const problem = (): string => {
    const bytes = new TextEncoder().encode(next.value).length;
    if (bytes < kMinPasswordBytes || bytes > kMaxPasswordBytes) {
      return `The new password must have ${kMinPasswordBytes} to ${kMaxPasswordBytes} characters`;
    }
    return next.value === confirm.value ? '' : 'The new passwords do not match';
  };

  const form = h(
    'form',
    {
      class: 'space-y-4',
      on: {
        submit: async (event) => {
          event.preventDefault();
          const reason = problem();
          if (reason) {
            showToast('error', 'Password not changed', reason);
            return;
          }
          submit.disabled = true;
          try {
            await api.changePassword(current.value, next.value);
            form.reset();
            showToast(
              'success',
              'Password changed',
              saved
                ? 'Saved on the device; other sessions were signed out.'
                : 'Other sessions were signed out. The device forgets it when it restarts.',
            );
          } catch (error) {
            showToast('error', 'Password not changed', error instanceof ApiError ? error.message : String(error));
          } finally {
            submit.disabled = false;
          }
        },
      },
    },
    currentField,
    nextField,
    confirmField,
    h('p', { class: 'field-hint' }, `${kMinPasswordBytes} to ${kMaxPasswordBytes} characters.`),
    h('div', { class: 'flex justify-end' }, submit),
  );
  return h('section', { class: 'card p-6' }, h('h2', { class: 'card-title mb-4' }, 'Password'), form);
}

function savedSettingsCard(saved: boolean): HTMLElement {
  const description = saved
    ? 'Port settings, bridges and a changed password are saved on the device and come back after a restart. ' +
      'Holding the USER button (B1) during a reset also deletes them.'
    : 'This device keeps settings in RAM only: after a restart, every port, bridge and the password are back ' +
      'to their defaults.';
  const reset = h(
    'button',
    { class: 'btn btn-secondary', attrs: { type: 'button' } },
    icon('revert'),
    'Delete saved settings',
  );
  reset.addEventListener('click', async () => {
    if (
      !window.confirm(
        'Delete the saved settings? The device keeps running with the current ones and starts with the defaults ' +
          '(and the build-time password) after the next restart.',
      )
    ) {
      return;
    }
    reset.disabled = true;
    try {
      await api.resetSettings();
      showToast('success', 'Saved settings deleted', 'Defaults apply after the next restart.');
    } catch (error) {
      showToast('error', 'Not deleted', error instanceof ApiError ? error.message : String(error));
    } finally {
      reset.disabled = false;
    }
  });
  return h(
    'section',
    { class: 'card space-y-4 p-6' },
    h('h2', { class: 'card-title' }, 'Saved settings'),
    h(
      'p',
      { class: 'flex gap-2 text-sm text-slate-400' },
      icon(saved ? 'check' : 'warning', saved ? 'mt-0.5 h-4 w-4 text-emerald-400' : 'mt-0.5 h-4 w-4 text-amber-400'),
      h('span', {}, description),
    ),
    saved && h('div', { class: 'flex justify-end' }, reset),
  );
}
