// The session token, kept in sessionStorage: it survives a reload but not
// closing the tab. Storage may be unavailable (private mode, blocked site
// data); the session then lasts until the page is left.

const kKey = 'webserver.session';
let memoryToken: string | null = null;

export const session = {
  token(): string | null {
    try {
      return sessionStorage.getItem(kKey) ?? memoryToken;
    } catch {
      return memoryToken;
    }
  },

  set(token: string): void {
    memoryToken = token;
    try {
      sessionStorage.setItem(kKey, token);
    } catch {
      // Memory only.
    }
  },

  clear(): void {
    memoryToken = null;
    try {
      sessionStorage.removeItem(kKey);
    } catch {
      // Memory only.
    }
  },
};
