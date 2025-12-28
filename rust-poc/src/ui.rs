//! Cross-platform UI using egui
//!
//! Windows와 macOS에서 동일한 코드로 동작하는 UI

use egui::{Context, Visuals};
use log::{debug, error, info};
use std::sync::Mutex;

use crate::{execute_script, with_state, Module};

// =============================================================================
// UI State
// =============================================================================

/// egui 컨텍스트 (전역)
static UI_CONTEXT: Mutex<Option<UiContext>> = Mutex::new(None);

struct UiContext {
    ctx: Context,
    // 윈도우 핸들 (플랫폼별)
    #[cfg(windows)]
    hwnd: Option<windows::Win32::Foundation::HWND>,
    #[cfg(target_os = "macos")]
    ns_window: Option<*mut objc::runtime::Object>,
}

// =============================================================================
// UI Initialization
// =============================================================================

/// UI 초기화
pub fn init() -> Result<(), crate::Error> {
    info!("Initializing egui UI...");

    let ctx = Context::default();

    // 다크 테마 설정 (AE 스타일)
    ctx.set_visuals(Visuals::dark());

    // 스타일 커스터마이징
    let mut style = (*ctx.style()).clone();
    style.spacing.item_spacing = egui::vec2(8.0, 4.0);
    style.spacing.button_padding = egui::vec2(8.0, 4.0);
    style.visuals.window_rounding = egui::Rounding::same(8.0);
    style.visuals.widgets.noninteractive.bg_fill = egui::Color32::from_rgb(28, 28, 32);
    style.visuals.widgets.inactive.bg_fill = egui::Color32::from_rgb(45, 45, 55);
    style.visuals.selection.bg_fill = egui::Color32::from_rgb(74, 158, 255);
    ctx.set_style(style);

    let mut ui_ctx = UI_CONTEXT.lock().unwrap();
    *ui_ctx = Some(UiContext {
        ctx,
        #[cfg(windows)]
        hwnd: None,
        #[cfg(target_os = "macos")]
        ns_window: None,
    });

    info!("egui UI initialized");
    Ok(())
}

/// UI 정리
pub fn cleanup() {
    debug!("Cleaning up UI...");
    let mut ui_ctx = UI_CONTEXT.lock().unwrap();
    *ui_ctx = None;
}

// =============================================================================
// UI Update (IdleHook에서 호출)
// =============================================================================

/// UI 업데이트 - IdleHook에서 호출
pub fn update_ui() -> Result<(), crate::Error> {
    // UI 표시 여부 확인
    let (show_ui, active_module) = with_state(|state| {
        (state.show_ui, state.active_module)
    }).unwrap_or((false, Module::None));

    if !show_ui {
        return Ok(());
    }

    // 모듈별 UI 렌더링
    match active_module {
        Module::Grid => render_grid_ui()?,
        Module::Text => render_text_ui()?,
        Module::Shape => render_shape_ui()?,
        Module::DMenu => render_dmenu_ui()?,
        Module::Control => render_control_ui()?,
        Module::Keyframe => render_keyframe_ui()?,
        Module::Align => render_align_ui()?,
        Module::Comp => render_comp_ui()?,
        Module::None => {}
    }

    Ok(())
}

// =============================================================================
// Module UIs
// =============================================================================

/// Grid 모듈 UI (Y키)
fn render_grid_ui() -> Result<(), crate::Error> {
    // TODO: egui로 앵커 포인트 그리드 구현
    debug!("Rendering Grid UI");
    Ok(())
}

/// Text 모듈 UI (D→T)
fn render_text_ui() -> Result<(), crate::Error> {
    // UI 컨텍스트 없으면 생성
    let ui_ctx = UI_CONTEXT.lock().unwrap();
    if ui_ctx.is_none() {
        drop(ui_ctx);
        init()?;
    }

    // egui로 Text 스타일 패널 렌더링
    // 실제 구현에서는 egui_glow + winit으로 렌더링
    debug!("Rendering Text UI");

    // 예시: egui 윈도우 정의
    // egui::Window::new("Text Style")
    //     .default_width(300.0)
    //     .show(&ctx, |ui| {
    //         ui.heading("Text Properties");
    //
    //         ui.horizontal(|ui| {
    //             ui.label("Font Size:");
    //             ui.add(egui::DragValue::new(&mut font_size).speed(0.5));
    //         });
    //
    //         if ui.button("Apply").clicked() {
    //             execute_script(&format!("applyTextFontSize({})", font_size))?;
    //         }
    //     });

    Ok(())
}

/// Shape 모듈 UI (D→S)
fn render_shape_ui() -> Result<(), crate::Error> {
    debug!("Rendering Shape UI");
    Ok(())
}

/// DMenu UI (D키)
fn render_dmenu_ui() -> Result<(), crate::Error> {
    debug!("Rendering DMenu UI");

    // DMenu는 다른 모듈로 가는 게이트웨이
    // A: Align, T: Text, S: Shape, K: Keyframe, C: Comp

    Ok(())
}

/// Control 모듈 UI (Shift+E)
fn render_control_ui() -> Result<(), crate::Error> {
    debug!("Rendering Control UI");
    Ok(())
}

/// Keyframe 모듈 UI (D→K)
fn render_keyframe_ui() -> Result<(), crate::Error> {
    debug!("Rendering Keyframe UI");
    Ok(())
}

/// Align 모듈 UI (D→A)
fn render_align_ui() -> Result<(), crate::Error> {
    debug!("Rendering Align UI");
    Ok(())
}

/// Comp 모듈 UI (D→C)
fn render_comp_ui() -> Result<(), crate::Error> {
    debug!("Rendering Comp UI");
    Ok(())
}

// =============================================================================
// Platform-specific Window Creation
// =============================================================================

#[cfg(windows)]
mod platform {
    use super::*;
    use windows::Win32::UI::WindowsAndMessaging::*;
    use windows::Win32::Foundation::*;

    /// Windows 네이티브 윈도우 생성
    pub fn create_window(width: i32, height: i32, title: &str) -> Result<HWND, crate::Error> {
        // TODO: CreateWindowExW 호출
        // egui-glow 렌더러 설정
        unimplemented!("Windows window creation")
    }
}

#[cfg(target_os = "macos")]
mod platform {
    use super::*;

    /// macOS 네이티브 윈도우 생성
    pub fn create_window(width: i32, height: i32, title: &str) -> Result<*mut objc::runtime::Object, crate::Error> {
        // TODO: NSWindow 생성
        // egui-glow 렌더러 설정
        unimplemented!("macOS window creation")
    }
}

// =============================================================================
// Tests
// =============================================================================

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_init_cleanup() {
        // UI 초기화 테스트
        assert!(init().is_ok());

        // UI 정리 테스트
        cleanup();

        let ui_ctx = UI_CONTEXT.lock().unwrap();
        assert!(ui_ctx.is_none());
    }
}
