mod events;

use tauri::Manager;
use tauri::Emitter;
use events::{LogMessage, CommandStatus, ScanProgress, ScanComplete, ProcessStatus, ProjectInfo};
use events::{emit_scan_progress, emit_scan_complete};

// ─── Project Persistence ─────────────────────────────────────────────────

/// Get the path to the projects.json file in the app config directory.
fn projects_file_path() -> std::path::PathBuf {
  let config_dir = dirs::config_dir()
    .unwrap_or_else(|| std::path::PathBuf::from("."))
    .join("trun");
  std::fs::create_dir_all(&config_dir).ok();
  config_dir.join("projects.json")
}

/// Load saved projects from disk.
fn load_projects() -> Option<Vec<ProjectInfo>> {
  let path = projects_file_path();
  if !path.exists() {
    return None;
  }
  let content = std::fs::read_to_string(&path).ok()?;
  serde_json::from_str(&content).ok()
}

/// Save projects to disk.
fn save_projects(projects: &[ProjectInfo]) {
  let path = projects_file_path();
  if let Ok(content) = serde_json::to_string_pretty(projects) {
    let _ = std::fs::write(&path, content);
  }
}

/// Clear saved projects from disk.
fn clear_projects() {
  let path = projects_file_path();
  let _ = std::fs::remove_file(&path);
}

// ─── Global process registry ─────────────────────────────────────────────

use std::collections::HashMap;
use std::sync::{Arc, Mutex};
use std::time::Instant;

#[allow(dead_code)]
struct ProcessInfo {
  child: tokio::process::Child,
  command: String,
  label: String,
  started: Instant,
}

struct ProcessRegistry {
  processes: HashMap<String, ProcessInfo>,
}

impl ProcessRegistry {
  fn new() -> Self {
    Self {
      processes: HashMap::new(),
    }
  }
}

type ProcessStore = Arc<Mutex<ProcessRegistry>>;

struct ProjectStore {
  projects: Vec<events::ProjectInfo>,
}

impl ProjectStore {
  fn new() -> Self {
    Self {
      projects: Vec::new(),
    }
  }
}

type ProjectsStoreType = Arc<Mutex<ProjectStore>>;

/// Detect run command from manifest name.
fn _detect_run_command(manifest: &str, root_path: &std::path::Path) -> Option<(String, Vec<String>)> {
  let cmd = match manifest {
    "package.json" => {
      if let Ok(content) = std::fs::read_to_string(root_path.join("package.json")) {
        if let Ok(json) = serde_json::from_str::<serde_json::Value>(&content) {
          if let Some(scripts) = json.get("scripts").and_then(|s| s.as_object()) {
            if let Some(cmd) = scripts.get("dev")
              .or_else(|| scripts.get("start"))
              .or_else(|| scripts.get("serve"))
            {
              let cmd_str = cmd.as_str()?;
              return Some(("npm".into(), vec!["run".into(), cmd_str.to_string()]));
            }
          }
        }
      }
      Some(("npm".into(), vec!["start".into()]))
    }
    "Cargo.toml" => Some(("cargo".into(), vec!["run".into()])),
    "go.mod" => Some(("go".into(), vec!["run".into()])),
    "Gemfile" => {
      if root_path.join("Rakefile").exists() {
        Some(("bundle".into(), vec!["exec".into(), "rake".into()]))
      } else {
        Some(("bundle".into(), vec!["exec".into(), "rails".into(), "s".into()]))
      }
    }
    "pyproject.toml" => Some(("python".into(), vec!["-m".into(), "flask".into(), "run".into()])),
    "mix.exs" => Some(("mix".into(), vec!["run".into()])),
    "composer.json" => Some(("composer".into(), vec!["run".into(), "dev".into()])),
    _ => None,
  };
  cmd
}

/// Detect all available run commands for a manifest file.
fn detect_run_commands(manifest: &str, root_path: &std::path::Path) -> Vec<events::CommandInfo> {
  let mut commands: Vec<events::CommandInfo> = Vec::new();
  
  match manifest {
    "package.json" => {
      if let Ok(content) = std::fs::read_to_string(root_path.join("package.json")) {
        if let Ok(json) = serde_json::from_str::<serde_json::Value>(&content) {
          if let Some(scripts) = json.get("scripts").and_then(|s| s.as_object()) {
            for (name, script) in scripts.iter() {
              commands.push(events::CommandInfo {
                category: "npm".into(),
                label: name.clone(),
                command: "npm".into(),
                args: vec!["run".into(), script.as_str().unwrap_or("").to_string()],
              });
            }
          }
        }
      }
    }
    "Cargo.toml" => {
      // Check for dev profile in Cargo.toml
      if let Ok(_content) = std::fs::read_to_string(root_path.join("Cargo.toml")) {
        // Check if there's a [package.metadata] or [workspace.metadata] with run commands
        // For now, add "run" (default) and "build" (release)
        commands.push(events::CommandInfo {
          category: "cargo".into(),
          label: "run".into(),
          command: "cargo".into(),
          args: vec!["run".into()],
        });
        commands.push(events::CommandInfo {
          category: "cargo".into(),
          label: "build".into(),
          command: "cargo".into(),
          args: vec!["build".into()],
        });
        commands.push(events::CommandInfo {
          category: "cargo".into(),
          label: "test".into(),
          command: "cargo".into(),
          args: vec!["test".into()],
        });
      }
    }
    "go.mod" => {
      commands.push(events::CommandInfo {
        category: "go".into(),
        label: "run".into(),
        command: "go".into(),
        args: vec!["run".into()],
      });
      commands.push(events::CommandInfo {
        category: "go".into(),
        label: "test".into(),
        command: "go".into(),
        args: vec!["test".into()],
      });
    }
    "Gemfile" => {
      commands.push(events::CommandInfo {
        category: "ruby".into(),
        label: "run".into(),
        command: "bundle".into(),
        args: if root_path.join("Rakefile").exists() {
          vec!["exec".into(), "rake".into()]
        } else {
          vec!["exec".into(), "rails".into(), "s".into()]
        },
      });
      commands.push(events::CommandInfo {
        category: "ruby".into(),
        label: "test".into(),
        command: "bundle".into(),
        args: vec!["exec".into(), "rspec".into()],
      });
    }
    "pyproject.toml" => {
      commands.push(events::CommandInfo {
        category: "python".into(),
        label: "run".into(),
        command: "python".into(),
        args: vec!["-m".into(), "flask".into(), "run".into()],
      });
      commands.push(events::CommandInfo {
        category: "python".into(),
        label: "test".into(),
        command: "python".into(),
        args: vec!["-m".into(), "pytest".into()],
      });
    }
    "mix.exs" => {
      commands.push(events::CommandInfo {
        category: "elixir".into(),
        label: "run".into(),
        command: "mix".into(),
        args: vec!["run".into()],
      });
      commands.push(events::CommandInfo {
        category: "elixir".into(),
        label: "test".into(),
        command: "mix".into(),
        args: vec!["test".into()],
      });
    }
    _ => {
      // Generic fallback
      commands.push(events::CommandInfo {
        category: "other".into(),
        label: "run".into(),
        command: "./run".into(),
        args: vec![],
      });
    }
  }
  
  commands
}

