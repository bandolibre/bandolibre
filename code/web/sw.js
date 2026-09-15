// Minimal service worker: makes the tool installable and lets the app
// shell load when offline. USB-MIDI itself still needs a real connection.
const CACHE_NAME = "bandolibre-midi-v1";
const SHELL_URL = "midi.html";
const SHELL_FILES = [
  SHELL_URL,
  "manifest.webmanifest",
  "icon-192.png",
  "icon-512.png",
  "icon-512-maskable.png",
  "apple-touch-icon.png",
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
      .then((keys) => Promise.all(keys.filter((key) => key !== CACHE_NAME).map((key) => caches.delete(key))))
      .then(() => self.clients.claim())
  );
});

self.addEventListener("fetch", (event) => {
  const { request } = event;
  if (request.method !== "GET" || new URL(request.url).origin !== self.location.origin) {
    return;
  }

  const isNavigation = request.mode === "navigate";
  event.respondWith(
    caches.match(isNavigation ? SHELL_URL : request).then((cached) => {
      const network = fetch(request)
        .then((response) => {
          if (response.ok) {
            const copy = response.clone();
            caches.open(CACHE_NAME).then((cache) => cache.put(isNavigation ? SHELL_URL : request, copy));
          }
          return response;
        })
        .catch(() => cached);
      return cached || network;
    })
  );
});
