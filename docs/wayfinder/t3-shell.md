# T3 — Shell: menu item, navigation, lazy Loader, icon (task, AFK)

Map: docs/wayfinder/api-saved-requests-map.md
Label: wayfinder:task
Status: closed — resolved

## Resolution

Shell done, build green, app starts without QML errors.
- Sidebar: API item after Docker (signal apiRequested, apiHeader) — qml/Sidebar.qml
- main.qml: onApiRequested → dashboardView.openApi()
- Dashboard: openApi(), breadcrumb Dashboard/API, apiPageWrap + ApiPage — qml/Dashboard.qml
- New qml/ApiPage.qml (stub; T4/T5/T6 fill it in), registered in CMakeLists.txt QML_FILES + qml.qrc
- Decision: no Loader — a permanent instance like Docker/Databases (transitions/codebase pattern), data lazily via pageActive (T2+)
- Open question: icons assets/icons/api.png + api-dark.png missing — temporarily Theme.icon("terminal") + TODO in Sidebar. An asset is needed from the designer.
- Update: icon globe-code.png turned out pure black → created globe-code-dark.png (white), both in qml.qrc, Sidebar switched to Theme.icon("globe-code").
Blocked-by: — (frontier)

## Question

Add an API item after Docker (Sidebar + Dashboard activePage="api" + breadcrumb Dashboard/API + header like Docker), QML page via Loader (zero memory until first open), icon api(.png/-dark) in assets/icons + qml.qrc. Existing components only; no component — stop.

## Constraints

- AGENTS.md: qrc:// paths, killall trun before launch, Theme.icon().
