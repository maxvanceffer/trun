# R1 — HTTP off the GUI thread: findings

Ticket: `docs/wayfinder/r1-networking.md`. Map: `docs/wayfinder/api-request-page-map.md`.
Facts only + `file:line` pointers. API facts verified against Qt docs (qt-docs
MCP, Qt 6.12).

## 1. Existing precedent in the codebase

`Updater` is the only HTTP today: a `QNetworkAccessManager` member on the GUI
thread, async, replies via `finished`/`readyRead`/`downloadProgress` handlers —
`src/updater.cpp:12-14`, `:44` (`m_nam(new QNetworkAccessManager(this))`),
`checkForUpdates()` `:90-97`, `onCheckFinished` `:102-112`, download `:158-203`.

That is acceptable for a rare updater check, but the API client is interactive:
the standing pref "C++ network off the GUI thread" applies, so the Send slice
must not touch QNAM on the GUI thread.

Contrast from the services research (research-t1-patterns.md §1): Database/Docker
do *blocking* work in a throwaway `QThread::create` + return via queued
`invokeMethod`; `CommandExecutor` wraps an already-async `QProcess`. HTTP is not
natively blocking, but it can be *made* blocking-in-worker with a local event
loop (below), which reuses the house pattern.

## 2. Threading rules (verified)

- `QNetworkAccessManager` and `QNetworkReply` docs state "All functions in this
  class are reentrant" — safe on *different* objects in different threads, but
  each instance must be used from the thread that created it.
- `QNetworkReply::abort()` is a public slot → safe to call cross-thread through
  `QMetaObject::invokeMethod(reply, "abort", Qt::QueuedConnection)`; it runs in
  the reply's thread.
- `QThread::create(...)` is the Qt 6 way to run one shot of work (already the
  codebase pattern). Inside it, a `QEventLoop` must pump events so QNAM can
  drive the network; `exec()` returns when `finished` fires (or the loop is
  quit on cancel).
- Canceled transfers surface as `QNetworkReply::OperationCanceledError` in
  `finished`.

## 3. Chosen design: blocking-in-worker, one send at a time

Per-request `QThread::create` + local `QNAM` + local `QEventLoop`, matching the
Database/Docker pattern. Not a persistent worker thread — over-engineering for
the design's guaranteed one-in-flight Send/Cancel cycle.

Worker flow (all inside the thread lambda):

1. Build `QNetworkRequest` from a snapshot handed in by the GUI thread (resolved
   URL, method, headers list, body bytes, transfer timeout). All QNAM/request/
   reply objects are created and live in this thread.
2. `QEventLoop loop; QObject::connect(reply, &QNetworkReply::finished, &loop,
   &QEventLoop::quit);` then `loop.exec()`.
3. Read the reply in-thread: `QNetworkReply::error()`, status via
   `QNetworkRequest::HttpStatusCodeAttribute` (+ `HttpReasonPhraseAttribute`),
   headers via `rawHeaderList()`/`rawHeader()`, body via `readAll()`, elapsed
   ms via `QElapsedTimer`.
4. Compose a `QVariantMap result` on the worker side and deliver it to the
   GUI-thread `ApiService` with a direct/queued signal — the QObject lives on
   the GUI thread, so the emission crosses threads queued and needs no locks
   (same as `detailReady`/`queryResult` in databaseservice).
5. `delete reply` and the thread via `thread->deleteLater()` (the
   `connect(thread, &QThread::finished, thread, &QObject::deleteLater)`
   house pattern).

ApiService surface (GUI thread):

- `Q_INVOKABLE bool send(QVariantMap spec)` — snapshots and starts the worker;
  `false` while one is in flight (the design's Send↔Cancel toggle).
- `Q_INVOKABLE void cancel()` — `invokeMethod(reply, "abort", QueuedConnection)`
  on the in-flight reply (pointer stored for the lifetime of the worker).
- `signal void sendFinished(QVariantMap result)` — `{ ok, canceled, status,
  reason, headers: [{name,value}], body, ms, error, errorCode }`; the GUI maps
  it to the Response panel and `appendHistory`.
- `property bool sending` — for the Send button label/border.

Error taxonomy: `NoError` + status → success (4xx/5xx are a *response*, not an
error — the design's pill shows the band); `OperationCanceledError` → canceled
(no history entry); `ConnectionRefusedError` → the design's "Could not connect"
screen; `TimeoutError` → timeout; `HostNotFoundError`/others → same error
screen with the message. The GUI keeps the design's `host` for the message.

## 4. Verified request-building facts (qt-docs)

- Timeout: `QNetworkRequest::setTransferTimeout(ms)`; called without an argument
  it uses `DefaultTransferTimeout` = **30000 ms** (default is 0 = disabled).
- Redirects: by default QNAM follows them; `RedirectPolicyAttribute` default is
  `NoLessSafeRedirectPolicy`; `setMaximumRedirectsAllowed(n)` caps the chain.
  Qt's legacy note: on 301/302 the followed request uses GET unless the
  original was HEAD.
- Bodies: `nam->get/post/put/delete(...)`; PATCH (no direct method) →
  `nam->sendCustomRequest(request, "PATCH", body)` (or the
  `CustomVerbAttribute`). `ContentTypeHeader` set from the editor; JSON body
  mode adds `Content-Type: application/json` (design hint) unless the user set
  their own.
- Reply headers/status come from `rawHeaderList`/`rawHeader` and the
  `HttpStatusCodeAttribute`/`HttpReasonPhraseAttribute` attributes.

## 5. Conclusion for ApiService

Add the Send slice to `src/apiservice.h/.cpp` (the class stays on the GUI thread
for Settings/Keychain/storage, exactly the Database/Docker split); the HTTP I/O
spawns the throwaway thread. One request in flight; Cancel aborts via queued
`abort()`; results arrive on `sendFinished`; resolved-URL work and
`appendHistory` live in the GUI-side slice (R2).

## References

- `src/updater.cpp:12-14,44,90-112,158-203` — existing async QNAM on GUI thread
- `src/databaseservice.cpp:21-46,104-131` — blocking-in-worker + queued return
- `src/databaseservice.h:47-58` — signal surface model
- Qt 6.12 docs: qnetworkrequest.html (setTransferTimeout, RedirectPolicy,
  attributes), qnetworkreply.html (abort, NetworkError), qthread.html
  (QThread::create), qeventloop.html (exec/quit)