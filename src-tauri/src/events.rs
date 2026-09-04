use serde::{Deserialize, Serialize};

// ─── Events: Rust → Frontend ───────────────────────────────────────────

/// Emitted during directory scan (progress).
#[derive(Clone, Serialize, Deserialize)]
pub struct ScanProgress {
  /// Current file/dir being scanned.
  pub path: String,
  /// Number of projects found so far.
  pub found: usize,
}

/// Emitted when directory scan is complete.
#[derive(Clone, Serialize, Deserialize)]
pub struct ScanComplete {
  /// Total projects discovered.
  pub projects: Vec<ProjectInfo>,
  /// Duration of the scan in milliseconds.
  pub elapsed_ms: u64,
}

/// Information about a discovered project.
#[derive(Clone, Serialize, Deserialize)]
pub struct ProjectInfo {
  pub id: String,
  pub name: String,
  pub project_path: String,  // actual project directory (contains manifest)
  pub root_path: String,     // scanned root folder (parent of all projects)
  pub icon: String,        // file extension like "ts", "go", "rs"
  pub manifest: String,    // e.g. "package.json", "Cargo.toml"
  pub commands: Vec<CommandInfo>,  // detected run commands
}

/// Represents a detected run command for a project.
#[derive(Clone, Serialize, Deserialize)]
pub struct CommandInfo {
  pub category: String,  // e.g. "npm", "cargo", "go", "composer"
  pub label: String,   // e.g. "dev", "start", "run"
  pub command: String,  // binary (e.g. "npm", "cargo")
  pub args: Vec<String>,  // arguments (e.g. ["run", "dev"])
}

/// System stats (CPU % and memory usage).
#[derive(Clone, Serialize, Deserialize)]
pub struct SystemStats {
  pub cpu_percent: f64,
  pub ram_used_mb: u64,
  pub ram_total_mb: u64,
}

/// Log line from tail.
#[derive(Clone, Serialize, Deserialize)]
pub struct LogLine {
  pub file: String,
  pub line: String,
  pub timestamp: String,
}

/// Emitted when a log message is produced.
#[derive(Clone, Serialize, Deserialize)]
pub struct LogMessage {
  pub level: String,  // "debug", "info", "warn", "error"
  pub target: String,
  pub message: String,
}

/// Emitted when a command starts (run/stop).
#[derive(Clone, Serialize, Deserialize)]
pub struct CommandStatus {
  pub project_id: String,
  pub status: String, // "running", "stopped", "completed", "failed"
  pub exit_code: Option<i32>,
}

/// Information about a running process.
#[derive(Clone, Serialize, Deserialize)]
pub struct ProcessStatus {
  pub project_id: String,
  pub command: String,
  pub label: String,
  pub pid: u32,
  pub status: String, // "running", "stopped", "failed"
  pub memory_mb: f64,
  pub cpu_percent: f64,
}

// ─── Event names ───────────────────────────────────────────────────────

pub const EVENT_SCAN_PROGRESS: &str = "scan:progress";
pub const EVENT_SCAN_COMPLETE: &str = "scan:complete";
pub const EVENT_LOG: &str = "log";
pub const EVENT_COMMAND_STATUS: &str = "command:status";
pub const EVENT_PROCESS_STATUS: &str = "process:status";

// ─── Emit helpers ──────────────────────────────────────────────────────

use tauri::{Emitter, Manager};

/// Emit to all windows.
pub fn emit_all<R: tauri::Runtime>(app: &tauri::AppHandle<R>, event: &str, payload: impl Serialize + Clone) -> tauri::Result<()> {
  app.emit(event, payload)
}

/// Emit to a specific window.
pub fn _emit_window<R: tauri::Runtime>(app: &tauri::AppHandle<R>, window_label: &str, event: &str, payload: impl Serialize + Clone) -> tauri::Result<()> {
  if let Some(window) = app.get_webview_window(window_label) {
    window.emit(event, payload)
  } else {
    // Window not found - just warn and continue
    eprintln!("Warning: window '{}' not found, skipping emit", window_label);
    Ok(())
  }
}

/// Emit a progress event.
pub fn emit_scan_progress<R: tauri::Runtime>(app: &tauri::AppHandle<R>, progress: &ScanProgress) -> tauri::Result<()> {
  emit_all(app, EVENT_SCAN_PROGRESS, progress)
}

/// Emit a scan completion event.
pub fn emit_scan_complete<R: tauri::Runtime>(app: &tauri::AppHandle<R>, complete: &ScanComplete) -> tauri::Result<()> {
  emit_all(app, EVENT_SCAN_COMPLETE, complete)
}

/// Emit a log message to all windows.
pub fn _emit_log<R: tauri::Runtime>(app: &tauri::AppHandle<R>, message: &LogMessage) -> tauri::Result<()> {
  emit_all(app, EVENT_LOG, message)
}

/// Emit command status to all windows.
pub fn _emit_command_status<R: tauri::Runtime>(app: &tauri::AppHandle<R>, status: &CommandStatus) -> tauri::Result<()> {
  emit_all(app, EVENT_COMMAND_STATUS, status)
}

/// Emit system stats to all windows.
#[allow(dead_code)]
pub fn emit_system_stats<R: tauri::Runtime>(app: &tauri::AppHandle<R>, stats: &SystemStats) -> tauri::Result<()> {
  emit_all(app, "system:stats", stats)
}
pub fn _emit_debug<R: tauri::Runtime>(app: &tauri::AppHandle<R>, message: &str) -> tauri::Result<()> {
  emit_all(app, EVENT_LOG, LogMessage {
    level: "debug".into(),
    target: "trun".into(),
    message: message.to_string(),
  })
}
