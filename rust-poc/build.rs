//! Build script for SnapPlugin Rust PoC
//!
//! Generates PiPL resource for After Effects plugin registration

use pipl::*;

fn main() {
    // PiPL 리소스 생성 (AE 플러그인 등록에 필수)
    pipl::build_pipl! {
        // 플러그인 기본 정보
        kind: PIPLType::AEGP,
        name: "SnapPlugin Rust",
        category: "Utility",
        entry_point: "PluginMain",

        // 버전 정보
        version: (1, 0, 0),

        // 지원 AE 버전 (17.0 = 2020)
        ae_spec_version: (17, 0),

        // 플러그인 고유 식별자
        match_name: "com.snap.plugin.rust",
    }

    // Windows에서 리소스 컴파일 필요
    #[cfg(windows)]
    {
        println!("cargo:rerun-if-changed=build.rs");

        // Windows 리소스 파일이 있으면 컴파일
        if std::path::Path::new("resources/plugin.rc").exists() {
            embed_resource::compile("resources/plugin.rc", embed_resource::NONE);
        }
    }

    // macOS에서 번들 설정
    #[cfg(target_os = "macos")]
    {
        println!("cargo:rustc-link-arg=-Wl,-exported_symbols_list,exports.txt");
    }
}
