// Makes the update tool installable and lets it load offline. The page and
// the firmware it offers (firmware/) are fetched from the network when online,
// so a new release or protocol change shows at once; the cached copies are the
// offline fallback. Icons and the manifest are stale-while-revalidate.
const CACHE_PREFIX = "bandolibre-update-";
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
      // Caches are per origin: leave the configuration tool's alone.
      .then((keys) => Promise.all(keys.filter((key) => key.startsWith(CACHE_PREFIX) && key !== CACHE_NAME)
        .map((key) => caches.delete(key))))
      .then(() => self.clients.claim())
  );
});

self.addEventListener("fetch", (event) => {
  const { request } = event;
  const url = new URL(request.url);
  if (request.method !== "GET" || url.origin !== self.location.origin) return;
  const scope = new URL(self.registration.scope);
  if (!url.pathname.startsWith(scope.pathname)) return;

  const key = request.mode === "navigate" ? SHELL_URL : url.pathname;
  const save = (response) => {
    if (response.ok) {
      const copy = response.clone();
      caches.open(CACHE_NAME).then((cache) => cache.put(key, copy));
    }
    return response;
  };

  if (request.mode === "navigate" || url.pathname.startsWith(scope.pathname + "firmware/")) {
    // Network first: never offer an old firmware while a new one is online,
    // nor an old page, which must speak the protocol of the firmware it
    // installs.
    event.respondWith(fetch(request).then(save).catch(() => caches.match(key)));
    return;
  }

  event.respondWith(
    caches.match(key).then((cached) => {
      const network = fetch(request).then(save).catch(() => cached);
      return cached || network;
    })
  );
});
