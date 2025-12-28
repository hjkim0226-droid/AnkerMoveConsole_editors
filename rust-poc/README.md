# SnapPlugin Rust PoC

After Effects AEGP 플러그인의 Rust 구현 PoC (Proof of Concept)

## 목표

1. **AE에서 Rust 플러그인 로드** 확인
2. **IdleHook/UpdateMenuHook** 동작 확인
3. **egui 크로스플랫폼 UI** 검증
4. **ExtendScript 실행** 확인

## 구조

```
rust-poc/
├── Cargo.toml          # 의존성 설정
├── build.rs            # PiPL 리소스 생성
├── src/
│   ├── lib.rs          # 플러그인 엔트리포인트
│   ├── hooks.rs        # IdleHook, UpdateMenuHook 등
│   ├── ui.rs           # egui 크로스플랫폼 UI
│   └── preset.rs       # 프리셋 관리 (순수 로직, 테스트 가능)
└── README.md
```

## 의존성

- **after-effects**: AE SDK Rust 바인딩
- **egui**: 크로스플랫폼 즉시모드 GUI
- **winit**: 윈도우 생성
- **serde**: 프리셋 직렬화

## 빌드

```bash
# 개발 빌드
cargo build

# 릴리즈 빌드
cargo build --release

# 테스트 (AE 없이!)
cargo test
```

## 설치

### Windows
```bash
# 빌드 결과물을 MediaCore 폴더에 복사
cp target/release/snap_plugin_rust.dll \
   "C:/Program Files/Adobe/Common/Plug-ins/7.0/MediaCore/SnapPluginRust.aex"
```

### macOS
```bash
# 빌드 결과물을 MediaCore 폴더에 복사
cp target/release/libsnap_plugin_rust.dylib \
   "/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/SnapPluginRust.plugin/Contents/MacOS/SnapPluginRust"
```

## 테스트 가능한 것

```rust
// preset.rs - AE 없이 테스트 가능!
cargo test

// 예시 테스트
#[test]
fn test_text_preset_pipe_roundtrip() {
    let original = TextPreset { ... };
    let pipe_str = original.to_pipe_string();
    let restored = TextPreset::from_pipe_string(&pipe_str).unwrap();
    assert_eq!(original.name, restored.name);
}
```

## 다음 단계

1. [ ] 빌드 성공 확인
2. [ ] AE에서 로드 확인
3. [ ] IdleHook 호출 로그 확인
4. [ ] egui 윈도우 렌더링
5. [ ] ExtendScript 실행 테스트

## C++ 버전과의 차이점

| 항목 | C++ | Rust |
|------|-----|------|
| 메모리 관리 | 수동 | 자동 (소유권) |
| 빌드 시스템 | CMake | Cargo |
| 크로스플랫폼 UI | GDI+/Cocoa 분리 | egui 통합 |
| 테스트 | AE 필요 | AE 없이 가능 |
| 프리셋 형식 | 파이프 구분 | JSON (호환 유지) |