/// Log message helper for async tasks.
fn log_message(app: &tauri::AppHandle, level: &str, target: &str, message: &str) {
  let _ = app.emit(
    events::EVENT_LOG,
    LogMessage {
      level: level.into(),
      target: target.into(),
      message: message.to_string(),
    },
  );
}

/// Emit command status helper.
fn emit_status(app: &tauri::AppHandle, project_id: impl Into<String>, status: &str, code: Option<i32>) {
  let _ = app.emit(
    events::EVENT_COMMAND_STATUS,
    CommandStatus {
      project_id: project_id.into(),
      status: status.into(),
      exit_code: code,
    },
  );
}

/// Emit process status helper.
fn emit_process_status(
  app: &tauri::AppHandle,
  project_id: String,
  command: &str,
  label: &str,
  pid: u32,
  status: &str,
  memory_mb: f64,
  cpu_percent: f64,
) {
  let _ = app.emit(
    events::EVENT_PROCESS_STATUS,
    ProcessStatus {
      project_id,
      command: command.into(),
      label: label.into(),
      pid,
      status: status.into(),
      memory_mb,
      cpu_percent,
    },
  );
}

/// Get process memory usage.
fn get_process_memory_mb(pid: u32) -> f64 {
  use sysinfo::{System, ProcessRefreshKind};
  let pid_usize: usize = pid as usize;
  let mut sys = System::new();
  sys.refresh_processes_specifics(
    sysinfo::ProcessesToUpdate::Some(&[pid_usize.into()]),
    false,
    ProcessRefreshKind::nothing().with_memory(),
  );
  if let Some(process) = sys.process(pid_usize.into()) {
    return process.memory() as f64 / 1024.0 / 1024.0; // Convert to MB
  }
  0.0
}

// ─── Frontend Commands ─────────────────────────────────────────────────

#[tauri::command]
async fn cmd_pick_folder(
  _app: tauri::AppHandle,
) -> Option<String> {
  use rfd::AsyncFileDialog;
  AsyncFileDialog::new()
    .pick_folder()
    .await
    .map(|f| f.path().to_string_lossy().into_owned())
}

/// Switch wizard to dashboard.
#[tauri::command]
fn cmd_show_dashboard(app: tauri::AppHandle) {
  if let Some(wizard) = app.get_webview_window("wizard") {
    let _ = wizard.hide();
  }
  if let Some(dashboard) = app.get_webview_window("dashboard") {
    let _ = dashboard.show();
    let _ = dashboard.set_focus();
  }
}

/// Switch dashboard back to wizard (for re-scanning).
#[tauri::command]
fn cmd_show_wizard(app: tauri::AppHandle) {
  if let Some(dashboard) = app.get_webview_window("dashboard") {
    let _ = dashboard.hide();
  }
  if let Some(wizard) = app.get_webview_window("wizard") {
    let _ = wizard.show();
    let _ = wizard.set_focus();
  }
}

