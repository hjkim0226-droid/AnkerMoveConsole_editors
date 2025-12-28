//! AEGP Hooks Implementation
//!
//! IdleHook, UpdateMenuHook, CommandHook, DeathHook 구현

use after_effects::*;
use log::{debug, trace};
use std::time::{Duration, Instant};

use crate::{with_state, Module, PLUGIN_STATE};

// =============================================================================
// Constants
// =============================================================================

/// 키 홀드 인식 시간 (ms)
const HOLD_DELAY_MS: u64 = 400;

/// 더블탭 인식 시간 (ms)
const DOUBLE_TAP_MS: u64 = 250;

/// UpdateMenuHook 유효 시간 (ms)
/// 이 시간 내에 호출되면 텍스트 편집 모드가 아님
const MENU_HOOK_THRESHOLD_MS: u64 = 50;

// =============================================================================
// Hook State (내부 상태)
// =============================================================================

/// UpdateMenuHook 마지막 호출 시간
static mut LAST_MENU_HOOK_TIME: Option<Instant> = None;

/// UpdateMenuHook이 최근에 호출되었는지 확인
fn is_menu_hook_recent() -> bool {
    unsafe {
        if let Some(last_time) = LAST_MENU_HOOK_TIME {
            last_time.elapsed() < Duration::from_millis(MENU_HOOK_THRESHOLD_MS)
        } else {
            false
        }
    }
}

// =============================================================================
// IdleHook
// =============================================================================

/// IdleHook - AE가 idle 상태일 때 주기적으로 호출됨
/// 키보드 모니터링 및 UI 업데이트 담당
pub extern "C" fn idle_hook(
    _plugin_refcon: aegp::GlobalRefcon,
    _max_sleep: &mut i32,
) -> aegp::Error {
    trace!("IdleHook called");

    // 텍스트 편집 중이면 키 입력 무시
    if !is_menu_hook_recent() {
        trace!("Skipping key check - possibly in text edit mode");
        return aegp::Error::None;
    }

    // 키보드 상태 확인 및 모듈 활성화
    if let Err(e) = check_keyboard_and_update() {
        debug!("Keyboard check error: {:?}", e);
    }

    // UI 업데이트 (egui)
    if let Err(e) = crate::ui::update_ui() {
        debug!("UI update error: {:?}", e);
    }

    aegp::Error::None
}

/// 키보드 상태 확인 및 모듈 업데이트
fn check_keyboard_and_update() -> Result<(), Error> {
    // 플랫폼별 키 상태 확인
    let key_state = get_key_state()?;

    with_state(|state| {
        // Y 키 홀드 → Grid 모듈
        if key_state.y_held && !state.key_state.y_key_held {
            state.key_state.y_key_held = true;
            state.key_state.last_d_press = Instant::now();
        } else if !key_state.y_held && state.key_state.y_key_held {
            state.key_state.y_key_held = false;

            // 홀드 시간 체크
            if state.key_state.last_d_press.elapsed() >= Duration::from_millis(HOLD_DELAY_MS) {
                state.active_module = Module::Grid;
                state.show_ui = true;
                debug!("Grid module activated");
            }
        }

        // D 키 → DMenu
        if key_state.d_held && !state.key_state.d_key_held {
            state.key_state.d_key_held = true;
            state.active_module = Module::DMenu;
            state.show_ui = true;
            debug!("DMenu activated");
        } else if !key_state.d_held && state.key_state.d_key_held {
            state.key_state.d_key_held = false;
        }

        // Shift+E → Control 모듈
        if key_state.shift_held && key_state.e_held {
            state.active_module = Module::Control;
            state.show_ui = true;
            debug!("Control module activated");
        }

        // ESC → 모든 UI 닫기
        if key_state.esc_held {
            state.show_ui = false;
            state.active_module = Module::None;
            debug!("UI closed by ESC");
        }
    });

    Ok(())
}

/// 플랫폼별 키 상태
struct PlatformKeyState {
    y_held: bool,
    d_held: bool,
    e_held: bool,
    shift_held: bool,
    esc_held: bool,
}

/// 플랫폼별 키 상태 가져오기
#[cfg(windows)]
fn get_key_state() -> Result<PlatformKeyState, Error> {
    use windows::Win32::UI::Input::KeyboardAndMouse::GetAsyncKeyState;

    // Virtual Key Codes
    const VK_Y: i32 = 0x59;
    const VK_D: i32 = 0x44;
    const VK_E: i32 = 0x45;
    const VK_SHIFT: i32 = 0x10;
    const VK_ESCAPE: i32 = 0x1B;

    unsafe {
        Ok(PlatformKeyState {
            y_held: GetAsyncKeyState(VK_Y) < 0,
            d_held: GetAsyncKeyState(VK_D) < 0,
            e_held: GetAsyncKeyState(VK_E) < 0,
            shift_held: GetAsyncKeyState(VK_SHIFT) < 0,
            esc_held: GetAsyncKeyState(VK_ESCAPE) < 0,
        })
    }
}

#[cfg(target_os = "macos")]
fn get_key_state() -> Result<PlatformKeyState, Error> {
    // macOS에서는 CGEventSource 사용
    // TODO: 구현 필요
    Ok(PlatformKeyState {
        y_held: false,
        d_held: false,
        e_held: false,
        shift_held: false,
        esc_held: false,
    })
}

#[cfg(not(any(windows, target_os = "macos")))]
fn get_key_state() -> Result<PlatformKeyState, Error> {
    Ok(PlatformKeyState {
        y_held: false,
        d_held: false,
        e_held: false,
        shift_held: false,
        esc_held: false,
    })
}

// =============================================================================
// UpdateMenuHook
// =============================================================================

/// UpdateMenuHook - 키보드 입력 시 호출됨
/// 텍스트 편집 모드 감지에 사용
pub extern "C" fn update_menu_hook(
    _plugin_refcon: aegp::GlobalRefcon,
    _menu_refcon: aegp::UpdateMenuRefcon,
    _active_window: aegp::WindowType,
) -> aegp::Error {
    trace!("UpdateMenuHook called");

    // 타임스탬프 업데이트
    unsafe {
        LAST_MENU_HOOK_TIME = Some(Instant::now());
    }

    aegp::Error::None
}

// =============================================================================
// CommandHook
// =============================================================================

/// CommandHook - 커맨드 실행 시 호출됨
pub extern "C" fn command_hook(
    _plugin_refcon: aegp::GlobalRefcon,
    _command: aegp::Command,
) -> aegp::Error {
    trace!("CommandHook called");
    aegp::Error::None
}

// =============================================================================
// DeathHook
// =============================================================================

/// DeathHook - 플러그인/AE 종료 시 호출됨
pub extern "C" fn death_hook(
    _plugin_refcon: aegp::GlobalRefcon,
) -> aegp::Error {
    debug!("DeathHook called - cleaning up");

    // 리소스 정리
    crate::ui::cleanup();

    aegp::Error::None
}
