//! Preset Management (프리셋 관리)
//!
//! Text/Shape 프리셋 저장, 로드, 적용
//! 순수 비즈니스 로직 - 외부 의존성 없이 테스트 가능

use serde::{Deserialize, Serialize};
use std::path::PathBuf;
use log::{debug, error, info};

// =============================================================================
// Data Structures
// =============================================================================

/// RGB 색상 (0.0 ~ 1.0)
#[derive(Debug, Clone, Copy, PartialEq, Serialize, Deserialize, Default)]
pub struct Color {
    pub r: f32,
    pub g: f32,
    pub b: f32,
}

impl Color {
    pub fn new(r: f32, g: f32, b: f32) -> Self {
        Self { r, g, b }
    }

    pub fn white() -> Self {
        Self::new(1.0, 1.0, 1.0)
    }

    pub fn black() -> Self {
        Self::new(0.0, 0.0, 0.0)
    }

    /// egui::Color32로 변환
    pub fn to_egui(&self) -> egui::Color32 {
        egui::Color32::from_rgb(
            (self.r * 255.0) as u8,
            (self.g * 255.0) as u8,
            (self.b * 255.0) as u8,
        )
    }
}

/// 텍스트 정렬
#[derive(Debug, Clone, Copy, PartialEq, Serialize, Deserialize, Default)]
pub enum TextJustify {
    #[default]
    Left = 0,
    Center = 1,
    Right = 2,
}

/// Text 프리셋
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TextPreset {
    pub name: String,
    pub font: String,
    pub font_size: f32,
    pub tracking: f32,
    pub leading: f32,
    pub stroke_width: f32,
    pub fill_color: Color,
    pub stroke_color: Color,
    pub apply_fill: bool,
    pub apply_stroke: bool,
    pub justify: TextJustify,
}

impl Default for TextPreset {
    fn default() -> Self {
        Self {
            name: "Default".to_string(),
            font: "Arial".to_string(),
            font_size: 72.0,
            tracking: 0.0,
            leading: 0.0,
            stroke_width: 0.0,
            fill_color: Color::white(),
            stroke_color: Color::black(),
            apply_fill: true,
            apply_stroke: false,
            justify: TextJustify::Left,
        }
    }
}

/// Shape 프리셋
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ShapePreset {
    pub name: String,
    pub fill_color: Color,
    pub stroke_color: Color,
    pub stroke_width: f32,
    pub opacity: f32,
    pub size_w: f32,
    pub size_h: f32,
    pub roundness: f32,
    pub has_fill: bool,
    pub has_stroke: bool,
}

impl Default for ShapePreset {
    fn default() -> Self {
        Self {
            name: "Default".to_string(),
            fill_color: Color::white(),
            stroke_color: Color::black(),
            stroke_width: 2.0,
            opacity: 100.0,
            size_w: 100.0,
            size_h: 100.0,
            roundness: 0.0,
            has_fill: true,
            has_stroke: false,
        }
    }
}

// =============================================================================
// Preset Manager
// =============================================================================

/// 프리셋 매니저
#[derive(Debug, Default)]
pub struct PresetManager {
    pub text_presets: Vec<TextPreset>,
    pub shape_presets: Vec<ShapePreset>,
    presets_dir: Option<PathBuf>,
}

impl PresetManager {
    pub fn new() -> Self {
        let mut manager = Self::default();
        manager.init_presets_dir();
        manager
    }

    /// 프리셋 디렉토리 초기화
    fn init_presets_dir(&mut self) {
        #[cfg(windows)]
        {
            if let Some(appdata) = std::env::var_os("APPDATA") {
                let mut path = PathBuf::from(appdata);
                path.push("SnapPlugin");
                std::fs::create_dir_all(&path).ok();
                self.presets_dir = Some(path);
            }
        }

        #[cfg(target_os = "macos")]
        {
            if let Some(home) = std::env::var_os("HOME") {
                let mut path = PathBuf::from(home);
                path.push("Library/Application Support/SnapPlugin");
                std::fs::create_dir_all(&path).ok();
                self.presets_dir = Some(path);
            }
        }
    }

