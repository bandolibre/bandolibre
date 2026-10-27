// The configuration tool moved to ../connect/, with its own service worker.
// Browsers that installed the old one fetch this file on their next visit: it
// drops the old offline copy and unregisters itself, so midi.html here (now a
// redirect) loads from the network.
self.addEventListener("install", () => self.skipWaiting());

self.addEventListener("activate", (event) => {
  event.waitUntil(
    caches
      .keys()
      .then((keys) => Promise.all(keys.filter((key) => key.startsWith("bandolibre-midi-"))
        .map((key) => caches.delete(key))))
      .then(() => self.registration.unregister())
      .then(() => self.clients.matchAll({ type: "window" }))
      .then((clients) => clients.forEach((client) => client.navigate("../connect/")))
  );
});
