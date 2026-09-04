/**
 * Tauri IPC composable
 * 
 * Wraps the Tauri v2 JS bridge (injected by the Tauri shell)
 * for invoke (commands) and listen (events).
 */

export function useTauri() {
  // Check if we're inside Tauri shell
  const isTauri = typeof window !== 'undefined' && '__TAURI__' in window

  /**
   * Invoke a Tauri command
   */
  async function invoke<T = any>(cmd: string, args: Record<string, unknown> = {}): Promise<T> {
    if (!isTauri) {
      console.warn('⚠️ Tauri not available (running in browser)')
      return {} as T
    }

    const core = (window as any).__TAURI__?.core
    if (!core?.invoke) {
      throw new Error('Tauri invoke not available')
    }

    return core.invoke(cmd, args)
  }

  /**
   * Listen to a Tauri event
   */
  async function listen<T = any>(event: string, callback: (payload: { event: string; payload: T }) => void): Promise<() => void> {
    if (!isTauri) {
      console.warn('⚠️ Tauri events not available (running in browser)')
      return () => {}
    }

    const eventModule = (window as any).__TAURI__?.event
    if (!eventModule?.listen) {
      throw new Error('Tauri event listener not available')
    }

    return eventModule.listen(event, callback)
  }

  return {
    invoke,
    listen,
    isTauri,
  }
}
