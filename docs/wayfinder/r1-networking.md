# R1 — HTTP off the GUI thread (research, AFK)

Map: docs/wayfinder/api-request-page-map.md
Label: wayfinder:research
Status: closed — resolved, findings in docs/wayfinder/research-r1-networking.md
Conclusion: Send = per-request `QThread::create` with a local `QNAM` + `QEventLoop`
(blocking-in-worker, the Database/Docker pattern); ApiService stays on the GUI
thread; cancel via queued `abort()`; results on `sendFinished`.
Blocked-by: — (frontier)

## Question

Which pattern to give ApiService for Send: (a) a dedicated worker thread with its
own QNetworkAccessManager and event loop (async replies, requests and replies
cross threads via queued connections), or (b) a `QThread::create` per request —
a QNAM created in the thread, a local `QEventLoop` running until the reply
finishes, the result returned to the GUI thread via `invokeMethod` (the
Database/Docker blocking pattern)? Also: where the worker lives, the signal
surface (reply/cancel/error), timeout, redirect and TLS defaults, the error
taxonomy (connection refused vs timeout vs HTTP status), and how One Send-at-a-
time (Cancel aborts the current one) maps onto the worker.

## Context pointers

- src/updater.*, src/mcpserver.* — any existing HTTP in the app (grep QNetwork*)
- src/databaseservice.cpp:21-46, src/dockerservice.cpp:278-308 — the blocking
  QThread::create pattern (research-t1-patterns.md §1)
- src/commandexecutor.h — the async-signals contrast (QProcess)
- src/apiservice.* — where the slice lands
- CMakeLists.txt — QtNetwork linkage

## Answer (resolution comment — filled by the executor, not here)

Findings → docs/wayfinder/research-r1-networking.md + conclusion line.