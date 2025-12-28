//! SnapPlugin Rust PoC
//!
//! After Effects AEGP plugin implemented in Rust
//! Uses after-effects crate for SDK bindings and egui for cross-platform UI

mod hooks;
mod ui;
mod preset;

use after_effects::*;
use std::sync::Mutex;
use log::{info, error, debug};

// =============================================================================
// Plugin State (전역 상태)
// =============================================================================

/// 플러그인 전역 상태
/// Mutex로 감싸서 thread-safe하게 접근
static PLUGIN_STATE: Mutex<Option<PluginState>> = Mutex::new(None);

pub struct PluginState {
    /// 플러그인 ID (AE에서 할당)
    pub plugin_id: aegp::PluginId,

    /// UI 표시 여부
    pub show_ui: bool,

    /// 현재 모듈 (Text, Shape 등)
    pub active_module: Module,

    /// 키보드 상태
    pub key_state: KeyState,

    /// 프리셋 매니저
    pub presets: preset::PresetManager,
}

#[derive(Debug, Clone, Copy, PartialEq, Default)]
pub enum Module {
    #[default]
    None,
    Grid,       // Y key
    Text,       // D → T
    Shape,      // D → S
    Keyframe,   // D → K
    Align,      // D → A
    Control,    // Shift+E
    Comp,       // D → C
    DMenu,      // D key
}

#[derive(Debug, Default)]
pub struct KeyState {
    pub d_key_held: bool,
    pub y_key_held: bool,
    pub shift_held: bool,
    pub last_d_press: std::time::Instant,
}

impl Default for PluginState {
    fn default() -> Self {
        Self {
            plugin_id: aegp::PluginId::default(),
            show_ui: false,
            active_module: Module::None,
            key_state: KeyState::default(),
            presets: preset::PresetManager::new(),
        }
    }
}

// =============================================================================
// Plugin Entry Point
// =============================================================================

/// AE 플러그인 엔트리포인트 매크로
/// after-effects crate가 제공하는 매크로로 보일러플레이트 생성
aegp::define_aegp_plugin!(
    "SnapPlugin Rust",           // 표시 이름
    "SNRS",                      // 4자리 매치 이름
    SnapPluginRust,              // 플러그인 구조체
);

pub struct SnapPluginRust;

impl AdobePluginGlobal for SnapPluginRust {
    fn can_load(_host_name: &str, _host_version: u32) -> bool {
        // After Effects에서만 로드
        true
    }

    fn startup(&self, plugin_id: aegp::PluginId, _pica_basic: aegp::PicaBasicSuite) -> Result<(), Error> {
        // 로깅 초기화
        env_logger::init();
        info!("SnapPlugin Rust starting up...");

        // 플러그인 상태 초기화
        {
            let mut state = PLUGIN_STATE.lock().unwrap();
            *state = Some(PluginState {
                plugin_id,
                ..Default::default()
            });
        }

        // Hooks 등록
        self.register_hooks(plugin_id)?;

        info!("SnapPlugin Rust initialized successfully!");
        Ok(())
    }

    fn shutdown(&self) -> Result<(), Error> {
        info!("SnapPlugin Rust shutting down...");

        // 상태 정리
        let mut state = PLUGIN_STATE.lock().unwrap();
        *state = None;

        Ok(())
    }
}

impl SnapPluginRust {
    /// AEGP Hooks 등록
    fn register_hooks(&self, plugin_id: aegp::PluginId) -> Result<(), Error> {
        // RegisterSuite 가져오기
        let register_suite = aegp::RegisterSuite::new()?;

        // IdleHook 등록 - 키보드 모니터링 및 UI 업데이트
        register_suite.register_idle_hook(plugin_id, hooks::idle_hook)?;
        debug!("IdleHook registered");

        // UpdateMenuHook 등록 - 텍스트 편집 모드 감지
        register_suite.register_update_menu_hook(plugin_id, hooks::update_menu_hook)?;
        debug!("UpdateMenuHook registered");

        // CommandHook 등록 - 커맨드 처리 (선택적)
        register_suite.register_command_hook(plugin_id, hooks::command_hook)?;
        debug!("CommandHook registered");

        // DeathHook 등록 - 종료 처리
        register_suite.register_death_hook(plugin_id, hooks::death_hook)?;
        debug!("DeathHook registered");

        Ok(())
    }
}

// =============================================================================
// Helper Functions
// =============================================================================

/// ExtendScript 실행 헬퍼
pub fn execute_script(script: &str) -> Result<String, Error> {
    let state = PLUGIN_STATE.lock().unwrap();
    if let Some(ref s) = *state {
        let utility = aegp::UtilitySuite::new()?;
        let (result, error) = utility.execute_script(s.plugin_id, script, false)?;
        if !error.is_empty() {
            error!("ExtendScript error: {}", error);
        }
        Ok(result)
    } else {
        Err(Error::Generic)
    }
}

/// 플러그인 상태 접근 헬퍼
pub fn with_state<F, R>(f: F) -> Option<R>
where
    F: FnOnce(&mut PluginState) -> R,
{
    let mut state = PLUGIN_STATE.lock().ok()?;
    state.as_mut().map(f)
}

// =============================================================================
// Tests
// =============================================================================

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_module_default() {
        assert_eq!(Module::default(), Module::None);
    }

    #[test]
    fn test_preset_manager() {
        let manager = preset::PresetManager::new();
        assert!(manager.text_presets.is_empty());
        assert!(manager.shape_presets.is_empty());
    }
}
