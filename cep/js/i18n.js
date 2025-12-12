/**
 * i18n.js - Internationalization module
 * Supports English and Korean
 */

const i18n = {
    currentLang: 'en',

    strings: {
        en: {
            gridPreview: 'Grid Preview',
            gridSize: 'Grid Size',
            gridScale: 'Grid Scale',
            mode: 'Mode',
            selection: 'Selection',
            composition: 'Composition',
            maskOn: 'Mask ON',
            maskOff: 'Mask OFF',
            customAnchors: 'Custom Anchors',
            transparency: 'Transparency',
            gridBg: 'Grid BG:',
            cellBg: 'Cell BG:',
            shortcuts: 'Shortcuts',
            holdGrid: 'Hold Grid:',
            toggleClick: 'Toggle Click:',
            language: 'Language',
            helpText: 'Click or drag to set position. Shift+drag: vertical only. Ctrl+drag: snap to grid.',
            flipH: 'Flip Horizontal',
            flipV: 'Flip Vertical'
        },
        ko: {
            gridPreview: '그리드 미리보기',
            gridSize: '그리드 크기',
            gridScale: '그리드 스케일',
            mode: '모드',
            selection: '셀렉션',
            composition: '컴포지션',
            maskOn: '마스크 적용',
            maskOff: '마스크 미적용',
            customAnchors: '커스텀 앵커',
            transparency: '투명도',
            gridBg: '그리드 배경:',
            cellBg: '셀 배경:',
            shortcuts: '단축키',
            holdGrid: '그리드 홀드:',
            toggleClick: '토글 클릭:',
            language: '언어',
            helpText: '클릭 또는 드래그로 위치 설정. Shift+드래그: 수직 이동. Ctrl+드래그: 그리드 스냅.',
            flipH: '좌우 반전',
            flipV: '상하 반전'
        }
    },

    init() {
        const saved = localStorage.getItem('anchorGrid_language');
        if (saved) {
            this.currentLang = saved;
        }
        this.applyLanguage();
    },

    setLanguage(lang) {
        this.currentLang = lang;
        localStorage.setItem('anchorGrid_language', lang);
        this.applyLanguage();
    },

    get(key) {
        return this.strings[this.currentLang]?.[key] || this.strings.en[key] || key;
    },

    applyLanguage() {
        // Update section titles
        document.querySelectorAll('.section-title').forEach(el => {
            const key = el.dataset.i18n;
            if (key) el.textContent = this.get(key);
        });

        // Update other elements with data-i18n attribute
        document.querySelectorAll('[data-i18n]').forEach(el => {
            const key = el.dataset.i18n;
            if (key) el.textContent = this.get(key);
        });

        // Update language select
        const langSelect = document.getElementById('language-select');
        if (langSelect) {
            langSelect.value = this.currentLang;
        }
    }
};

// Export for use in other modules
window.i18n = i18n;