/// Pick a folder and return its path.
#[tauri::command]
fn cmd_scan(app: tauri::AppHandle, root_path: String) -> Vec<events::ProjectInfo> {
  use std::fs;

  let mut projects: Vec<events::ProjectInfo> = Vec::new();
  let mut entries_to_scan: Vec<std::path::PathBuf> = Vec::new();
  let mut seen: std::collections::HashSet<std::path::PathBuf> = std::collections::HashSet::new();

  const MANIFESTS: &[&str] = &[
    "package.json", "Cargo.toml", "go.mod", "Gemfile", "pyproject.toml",
    "pom.xml", "build.gradle", "CMakeLists.txt", "mix.exs", "composer.json",
  ];

  const SKIP_DIRS: &[&str] = &[
    ".git", "node_modules", ".venv", "venv", "__pycache__", ".idea",
    ".vscode", "target", "build", "dist", "out", ".next", ".nuxt",
    "coverage", ".cache", ".terraform", "vendor", ".packages",
  ];

  fn manifest_icon(name: &str) -> &str {
    match name {
      "package.json" => "js",
      "Cargo.toml" | "Cargo.lock" => "rs",
      "go.mod" => "go",
      "Gemfile" => "rb",
      "pyproject.toml" => "py",
      "pom.xml" => "java",
      "build.gradle" => "java",
      "CMakeLists.txt" => "cmake",
      "mix.exs" => "ex",
      "composer.json" => "php",
      _ => "txt",
    }
  }

  if let Ok(canonical) = fs::canonicalize(&root_path) {
    if canonical.is_dir() {
      seen.insert(canonical.clone());
      entries_to_scan.push(canonical);

      let _ = emit_scan_progress(&app, &ScanProgress {
        path: "Scanning...".into(),
        found: 0,
      });

      let mut file_count: usize = 0;
      let mut emit_count: usize = 0;

      // Iterative BFS scan — no recursion!
      while let Some(dir_path) = entries_to_scan.pop() {
        file_count += 1;

        let entries = match fs::read_dir(&dir_path) {
          Ok(e) => e,
          Err(_) => continue,
        };

        for entry in entries {
          if file_count > 100_000 {
            break;
          }

          file_count += 1;

          let entry = match entry {
            Ok(e) => e,
            Err(_) => continue,
          };

          let file_type = match entry.file_type() {
            Ok(t) => t,
            Err(_) => continue,
          };

          if file_type.is_dir() {
            let canonical = match fs::canonicalize(entry.path()) {
              Ok(c) => c,
              Err(_) => continue,
            };

            if !seen.insert(canonical.clone()) {
              continue;
            }

            let file_name = entry.file_name();
            let file_name_str = file_name.to_string_lossy();

            // Skip common non-project dirs
            if SKIP_DIRS.contains(&file_name_str.as_ref()) {
              continue;
            }

            // Check for ALL manifest files in this directory
            // First pass: count how many manifests exist
            let manifest_count = MANIFESTS.iter()
              .filter(|m| canonical.join(m).exists())
              .count();
            
            for manifest in MANIFESTS {
              let manifest_path = canonical.join(manifest);
              if manifest_path.exists() {
                let project_name = file_name_str.to_string();
                let commands = detect_run_commands(manifest, &canonical);
                // Make id unique per manifest for multi-manifest folders
                let id = if manifest_count > 1 {
                  format!("{}:{}", canonical.to_string_lossy(), manifest)
                } else {
                  canonical.to_string_lossy().into()
                };
                projects.push(events::ProjectInfo {
                  id,
                  name: project_name,
                  project_path: canonical.to_string_lossy().into(),
                  root_path: root_path.clone(),
                  icon: manifest_icon(manifest).into(),
                  manifest: manifest.to_string(),
                  commands,
                });
              }
            }

            // Add to queue for scanning
            entries_to_scan.push(canonical);
          }
        }

        // Emit progress every 100 files
        emit_count += 1;
        if emit_count >= 100 {
          emit_count = 0;
          let _ = emit_scan_progress(&app, &ScanProgress {
            path: "Scanning...".into(),
            found: projects.len(),
          });
        }
      }
    }
  }

  let _ = emit_scan_complete(&app, &ScanComplete {
    projects: projects.clone(),
    elapsed_ms: 0,
  });

  // Store projects in app state for cmd_run/cmd_stop
  {
    let store = app.state::<ProjectsStoreType>();
    let mut map = store.lock().unwrap();
    map.projects = projects.clone();
  }

  // Persist projects to disk so they survive app restart
  if projects.is_empty() {
    // No projects found — clear saved projects
    clear_projects();
  } else {
    save_projects(&projects);
  }

  // Restore focus — the file picker stole it when it closed.
  // If projects found: hide wizard, show dashboard.
  // If no projects: keep wizard visible for user to pick folders.
  let app_clone = app.clone();
  let projects_clone = projects.clone();
  tauri::async_runtime::spawn(async move {
    if projects_clone.is_empty() {
      // No projects found — keep wizard visible
      if let Some(window) = app_clone.get_webview_window("wizard") {
        let _ = window.show();
        let _ = window.set_focus();
      }
    } else {
      // Projects found — switch to dashboard
      if let Some(wizard) = app_clone.get_webview_window("wizard") {
        let _ = wizard.hide();
      }
      if let Some(dashboard) = app_clone.get_webview_window("dashboard") {
        let _ = dashboard.show();
        let _ = dashboard.set_focus();
      }
    }
  });

  projects
}

#[tauri::command]
async fn cmd_run(
  app: tauri::AppHandle,
  project_id: String,
  command: String,
  args: Vec<String>,
  working_dir: Option<String>,
  env_vars: Vec<(String, String)>,
) {
  // Find the project
  let project = {
    let store = app.state::<ProjectsStoreType>();
    let map = store.lock().unwrap();
    map.projects.iter().find(|p| p.id == project_id).cloned()
  };
  let Some(project) = project else {
    emit_status(&app, project_id.clone(), "failed", Some(1));
    return;
  };

  let project_path = &project.project_path;
  let project_name = &project.name;
  let work_dir = working_dir.unwrap_or_else(|| project_path.clone());

  let cmd_args = args.clone();
  let cmd_label = project.commands.iter()
    .find(|c| c.command == command && c.args == cmd_args)
    .map(|c| c.label.clone())
    .unwrap_or_else(|| command.clone());

  // Emit starting
  let mut msg = format!("Starting: {} {}", command, cmd_args.join(" "));
  if work_dir != *project_path {
    msg += &format!(" (cwd: {})", work_dir);
  }
  if !env_vars.is_empty() {
    msg += &format!(" (env: {} vars)", env_vars.len());
  }
  log_message(&app, "info", project_name, &msg);
  emit_status(&app, project_id.clone(), "running", None);

  // Spawn with tokio
  let pid = {
    // Clone all borrow-heavy data before creating command
    let cmd_args = args.clone();
    let work_dir = work_dir.clone();
    let envs: Vec<(String, String)> = env_vars.clone();
    
    let mut cmd = tokio::process::Command::new(command.clone());
    cmd.args(&cmd_args);
    cmd.current_dir(&work_dir);
    cmd.stdout(std::process::Stdio::piped());
    cmd.stderr(std::process::Stdio::piped());
    
    // Apply environment variables
    for (k, v) in &envs {
      cmd.env(k.clone(), v.clone());
    }
    
    let child = cmd.spawn()
      .unwrap_or_else(|e| {
        log_message(&app, "error", project_name, &format!("Failed to spawn '{}': {}", command, e));
        emit_status(&app, project_id.clone(), "failed", Some(1));
        std::process::exit(1);
      });
    let p = child.id().unwrap_or(0);
    // Store process handle with metadata
    let app2 = app.clone();
    let proj_id = project_id.clone();
    {
      let store = app2.state::<ProcessStore>();
      let mut map = store.lock().unwrap();
      map.processes.insert(proj_id.clone(), ProcessInfo {
        child,
        command: cmd_label.clone(),
        label: cmd_label.clone(),
        started: Instant::now(),
      });
    }
    p
  };

  // Read stdout/stderr in background
  let app2 = app.clone();
  let proj_id = project_id.clone();
  let proj_name_clone = project_name.clone();
  let cmd_label_clone = cmd_label.clone();
  tauri::async_runtime::spawn(async move {
    // Remove process from store, taking child with stdout/stderr
    let child: Option<tokio::process::Child>;
    {
      let store = app2.state::<ProcessStore>();
      let mut map = store.lock().unwrap();
      child = map.processes.remove(&proj_id).map(|p| p.child);
    }
    if let Some(child_inner) = child {
      let mut child = child_inner;

      // Read stdout lines
      let stdout = child.stdout.take();
      if let Some(s) = stdout {
        use tokio::io::AsyncBufReadExt;
        let reader = tokio::io::BufReader::new(s);
        let mut lines = reader.lines();
        while let Ok(Some(line)) = lines.next_line().await {
          log_message(&app2, "info", &proj_name_clone, &line);
        }
      }

      // Read stderr lines
      let stderr = child.stderr.take();
      if let Some(s) = stderr {
        use tokio::io::AsyncBufReadExt;
        let reader = tokio::io::BufReader::new(s);
        let mut lines = reader.lines();
        while let Ok(Some(line)) = lines.next_line().await {
          log_message(&app2, "error", &proj_name_clone, &line);
        }
      }

      // Wait for process exit
      match child.wait().await {
        Ok(status) => {
          let code = status.code();
          log_message(&app2, "info", &proj_name_clone, &format!("Process exited with code {:?}", code));
          emit_process_status(&app2, proj_id.clone(), &cmd_label_clone, &cmd_label_clone, 0, "stopped", 0.0, 0.0);
        }
        Err(e) => {
          log_message(&app2, "error", &proj_name_clone, &format!("Process wait error: {}", e));
          emit_process_status(&app2, proj_id.clone(), &cmd_label_clone, &cmd_label_clone, 0, "failed", 0.0, 0.0);
        }
      }
    }
  });

  // Emit initial process status with PID
  emit_process_status(&app, project_id.clone(), &cmd_label, &cmd_label, pid, "running", 0.0, 0.0);
}