    // =========================================================================
    // Text Presets
    // =========================================================================

    /// Text 프리셋 파일 경로
    fn text_presets_path(&self) -> Option<PathBuf> {
        self.presets_dir.as_ref().map(|dir| dir.join("text-presets.json"))
    }

    /// Text 프리셋 로드
    pub fn load_text_presets(&mut self) -> Result<(), std::io::Error> {
        if let Some(path) = self.text_presets_path() {
            if path.exists() {
                let content = std::fs::read_to_string(&path)?;
                self.text_presets = serde_json::from_str(&content)
                    .unwrap_or_default();
                info!("Loaded {} text presets", self.text_presets.len());
            }
        }
        Ok(())
    }

    /// Text 프리셋 저장
    pub fn save_text_presets(&self) -> Result<(), std::io::Error> {
        if let Some(path) = self.text_presets_path() {
            let content = serde_json::to_string_pretty(&self.text_presets)?;
            std::fs::write(&path, content)?;
            debug!("Saved {} text presets", self.text_presets.len());
        }
        Ok(())
    }

    /// Text 프리셋 추가
    pub fn add_text_preset(&mut self, preset: TextPreset) {
        self.text_presets.push(preset);
        self.save_text_presets().ok();
    }

    /// Text 프리셋 삭제
    pub fn remove_text_preset(&mut self, index: usize) -> Option<TextPreset> {
        if index < self.text_presets.len() {
            let removed = self.text_presets.remove(index);
            self.save_text_presets().ok();
            Some(removed)
        } else {
            None
        }
    }

    // =========================================================================
    // Shape Presets
    // =========================================================================

    /// Shape 프리셋 파일 경로
    fn shape_presets_path(&self) -> Option<PathBuf> {
        self.presets_dir.as_ref().map(|dir| dir.join("shape-presets.json"))
    }

    /// Shape 프리셋 로드
    pub fn load_shape_presets(&mut self) -> Result<(), std::io::Error> {
        if let Some(path) = self.shape_presets_path() {
            if path.exists() {
                let content = std::fs::read_to_string(&path)?;
                self.shape_presets = serde_json::from_str(&content)
                    .unwrap_or_default();
                info!("Loaded {} shape presets", self.shape_presets.len());
            }
        }
        Ok(())
    }

    /// Shape 프리셋 저장
    pub fn save_shape_presets(&self) -> Result<(), std::io::Error> {
        if let Some(path) = self.shape_presets_path() {
            let content = serde_json::to_string_pretty(&self.shape_presets)?;
            std::fs::write(&path, content)?;
            debug!("Saved {} shape presets", self.shape_presets.len());
        }
        Ok(())
    }

    /// Shape 프리셋 추가
    pub fn add_shape_preset(&mut self, preset: ShapePreset) {
        self.shape_presets.push(preset);
        self.save_shape_presets().ok();
    }

    /// Shape 프리셋 삭제
    pub fn remove_shape_preset(&mut self, index: usize) -> Option<ShapePreset> {
        if index < self.shape_presets.len() {
            let removed = self.shape_presets.remove(index);
            self.save_shape_presets().ok();
            Some(removed)
        } else {
            None
        }
    }
}

// =============================================================================
// Serialization (파이프 구분 형식 - 기존 C++ 호환용)
// =============================================================================

impl TextPreset {
    /// 파이프 구분 문자열로 직렬화 (C++ 호환)
    pub fn to_pipe_string(&self) -> String {
        format!(
            "{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}",
            self.name,
            self.font,
            self.font_size,
            self.tracking,
            self.leading,
            self.stroke_width,
            self.fill_color.r,
            self.fill_color.g,
            self.fill_color.b,
            self.stroke_color.r,
            self.stroke_color.g,
            self.stroke_color.b,
            if self.apply_fill { "1" } else { "0" },
            if self.apply_stroke { "1" } else { "0" },
            self.justify as i32,
        )
    }

