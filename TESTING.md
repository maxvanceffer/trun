# Тесты проекта Trun

## Содержание

1. [Rust (backend)](#rust-backend)
2. [Vitest (unit, frontend)](#vitest-unit-frontend)
3. [Playwright (e2e, frontend)](#playwright-e2e-frontend)

---

## Rust (backend)

**Фреймворк:** встроенный тестовый раннер Cargo + `#[tokio::test]` для async

### Где находятся

Все тесты inline в одном файле, внутри блока `#[cfg(test)] mod tests { ... }`:

```
src-tauri/src/lib.rs          ← строка 926+: #[cfg(test)] mod tests { ... }
```

В файле **16 тестов**:

| № | Название | Строка | Описание |
|---|----------|--------|----------|
| 1 | `test_projects_file_path_exists` | 945 | Путь к файлу проектов в config dir |
| 2 | `test_project_persistence_save_and_load` | 955 | Сохранение и загрузка проектов в JSON |
| 3 | `test_projects_persistence_survives_re_load` | 995 | Файл проектов выживает при перезагрузке |
| 4 | `test_clear_projects` | 1018 | Очистка файла проектов |
| 5 | `test_startup_decision_with_projects` | 1042 | Старт с проектами |
| 6 | `test_startup_decision_without_projects` | 1065 | Старт без проектов |
| 7 | `test_bfs_scan_no_stack_overflow` | 1074 | BFS сканер глубинных директорий (50 уровней) |
| 8 | `test_detect_run_command_package_json_dev` | 1176 | Автоопределение run-команды из package.json |
| 9 | `test_detect_run_command_package_json_no_scripts` | 1187 | Автоопределение при отсутствии scripts |
| 10 | `test_detect_run_command_cargo` | 1197 | Автоопределение для Cargo.toml |
| 11 | `test_detect_run_command_go` | 1205 | Автоопределение для go.mod |
| 12 | `test_detect_run_command_unknown_manifest` | 1213 | Ненужные манифесты возвращают None |
| 13 | `test_process_store_store_and_remove` | 1222 | ProcessStore: store + remove (tokio::test) |
| 14 | `test_tokio_process_in_store` | 1275 | Async процесс в ProcessStore (tokio::test) |
| 15 | `test_multi_manifest_folder` | 1325 | Много манифестов в одной папке |
| 16 | `test_personizely_structure_scan` | 1411 | Сканирование реальной структуры Personizely |

### Зависимости для тестов (`src-tauri/Cargo.toml`)

```toml
[dev-dependencies]
tempfile = "3"
tokio = { version = "1", features = ["full"] }
```

### Как запустить

```bash
cd /Users/maxxxtraxxx/projects/trun/src-tauri
cargo test
```

или из корня проекта:

```bash
cd /Users/maxxxtraxxx/projects/trun && cargo test --manifest-path src-tauri/Cargo.toml
```

**Результат:** 16 passed (0.02s)

> **Важно:** Некоторые тесты (persistence) используют статический mutex (`PERSISTENCE_MUTEX`) для сериализации — они НЕ должны запускаться параллельно. `cargo test` по умолчанию работает с одним потоком для `#[test]` (не `#[tokio::test]`), так что всё работает корректно.

---

## Vitest (unit, frontend)

**Фреймворк:** Vitest v4.1.11

### Где находятся

```
frontend/tests/unit/
├── custom-commands.test.ts    # 7 тестов: buildTreeItems, runCustomCommand, addCustomCommand
└── tree-selection.test.ts     # 10 тестов: buildTreeItems, onTreeSelect
```

**Итого:** 2 файла, 17 тестов

### Конфиг

```ts
// frontend/vitest.config.ts
import { defineConfig } from 'vitest/config'
export default defineConfig({
  test: {
    include: ['tests/unit/**/*.test.ts'],
  },
})
```

### Как запустить

```bash
npm run test:unit
```

Или напрямую:

```bash
cd /Users/maxxxtraxxx/projects/trun/frontend
npx vitest run tests/unit/
```

**Результат:** 2 test files, 17 tests, all passed (230ms)

### Нюансы

- Тесты не используют `import { useTauri }` — они тестируют чистые функции (buildTreeItems, runCustomCommand, onTreeSelect) без зависимостей от Tauri
- Типы дублированы inline в тестовых файлах (ProjectInfo, CommandInfo, TreeItem, CustomCommand)
- Нет setup файлов, нет mocking библиотеки — всё мокнут вручную через интерфейсы
- `vitest` установлен как dependency в `frontend/package.json`

---

## Playwright (e2e, frontend)

**Фреймворк:** Playwright v1.62.1, браузер Chromium

### Где находятся

```
frontend/tests/e2e/
├── startup.spec.ts                        # 2 теста: стартовое поведение
├── sidebar-tree.spec.ts                   # 8 тестов: дерево, кнопки "+", выбор
└── add-custom-command.spec.ts            # 6 тестов: диалог + отображение в сайдбаре
```

**Итого:** 3 файла, 16 тестов

### Конфиг

```ts
// frontend/playwright.config.ts
import { defineConfig, devices } from '@playwright/test'

export default defineConfig({
  testDir: './tests/e2e',
  timeout: 120_000,
  expect: { timeout: 10_000 },
  use: {
    baseURL: 'http://localhost:3000',
    headless: true,
    screenshot: 'only-on-failure',
    video: 'retain-on-failure',
    trace: 'retain-on-failure',
    actionTimeout: 5000,
    navigationTimeout: 30000
  },
  projects: [
    { name: 'chromium', use: { ...devices['Desktop Chrome'] } }
  ],
  retries: 1, // HMR instability causes occasional false failures
  webServer: {
    command: 'npx nuxi dev --port 3000',
    url: 'http://localhost:3000',
    reuseExistingServer: true,
    timeout: 120_000,
    stderr: 'pipe'
  }
})
```

### Как запустить

```bash
npm run test:e2e
```

Или напрямую:

```bash
cd /Users/maxxxtraxxx/projects/trun/frontend
npx playwright test
```

### Результаты

| Статус | Кол-во | Детали |
|--------|--------|--------|
| ✅ Passed | 13 | Все тесты, кроме "Sidebar Display" |
| ❌ Failed | 3 | Все из `add-custom-command.spec.ts` — секция "Sidebar Display" |

**Фailing тесты:**

1. `should show custom commands in sidebar after adding them` — кастомная команда не появляется в дереве после добавления
2. `should show custom commands as tree items with custom icons` — две команды не появляются в дереве
3. `should show custom commands under the correct project/folder` — команда не видна в дереве

**Причина:** Тесты корректно обнаруживают баг — кастомные команды сохраняются в localStorage, но не отображаются в sidebar tree. Это **баг в коде**, а не в тестах.

### Зависимости

```json
{
  "devDependencies": {
    "@playwright/test": "^1.62.1",
    "playwright": "^1.62.1"
  }
```

### Нюансы

- Playwright **сам запускает dev-сервер** через `webServer` в конфиге (`npx nuxi dev --port 3000`)
- `reuseExistingServer: true` — если сервер уже на порту 3000, не запускает новый
- `retries: 1` — тесты retry-ятся один раз из-за нестабильности HMR
- `baseURL: 'http://localhost:3000'` — все `page.goto('/')` относительно этого URL
- Тесты injectруют данные через `localStorage.setItem('trun:projects', ...)` до хеерации
- **Никакий перезагрузки страницы не нужно** — данные injectруются после `domcontentloaded`
- При падении: screenshot, video, trace — в `test-results/`

---

## Сводная таблица

| Тип | Фреймворк | Файлов | Тестов | Команда | Рабочая директория |
|-----|-----------|--------|--------|---------|-------------------|
| Rust | Cargo test | 1 (inline) | 16 | `cargo test` | `src-tauri/` |
| Unit | Vitest | 2 | 17 | `npm run test:unit` | `frontend/` |
| E2E | Playwright | 3 | 16 (3 known fail) | `npm run test:e2e` | `frontend/` |
| Все | — | — | 49 | `npm run test` | `frontend/` |
| **Итого** | | **6** | **49** | | |

---

## ⚠️ КРИТИЧЕСКИЕ ПРАВИЛА

### Ничего не менять

При работе с тестами **абсолютно запрещено**:
- ❌ Менять тестовый код для "фикса" падающих тестов
- ❌ Менять production code для "прохождения" тестов
- ❌ Изменять конфиги тестов
- ❌ Добавлять новые тесты
- ❌ Удалять тесты

Задача — понимать что есть, как запустить, и что падает. Не чинить.

### Если команда не работает

Если команда для запуска тестов не работает — **СТОП**. Не пытаться:
- ❌ Не менять конфиг
- ❌ Не устанавливать зависимости
- ❌ Не менять команду
- ❌ Не крутиться в loop

Написать: **"Я не могу запустить тесты, потому что: ..."** и объяснить что именно пошло не так.

### Playwright требует dev-сервер

Playwright запускает свой own dev-сервер через `webServer`. Если нужно запустить вручную:

```bash
# В frontend/
./node_modules/.bin/nuxt dev --port 3000
# В другом терминале
./node_modules/.bin/playwright test
```

### Vitest НЕ требует сервера

Vitest тесты — чистые функции, никакого dev-сервера.

### Rust НЕ требует ничего

`cargo test` работает автономно, никаких зависимостей.