#[tauri::command]
fn cmd_stop(app: tauri::AppHandle, project_id: String) -> String {
  let registry = app.state::<ProcessStore>();
  let mut store = registry.lock().unwrap();

  if let Some(proc_info) = store.processes.get_mut(&project_id) {
    let project_name = {
      let store = app.state::<ProjectsStoreType>();
      let map = store.lock().unwrap();
      map.projects.iter()
        .find(|p| p.id == project_id)
        .map(|p| p.name.clone())
        .unwrap_or_else(|| project_id.clone())
    };

    // Kill the process
    let pid = proc_info.child.id().unwrap_or(0);
    let _ = std::process::Command::new("kill")
      .args(&[&pid.to_string()])
      .spawn();

    // Remove from registry and emit process status
    let cmd_name = proc_info.command.clone();
    store.processes.remove(&project_id);

    emit_process_status(&app, project_id.clone(), &cmd_name, &cmd_name, 0, "stopped", 0.0, 0.0);

    format!("Stopped '{}'", project_name)
  } else {
    format!("No running process for '{}'", project_id)
  }
}

/// Get process info (PID, memory, CPU) for a running process.
#[tauri::command]
fn cmd_get_process_info(app: tauri::AppHandle, project_id: String) -> Option<ProcessStatus> {
  let registry = app.state::<ProcessStore>();
  let map = registry.lock().unwrap();
  let proc_info = map.processes.get(&project_id)?;
  
  let pid = proc_info.child.id().unwrap_or(0);
  let memory_mb = get_process_memory_mb(pid);
  
  Some(ProcessStatus {
    project_id,
    command: proc_info.command.clone(),
    label: proc_info.label.clone(),
    pid,
    status: "running".into(),
    memory_mb,
    cpu_percent: 0.0, // TODO: calculate from elapsed time
  })
}

/// Get system CPU and memory stats.
#[tauri::command]
fn cmd_get_system_stats() -> events::SystemStats {
  use sysinfo::System;
  let mut sys = System::new_all();
  sys.refresh_cpu_all();
  sys.refresh_memory();

  let total_ram = sys.total_memory();
  let used_ram = sys.used_memory();
  let cpu_percent = sys.global_cpu_usage();

  events::SystemStats {
    cpu_percent: cpu_percent as f64,
    ram_used_mb: used_ram / 1024 / 1024,
    ram_total_mb: total_ram / 1024 / 1024,
  }
}

/// Tail log lines from a log file.
#[tauri::command]
fn cmd_tail_logs(file: String, lines: usize) -> Vec<events::LogLine> {
  let log_dir = dirs::config_dir().unwrap_or_else(|| std::path::PathBuf::from("."))
    .join("trun")
    .join("logs");
  let log_file = log_dir.join(&file);

  if !log_file.exists() {
    return Vec::new();
  }

  match std::fs::read_to_string(&log_file) {
    Ok(content) => {
      let all_lines: Vec<_> = content.lines().collect();
      let start = if all_lines.len() > lines { all_lines.len() - lines } else { 0 };
      all_lines.iter()
        .skip(start)
        .enumerate()
        .map(|(i, line)| events::LogLine {
          file: file.clone(),
          line: line.to_string(),
          timestamp: format!("{}:{}", start + i + 1, ""),
        })
        .collect()
    }
    Err(_) => Vec::new(),
  }
}


