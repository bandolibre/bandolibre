// Minimal service worker: makes the tool installable and lets the app
// shell load when offline. USB-MIDI itself still needs a real connection.
// Stale-while-revalidate: an update shows on the second load.
const CACHE_PREFIX = "bandolibre-connect-";
const CACHE_NAME = CACHE_PREFIX + "v1";
const SHELL_URL = "index.html";
const SHELL_FILES = [
  SHELL_URL,
  "manifest.webmanifest",
  "icon-192.png",
  "icon-512.png",
  "icon-512-maskable.png",
  "apple-touch-icon.png",
  "favicon-16.png",
  "favicon-32.png",
];

self.addEventListener("install", (event) => {
  event.waitUntil(
    caches.open(CACHE_NAME).then((cache) => cache.addAll(SHELL_FILES)).then(() => self.skipWaiting())
  );
});

self.addEventListener("activate", (event) => {
  event.waitUntil(
    caches
      .keys()
      // Caches are per origin: leave the update tool's alone.
      .then((keys) => Promise.all(keys.filter((key) => key.startsWith(CACHE_PREFIX) && key !== CACHE_NAME)
        .map((key) => caches.delete(key))))
      .then(() => self.clients.claim())
  );
});

self.addEventListener("fetch", (event) => {
  const { request } = event;
  const url = new URL(request.url);
  if (request.method !== "GET" || url.origin !== self.location.origin) {
    return;
  }
  // Only this folder: the update tool next door has its own worker.
  const scope = new URL(self.registration.scope);
  if (!url.pathname.startsWith(scope.pathname)) {
    return;
  }

  // Every navigation in this scope ("connect/", "connect/index.html",
  // with or without a query) is the one page.
  const key = request.mode === "navigate" ? SHELL_URL : request;
  event.respondWith(
    caches.match(key, { ignoreSearch: request.mode === "navigate" }).then((cached) => {
      const network = fetch(request)
        .then((response) => {
          if (response.ok) {
            const copy = response.clone();
            caches.open(CACHE_NAME).then((cache) => cache.put(key, copy));
          }
          return response;
        })
        .catch(() => cached);
      return cached || network;
    })
  );
});