    /// 파이프 구분 문자열에서 역직렬화
    pub fn from_pipe_string(s: &str) -> Option<Self> {
        let parts: Vec<&str> = s.split('|').collect();
        if parts.len() < 15 {
            return None;
        }

        Some(Self {
            name: parts[0].to_string(),
            font: parts[1].to_string(),
            font_size: parts[2].parse().ok()?,
            tracking: parts[3].parse().ok()?,
            leading: parts[4].parse().ok()?,
            stroke_width: parts[5].parse().ok()?,
            fill_color: Color::new(
                parts[6].parse().ok()?,
                parts[7].parse().ok()?,
                parts[8].parse().ok()?,
            ),
            stroke_color: Color::new(
                parts[9].parse().ok()?,
                parts[10].parse().ok()?,
                parts[11].parse().ok()?,
            ),
            apply_fill: parts[12] == "1",
            apply_stroke: parts[13] == "1",
            justify: match parts[14].parse::<i32>().ok()? {
                0 => TextJustify::Left,
                1 => TextJustify::Center,
                2 => TextJustify::Right,
                _ => TextJustify::Left,
            },
        })
    }
}

// =============================================================================
// Tests (순수 비즈니스 로직 - AE 없이 테스트 가능!)
// =============================================================================

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_color_default() {
        let color = Color::default();
        assert_eq!(color.r, 0.0);
        assert_eq!(color.g, 0.0);
        assert_eq!(color.b, 0.0);
    }

    #[test]
    fn test_color_white() {
        let color = Color::white();
        assert_eq!(color.r, 1.0);
        assert_eq!(color.g, 1.0);
        assert_eq!(color.b, 1.0);
    }

    #[test]
    fn test_text_preset_default() {
        let preset = TextPreset::default();
        assert_eq!(preset.name, "Default");
        assert_eq!(preset.font_size, 72.0);
        assert!(preset.apply_fill);
        assert!(!preset.apply_stroke);
    }

    #[test]
    fn test_text_preset_pipe_roundtrip() {
        let original = TextPreset {
            name: "MyStyle".to_string(),
            font: "Helvetica".to_string(),
            font_size: 48.0,
            tracking: 10.0,
            leading: 5.0,
            stroke_width: 2.0,
            fill_color: Color::new(1.0, 0.5, 0.0),
            stroke_color: Color::black(),
            apply_fill: true,
            apply_stroke: true,
            justify: TextJustify::Center,
        };

        let pipe_str = original.to_pipe_string();
        let restored = TextPreset::from_pipe_string(&pipe_str).unwrap();

        assert_eq!(original.name, restored.name);
        assert_eq!(original.font, restored.font);
        assert_eq!(original.font_size, restored.font_size);
        assert_eq!(original.tracking, restored.tracking);
        assert_eq!(original.fill_color.r, restored.fill_color.r);
        assert_eq!(original.apply_fill, restored.apply_fill);
        assert_eq!(original.justify, restored.justify);
    }

    #[test]
    fn test_text_preset_pipe_invalid() {
        let invalid = "not|enough|fields";
        assert!(TextPreset::from_pipe_string(invalid).is_none());
    }

    #[test]
    fn test_shape_preset_default() {
        let preset = ShapePreset::default();
        assert_eq!(preset.name, "Default");
        assert_eq!(preset.stroke_width, 2.0);
        assert!(preset.has_fill);
        assert!(!preset.has_stroke);
    }

    #[test]
    fn test_preset_manager_new() {
        let manager = PresetManager::new();
        assert!(manager.text_presets.is_empty());
        assert!(manager.shape_presets.is_empty());
    }

    #[test]
    fn test_preset_manager_add_remove() {
        let mut manager = PresetManager::default(); // 파일 저장 없이 테스트

        // 추가
        manager.text_presets.push(TextPreset::default());
        assert_eq!(manager.text_presets.len(), 1);

        // 삭제
        manager.text_presets.remove(0);
        assert!(manager.text_presets.is_empty());
    }

    #[test]
    fn test_json_serialization() {
        let preset = TextPreset::default();
        let json = serde_json::to_string(&preset).unwrap();
        let restored: TextPreset = serde_json::from_str(&json).unwrap();
        assert_eq!(preset.name, restored.name);
    }
}