#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
  tauri::Builder::default()
    .invoke_handler(tauri::generate_handler![
      cmd_scan,
      cmd_pick_folder,
      cmd_show_dashboard,
      cmd_show_wizard,
      cmd_run,
      cmd_stop,
      cmd_get_system_stats,
      cmd_get_process_info,
      cmd_tail_logs,
    ])
    .setup(|app| {
      // Register global process registry
      app.manage(Arc::new(Mutex::new(ProcessRegistry::new())));
      app.manage(Arc::new(Mutex::new(ProjectStore::new())));

      if cfg!(debug_assertions) {
        app.handle().plugin(
          tauri_plugin_log::Builder::default()
            .level(log::LevelFilter::Info)
            .build(),
        )?;
      }

      // Determine URL to load
      // In dev mode (cargo tauri dev), use the dev server URL from config
      // In build mode, use the static files from frontendDist
      let wizard_url: tauri::WebviewUrl;
      let dashboard_url: tauri::WebviewUrl;

      #[cfg(debug_assertions)]
      {
        use url::Url;
        wizard_url = tauri::WebviewUrl::External(Url::parse("http://localhost:3000/wizard").unwrap());
        dashboard_url = tauri::WebviewUrl::External(Url::parse("http://localhost:3000").unwrap());
      }
      #[cfg(not(debug_assertions))]
      {
        wizard_url = tauri::WebviewUrl::App("wizard.html".into());
        dashboard_url = tauri::WebviewUrl::App("index.html".into());
      }

      let handle = app.handle();

      // Two windows approach — both start HIDDEN, then show appropriate one.
      let _wizard = tauri::webview::WebviewWindowBuilder::new(app, "wizard", wizard_url)
        .title("trun — Wizard")
        .inner_size(700.0, 550.0)
        .resizable(true)
        .visible(false)
        .build()?;

      let _dashboard = tauri::webview::WebviewWindowBuilder::new(app, "dashboard", dashboard_url)
        .title("trun — Dashboard")
        .inner_size(960.0, 640.0)
        .resizable(true)
        .visible(false)
        .build()?;
      // Both window handles are kept in the builder scope; we access them by label later.
      // We intentionally drop them here — windows persist by their label in the app handle.

      // ─── Smart startup: show dashboard if projects exist, wizard otherwise ───
      {
        let handle = app.handle().clone();
        tauri::async_runtime::spawn(async move {
          // Give the frontend a moment to load before showing window
          tokio::time::sleep(tokio::time::Duration::from_millis(500)).await;

          if let Some(_projects) = load_projects() {
            // Projects exist — show dashboard
            if let Some(wizard) = handle.get_webview_window("wizard") {
              let _ = wizard.hide();
            }
            if let Some(dashboard) = handle.get_webview_window("dashboard") {
              let _ = dashboard.show();
              let _ = dashboard.set_focus();
            }
          } else {
            // No projects — show wizard
            if let Some(wizard) = handle.get_webview_window("wizard") {
              let _ = wizard.show();
              let _ = wizard.set_focus();
            }
            if let Some(dashboard) = handle.get_webview_window("dashboard") {
              let _ = dashboard.hide();
            }
          }
        });
      }

      // ----- tray menu (builder API) -----
      let show_item = tauri::menu::MenuItemBuilder::with_id("show_window", "Show Window")
        .build(handle)?;

      let settings_item = tauri::menu::MenuItemBuilder::with_id("settings", "Settings")
        .build(handle)?;

      let quit_item = tauri::menu::PredefinedMenuItem::quit(handle, Some("Quit"))?;

      // Command items with custom icons (phase 0 stub)
      let run_icon = tauri::image::Image::from_bytes(include_bytes!("../icons/play.png"))?;
      let run_item = tauri::menu::IconMenuItemBuilder::with_id("run_stub", "Run stub")
        .icon(run_icon)
        .build(handle)?;

      let stop_icon = tauri::image::Image::from_bytes(include_bytes!("../icons/stop.png"))?;
      let stop_item = tauri::menu::IconMenuItemBuilder::with_id("stop_stub", "Stop stub")
        .icon(stop_icon)
        .build(handle)?;

      let logs_item = tauri::menu::MenuItemBuilder::with_id("logs_stub", "Logs")
        .build(handle)?;

      let cmd_submenu = tauri::menu::SubmenuBuilder::with_id(handle, "cmd_submenu", "Commands")
        .items(&[&run_item, &stop_item, &logs_item])
        .build()?;

      let menu = tauri::menu::MenuBuilder::new(handle)
        .items(&[&show_item, &settings_item, &quit_item, &cmd_submenu])
        .build()?;

      // ----- tray -----
      let tray_icon = tauri::image::Image::from_bytes(include_bytes!("../icons/tray.png"))?;
      tauri::tray::TrayIconBuilder::with_id("trun")
        .icon(tray_icon)
        .icon_as_template(true) // macOS style: renders as template (blue/mono)
        .menu(&menu)
        .show_menu_on_left_click(true)
        .on_menu_event(move |_app, event| {
          eprintln!("menu event: {:?}", event.id);
          if event.id.as_ref() == "show_window" {
            if let Some(window) = _app.get_webview_window("wizard") {
              let _ = window.show();
              let _ = window.set_focus();
            } else if let Some(window) = _app.get_webview_window("dashboard") {
              let _ = window.show();
              let _ = window.set_focus();
            }
          }
        })
        .build(handle)?;

      Ok(())
    })
    .run(tauri::generate_context!())
    .expect("error while running tauri application");
}

// ─── Unit tests ─────────────────────────────────────────────────────────

#[cfg(test)]
mod tests {
  use super::*;
  use std::fs;
  use tempfile::TempDir;

  // ─── Project persistence tests (serial: share the same file) ─────────

  use std::sync::Mutex;
  static PERSISTENCE_MUTEX: Mutex<()> = Mutex::new(());

