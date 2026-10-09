# T1 — Threading, lazy-load and storage patterns (research, AFK)

Map: docs/wayfinder/api-saved-requests-map.md
Label: wayfinder:research
Status: closed — resolved, findings in docs/wayfinder/research-t1-patterns.md (branch research/t1-patterns)
Blocked-by: — (frontier)

## Question

Which existing patterns to reuse for ApiService: a separate thread for network work (as DatabaseService/DockerService do work off the GUI), lazy service creation on the first openApi + Loader for QML, Settings JSON for structures + Keychain for secrets? Where to register in main.cpp, how to extend Dashboard.openDocker/openDatabases to openApi?

## Context pointers

- qml/Dashboard.qml: openDocker/openDatabases, activePage
- qml/main.qml: Sidebar wiring onDockerRequested
- qml/Sidebar.qml: dockerHeader block (copy for API)
- src/settings.h/.cpp, src/mackeychain.h/.mm, src/databaseservice.*
- qml.qrc, CMakeLists.txt

## Answer (resolution comment — filled by the executor, not here)

Findings → docs/wayfinder/research-t1-patterns.md on the throwaway branch research/t1-patterns + context pointer from the ticket.
