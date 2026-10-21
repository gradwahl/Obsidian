/** Bounded, shared work with leases: offscreen consumers cancel queued requests.
 * Live resources are never disposed while a component is displaying them. */
export class ResourceCache<T> {
  private entries = new Map<string, {
    promise: Promise<T>; resolve: (value: T) => void; reject: (error: unknown) => void;
    controller: AbortController; refs: number; ready: boolean; started: boolean;
    value?: T; weight: number;
  }>();
  private running = 0;
  constructor(private load: (key: string, signal: AbortSignal) => Promise<T>,
    private capacity: number, private concurrency = 2,
    private weight: (value: T) => number = () => 1,
    private dispose: (value: T) => void = () => {}) {}

  acquire(key: string) {
    let entry = this.entries.get(key);
    if (!entry) {
      let resolve!: (value: T) => void;
      let reject!: (error: unknown) => void;
      const promise = new Promise<T>((yes, no) => { resolve = yes; reject = no; });
      entry = { promise, resolve, reject, controller: new AbortController(),
        refs: 0, ready: false, started: false, weight: 0 };
      this.entries.set(key, entry);
    } else {
      this.entries.delete(key); this.entries.set(key, entry);
    }
    entry.refs++;
    this.pump();
    let released = false;
    return { promise: entry.promise, release: () => {
      if (released) return;
      released = true;
      entry.refs--;
      if (!entry.refs && !entry.ready) {
        entry.controller.abort();
        if (this.entries.get(key) === entry) this.entries.delete(key);
        entry.reject(new DOMException('Consumer left', 'AbortError'));
      }
      this.trim(); this.pump();
    } };
  }

  private trim() {
    let total = 0;
    for (const entry of this.entries.values()) total += entry.weight;
    for (const [key, entry] of this.entries) {
      if (total <= this.capacity) break;
      if (!entry.ready || entry.refs) continue;
      this.entries.delete(key); total -= entry.weight;
      this.dispose(entry.value!);
    }
  }

  private pump() {
    for (const [key, entry] of this.entries) {
      if (this.running >= this.concurrency) break;
      if (entry.started || entry.ready || !entry.refs) continue;
      entry.started = true; this.running++;
      void this.load(key, entry.controller.signal).then(value => {
        if (entry.controller.signal.aborted) { this.dispose(value); return; }
        entry.value = value; entry.weight = this.weight(value); entry.ready = true;
        entry.resolve(value);
      }, error => {
        if (this.entries.get(key) === entry) this.entries.delete(key);
        entry.reject(error);
      }).finally(() => { this.running--; this.trim(); this.pump(); });
    }
  }

  get stats() {
    let retained = 0;
    for (const entry of this.entries.values()) retained += entry.weight;
    return { entries: this.entries.size, running: this.running, retained };
  }
}