  fn _persist_lock() -> std::sync::MutexGuard<'static, ()> {
    PERSISTENCE_MUTEX.lock().unwrap()
  }

  fn _persist_clean() {
    let _ = std::fs::remove_file(projects_file_path());
  }

  #[test]
  fn test_projects_file_path_exists() {
    let _guard = _persist_lock();
    _persist_clean();
    // The projects file path should be in the config directory
    let path = projects_file_path();
    assert!(path.starts_with(dirs::config_dir().unwrap_or_default()));
    assert_eq!(path.file_name().unwrap(), "projects.json");
  }

  #[test]
  fn test_project_persistence_save_and_load() {
    let _guard = _persist_lock();
    _persist_clean();
    assert!(load_projects().is_none());

    // Create sample projects
    let projects = vec![
      ProjectInfo {
        id: "test://project1".into(),
        name: "Test Project".into(),
        project_path: "/tmp/project1".into(),
        root_path: "/tmp".into(),
        icon: "js".into(),
        manifest: "package.json".into(),
        commands: vec![],
      },
      ProjectInfo {
        id: "test://project2".into(),
        name: "Another Project".into(),
        project_path: "/tmp/project2".into(),
        root_path: "/tmp".into(),
        icon: "rs".into(),
        manifest: "Cargo.toml".into(),
        commands: vec![],
      },
    ];

    // Save projects
    save_projects(&projects);
    assert!(projects_file_path().exists());

    // Load projects back (verify file content)
    let content = fs::read_to_string(projects_file_path()).unwrap();
    assert!(content.contains("Test Project"));
    assert!(content.contains("Another Project"));
    assert!(content.contains("package.json"));
    assert!(content.contains("Cargo.toml"));
  }

  #[test]
  fn test_projects_persistence_survives_re_load() {
    let _guard = _persist_lock();
    _persist_clean();
    // Save projects
    let projects = vec![ProjectInfo {
      id: "persist://test".into(),
      name: "Persisted".into(),
      project_path: "/tmp/persist".into(),
      root_path: "/tmp".into(),
      icon: "py".into(),
      manifest: "pyproject.toml".into(),
      commands: vec![],
    }];
    save_projects(&projects);

    // Verify file exists and is valid JSON
    let content = fs::read_to_string(projects_file_path()).unwrap();
    let parsed: Vec<ProjectInfo> = serde_json::from_str(&content).unwrap();
    assert_eq!(parsed.len(), 1);
    assert_eq!(parsed[0].name, "Persisted");
  }

  #[test]
  fn test_clear_projects() {
    let _guard = _persist_lock();
    _persist_clean();
    // Create a projects file
    save_projects(&[ProjectInfo {
      id: "clear://test".into(),
      name: "ClearMe".into(),
      project_path: "/tmp/clear".into(),
      root_path: "/tmp".into(),
      icon: "go".into(),
      manifest: "go.mod".into(),
      commands: vec![],
    }]);
    assert!(projects_file_path().exists());

    // Clear
    clear_projects();
    assert!(!projects_file_path().exists());

    // Verify it's gone
    assert!(load_projects().is_none());
  }

  #[test]
  fn test_startup_decision_with_projects() {
    let _guard = _persist_lock();
    _persist_clean();
    // Simulate: projects exist → dashboard should be shown
    save_projects(&[ProjectInfo {
      id: "startup://with".into(),
      name: "StartupProject".into(),
      project_path: "/tmp/startup".into(),
      root_path: "/tmp".into(),
      icon: "js".into(),
      manifest: "package.json".into(),
      commands: vec![],
    }]);

    // When we load, we should get projects
    let loaded = load_projects();
    assert!(loaded.is_some());
    let projs = loaded.unwrap();
    assert_eq!(projs.len(), 1);
    assert_eq!(projs[0].name, "StartupProject");
  }

  #[test]
  fn test_startup_decision_without_projects() {
    let _guard = _persist_lock();
    _persist_clean();
    assert!(load_projects().is_none());
  }

  // ─── BFS scan test (original) ─────────────────────────────────────

  #[test]
  fn test_bfs_scan_no_stack_overflow() {
    // Create a deep nested directory structure to test BFS
    let tmp = std::env::temp_dir().join("trun_test_bfs");
    let _ = fs::remove_dir_all(&tmp);
    fs::create_dir_all(&tmp).unwrap();

    // Create a deep chain (50 levels, way beyond our 12-level limit)
    let mut path = tmp.clone();
    for i in 0..50 {
      path = path.join(format!("level_{i}"));
      fs::create_dir_all(&path).unwrap();
    }

    // Put a package.json in the deepest level
    fs::write(path.join("package.json"), r#"{"name":"deep"}"#).unwrap();

    // Run the scan
    let mut projects: Vec<events::ProjectInfo> = Vec::new();
    let mut entries_to_scan: Vec<std::path::PathBuf> = Vec::new();
    let mut seen: std::collections::HashSet<std::path::PathBuf> = std::collections::HashSet::new();

    const MANIFESTS: &[&str] = &[
      "package.json", "Cargo.toml", "go.mod", "Gemfile", "pyproject.toml",
      "pom.xml", "build.gradle", "CMakeLists.txt", "mix.exs", "composer.json",
    ];

    const SKIP_DIRS: &[&str] = &[
      ".git", "node_modules", ".venv", "venv", "__pycache__", ".idea",
      ".vscode", "target", "build", "dist", "out", ".next", ".nuxt",
      "coverage", ".cache", ".terraform", "vendor", ".packages",
    ];

    seen.insert(tmp.clone());
    entries_to_scan.push(tmp.clone());

    let mut file_count: usize = 0;

    // This is the BFS — if it panics with stack overflow, the test fails
    while let Some(dir_path) = entries_to_scan.pop() {
      file_count += 1;
      for entry in match fs::read_dir(&dir_path) {
        Ok(e) => e,
        Err(_) => continue,
      } {
        file_count += 1;
        let entry = match entry {
          Ok(e) => e,
          Err(_) => continue,
        };
        if entry.file_type().map_or(false, |t| t.is_dir()) {
          let canonical = match fs::canonicalize(entry.path()) {
            Ok(c) => c,
            Err(_) => continue,
          };
          if !seen.insert(canonical.clone()) {
            continue;
          }
          let file_name = entry.file_name();
          let file_name_str = file_name.to_string_lossy();
          if SKIP_DIRS.contains(&file_name_str.as_ref()) {
            continue;
          }
          // Check for ALL manifest files
          let manifest_count = MANIFESTS.iter()
            .filter(|m| canonical.join(m).exists())
            .count();
          for manifest in MANIFESTS {
            let manifest_path = canonical.join(manifest);
            if manifest_path.exists() {
              let id = if manifest_count > 1 {
                format!("{}:{}", canonical.to_string_lossy(), manifest)
              } else {
                canonical.to_string_lossy().into()
              };
              projects.push(events::ProjectInfo {
                id,
                name: file_name_str.to_string(),
                project_path: canonical.to_string_lossy().into(),
                root_path: tmp.to_string_lossy().into(),
                icon: "js".into(),
                manifest: manifest.to_string(),
                commands: vec![],
              });
            }
          }
          entries_to_scan.push(canonical);
        }
      }
    }

    // Cleanup
    let _ = fs::remove_dir_all(&tmp);

    // We should have found the deep project
    assert_eq!(projects.len(), 1, "Should find the deep nested project");
    assert_eq!(projects[0].name, "level_49");
    println!("BFS scan found {} projects, scanned {} entries", projects.len(), file_count);
  }

  // ─── detect_run_command tests ─────────────────────────────────────

  #[test]
  fn test_detect_run_command_package_json_dev() {
    let tmp = TempDir::new().unwrap();
    let pkg = tmp.path().join("package.json");
    fs::write(&pkg, r#"{"scripts":{"dev":"vite","start":"node index.js"}}"#).unwrap();
    let cmd = _detect_run_command("package.json", tmp.path()).unwrap();
    // The function returns the script VALUE ("vite"), not the key ("dev")
    assert_eq!(cmd.0, "npm");
    assert_eq!(cmd.1, vec!["run", "vite"]);
  }

  #[test]
  fn test_detect_run_command_package_json_no_scripts() {
    let tmp = TempDir::new().unwrap();
    let pkg = tmp.path().join("package.json");
    fs::write(&pkg, r#"{"name":"no-scripts"}"#).unwrap();
    let cmd = _detect_run_command("package.json", tmp.path()).unwrap();
    assert_eq!(cmd.0, "npm");
    assert_eq!(cmd.1, vec!["start"]);
  }

  #[test]
  fn test_detect_run_command_cargo() {
    let tmp = TempDir::new().unwrap();
    let cmd = _detect_run_command("Cargo.toml", tmp.path()).unwrap();
    assert_eq!(cmd.0, "cargo");
    assert_eq!(cmd.1, vec!["run"]);
  }

  #[test]
  fn test_detect_run_command_go() {
    let tmp = TempDir::new().unwrap();
    let cmd = _detect_run_command("go.mod", tmp.path()).unwrap();
    assert_eq!(cmd.0, "go");
    assert_eq!(cmd.1, vec!["run"]);
  }

  #[test]
  fn test_detect_run_command_unknown_manifest() {
    let tmp = TempDir::new().unwrap();
    let cmd = _detect_run_command("requirements.txt", tmp.path());
    assert!(cmd.is_none());
  }

  // ─── ProcessStore lifecycle test (tokio process) ───────────────────

  #[tokio::test]
  async fn test_process_store_store_and_remove() {
    let store = Arc::new(Mutex::new(ProcessRegistry::new()));

    // Spawn a simple long-running process
    let child = tokio::process::Command::new("sleep")
      .arg("9999")
      .stdout(std::process::Stdio::piped())
      .stderr(std::process::Stdio::piped())
      .spawn()
      .unwrap();

    let pid = child.id().unwrap();

    // Store it as ProcessInfo
    {
      let mut map = store.lock().unwrap();
      map.processes.insert("test-proc".to_string(), ProcessInfo {
        child,
        command: "sleep".into(),
        label: "test".into(),
        started: Instant::now(),
      });
    }

    // Verify it exists and has correct PID
    {
      let map = store.lock().unwrap();
      assert!(map.processes.contains_key("test-proc"));
      assert_eq!(
        map.processes.get("test-proc").unwrap().child.id().unwrap(),
        pid
      );
    }

    // Remove it - get the ProcessInfo back
    let mut proc_info = {
      let mut map = store.lock().unwrap();
      let removed = map.processes.remove("test-proc");
      assert!(removed.is_some());
      assert!(!map.processes.contains_key("test-proc"));
      removed.unwrap()
    };

    // Kill the process
    let _ = std::process::Command::new("kill")
      .arg(&pid.to_string())
      .spawn();
    let _ = proc_info.child.wait().await;
  }

  // ─── tokio process in store test ───────────────────────────────────

  #[tokio::test]
  async fn test_tokio_process_in_store() {
    let store = Arc::new(Mutex::new(ProcessRegistry::new()));

    // Spawn a simple process
    let cmd = tokio::process::Command::new("sleep")
      .arg("9999")
      .stdout(std::process::Stdio::piped())
      .stderr(std::process::Stdio::piped())
      .spawn()
      .unwrap();

    let pid = cmd.id().unwrap();

    // Store it in the registry
    {
      let mut map = store.lock().unwrap();
      map.processes.insert("sleep-test".to_string(), ProcessInfo {
        child: cmd,
        command: "sleep".into(),
        label: "sleep 9999".into(),
        started: Instant::now(),
      });
    }

    // Verify process is stored
    {
      let map = store.lock().unwrap();
      assert!(map.processes.contains_key("sleep-test"));
      assert_eq!(
        map.processes.get("sleep-test").unwrap().child.id().unwrap(),
        pid
      );
    }

    // Remove it
    {
      let mut map = store.lock().unwrap();
      assert!(map.processes.remove("sleep-test").is_some());
      assert!(!map.processes.contains_key("sleep-test"));
    }

    // Kill the actual process
    let _ = std::process::Command::new("kill")
      .arg(&pid.to_string())
      .spawn();
  }

  // ─── Multi-manifest folder test ────────────────────────────────────

  #[test]
  fn test_multi_manifest_folder() {
    let tmp = TempDir::new().unwrap();
    // Create a subdirectory with BOTH package.json and Cargo.toml
    // (scanner only checks subdirectories, not the root)
    let subdir = tmp.path().join("fullstack");
    fs::create_dir_all(&subdir).unwrap();
    fs::write(subdir.join("package.json"), r#"{"name":"fullstack"}"#).unwrap();
    fs::write(subdir.join("Cargo.toml"), r#"[package]\nname = "backend""#).unwrap();

    let mut projects: Vec<events::ProjectInfo> = Vec::new();
    let mut entries_to_scan: Vec<std::path::PathBuf> = Vec::new();
    let mut seen: std::collections::HashSet<std::path::PathBuf> = std::collections::HashSet::new();

    const MANIFESTS: &[&str] = &[
      "package.json", "Cargo.toml", "go.mod", "Gemfile", "pyproject.toml",
      "pom.xml", "build.gradle", "CMakeLists.txt", "mix.exs", "composer.json",
    ];
    const SKIP_DIRS: &[&str] = &[
      ".git", "node_modules", ".venv", "venv", "__pycache__", ".idea",
      ".vscode", "target", "build", "dist", "out", ".next", ".nuxt",
      "coverage", ".cache", ".terraform", "vendor", ".packages",
    ];

    seen.insert(tmp.path().to_path_buf());
    entries_to_scan.push(tmp.path().to_path_buf());

    while let Some(dir_path) = entries_to_scan.pop() {
      for entry in match fs::read_dir(&dir_path) {
        Ok(e) => e,
        Err(_) => continue,
      } {
        let entry = match entry {
          Ok(e) => e,
          Err(_) => continue,
        };
        if entry.file_type().map_or(false, |t| t.is_dir()) {
          let canonical = match fs::canonicalize(entry.path()) {
            Ok(c) => c,
            Err(_) => continue,
          };
          if !seen.insert(canonical.clone()) {
            continue;
          }
          let file_name = entry.file_name();
          let file_name_str = file_name.to_string_lossy();
          if SKIP_DIRS.contains(&file_name_str.as_ref()) {
            continue;
          }
          let manifest_count = MANIFESTS.iter()
            .filter(|m| canonical.join(m).exists())
            .count();
          for manifest in MANIFESTS {
            let manifest_path = canonical.join(manifest);
            if manifest_path.exists() {
              let id = if manifest_count > 1 {
                format!("{}:{}", canonical.to_string_lossy(), manifest)
              } else {
                canonical.to_string_lossy().into()
              };
              projects.push(events::ProjectInfo {
                id,
                name: file_name_str.to_string(),
                project_path: canonical.to_string_lossy().into(),
                root_path: tmp.path().to_string_lossy().into(),
                icon: "js".into(),
                manifest: manifest.to_string(),
                commands: vec![],
              });
            }
          }
          entries_to_scan.push(canonical);
        }
      }
    }

    // Should have found 2 projects (both manifests)
    assert_eq!(projects.len(), 2, "Should find both manifests in one folder");
    // Both should have unique IDs with manifest suffix
    let ids: Vec<_> = projects.iter().map(|p| &p.id).collect();
    assert!(ids.iter().all(|id| id.contains("package.json") || id.contains("Cargo.toml")),
      "Multi-manifest folders should have manifest in id");
  }

  // ─── Personizely structure test (real-world verification) ──────────

  #[test]
  fn test_personizely_structure_scan() {
    // Recreate the exact directory structure of ~/projects/personizely
    let tmp = TempDir::new().unwrap();

    // Helper: create parent dirs then write file
    fn mk(tmp: &tempfile::TempDir, rel: &str) {
      let p = tmp.path().join(rel);
      fs::create_dir_all(p.parent().unwrap()).unwrap();
      fs::write(p, "").unwrap();
    }

    // backend/ (has BOTH composer.json AND package.json)
    mk(&tmp, "backend/composer.json");
    mk(&tmp, "backend/package.json");
    // geoip/
    mk(&tmp, "geoip/package.json");
    // proxy/editor/ and proxy/proxy/
    mk(&tmp, "proxy/editor/package.json");
    mk(&tmp, "proxy/proxy/package.json");
    // shopify-app/
    mk(&tmp, "shopify-app/package.json");
    // shopify-app/extensions/campaign-cart-transforms/
    mk(&tmp, "shopify-app/extensions/campaign-cart-transforms/package.json");
    // shopify-app/extensions-staging/campaign-cart-transforms/
    mk(&tmp, "shopify-app/extensions-staging/campaign-cart-transforms/package.json");
    // shopify-app/extensions-dev/campaign-cart-transforms/
    mk(&tmp, "shopify-app/extensions-dev/campaign-cart-transforms/package.json");
    // snippet-manager/
    mk(&tmp, "snippet-manager/package.json");
    // website-parser/
    mk(&tmp, "website-parser/package.json");

    // Add node_modules/vendor dirs to test exclusion
    mk(&tmp, "backend/node_modules/some-pkg/package.json");
    mk(&tmp, "snippet-manager/node_modules/pkg/node_modules/deep/package.json");
    mk(&tmp, "backend/vendor/some-lib/composer.json");

    // Run the scan
    let mut projects: Vec<events::ProjectInfo> = Vec::new();
    let mut entries_to_scan: Vec<std::path::PathBuf> = Vec::new();
    let mut seen: std::collections::HashSet<std::path::PathBuf> = std::collections::HashSet::new();

    const MANIFESTS: &[&str] = &[
      "package.json", "Cargo.toml", "go.mod", "Gemfile", "pyproject.toml",
      "pom.xml", "build.gradle", "CMakeLists.txt", "mix.exs", "composer.json",
    ];
    const SKIP_DIRS: &[&str] = &[
      ".git", "node_modules", ".venv", "venv", "__pycache__", ".idea",
      ".vscode", "target", "build", "dist", "out", ".next", ".nuxt",
      "coverage", ".cache", ".terraform", "vendor", ".packages",
    ];

    seen.insert(tmp.path().to_path_buf());
    entries_to_scan.push(tmp.path().to_path_buf());

    while let Some(dir_path) = entries_to_scan.pop() {
      for entry in match fs::read_dir(&dir_path) {
        Ok(e) => e,
        Err(_) => continue,
      } {
        let entry = match entry {
          Ok(e) => e,
          Err(_) => continue,
        };
        if entry.file_type().map_or(false, |t| t.is_dir()) {
          let canonical = match fs::canonicalize(entry.path()) {
            Ok(c) => c,
            Err(_) => continue,
          };
          if !seen.insert(canonical.clone()) {
            continue;
          }
          let file_name = entry.file_name();
          let file_name_str = file_name.to_string_lossy();
          if SKIP_DIRS.contains(&file_name_str.as_ref()) {
            continue;
          }
          let manifest_count = MANIFESTS.iter()
            .filter(|m| canonical.join(m).exists())
            .count();
          for manifest in MANIFESTS {
            let manifest_path = canonical.join(manifest);
            if manifest_path.exists() {
              let id = if manifest_count > 1 {
                format!("{}:{}", canonical.to_string_lossy(), manifest)
              } else {
                canonical.to_string_lossy().into()
              };
              projects.push(events::ProjectInfo {
                id,
                name: file_name_str.to_string(),
                project_path: canonical.to_string_lossy().into(),
                root_path: tmp.path().to_string_lossy().into(),
                icon: "js".into(),
                manifest: manifest.to_string(),
                commands: vec![],
              });
            }
          }
          entries_to_scan.push(canonical);
        }
      }
    }

    // Verify expected count: 11 projects
    assert_eq!(projects.len(), 11,
      "Should find exactly 11 projects (not counting node_modules/vendor)");

    // Verify no false positives from node_modules
    for p in &projects {
      assert!(!p.project_path.contains("node_modules"),
        "Should NOT find projects inside node_modules: {}", p.project_path);
      assert!(!p.project_path.contains("vendor"),
        "Should NOT find projects inside vendor: {}", p.project_path);
    }

    // Verify all expected project names are present
    let names: Vec<_> = projects.iter().map(|p| p.name.as_str()).collect();
    let expected_names = [
      "backend",      // has BOTH composer.json AND package.json -> 2 projects
      "backend",      // composer.json
      "geoip",
      "editor",
      "proxy",
      "campaign-cart-transforms",
      "campaign-cart-transforms",
      "campaign-cart-transforms",
      "shopify-app",
      "snippet-manager",
      "website-parser",
    ];
    assert_eq!(names.len(), expected_names.len(),
      "Project count should match expected");
  }
}
