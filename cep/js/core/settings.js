/**
 * settings.js - Settings management module
 * Handles grid size, scale, mode, transparency, and persistence
 */

class Settings {
    constructor() {
        console.log('Settings constructor called');
        this.defaults = {
            gridWidth: 3,
            gridHeight: 3,
            gridScale: 2, // 0-9 representing -20% to +70% (default: 0%)
            useCompMode: false,
            useMaskRecognition: true,
            settingsPanelOpen: false, // Track if CEP panel is visible
            gridOpacity: 75,
            cellOpacity: 50,
            language: 'en',
            // Custom anchor presets (0-100 percent)
            customAnchors: [
                { x: 50, y: 50 },
                { x: 50, y: 50 },
                { x: 50, y: 50 }
            ],
            selectedPreset: 0,
            // Clipboard anchor for copy/paste (ratio 0-1)
            clipboardAnchor: null
        };

        this.settings = { ...this.defaults };
        this.load();
        this.bindEvents();

        // Mark panel as open and save
        this.set('settingsPanelOpen', true);
    }

    load() {
        try {
            const saved = localStorage.getItem('anchorSnap_settings');
            if (saved) {
                const parsed = JSON.parse(saved);
                this.settings = { ...this.defaults, ...parsed };
            }
        } catch (e) {
            console.error('Failed to load settings:', e);
        }
        this.applyToUI();
    }

    save() {
        try {
            localStorage.setItem('anchorSnap_settings', JSON.stringify(this.settings));
            this.saveToFile(); // Also save for C++ plugin
        } catch (e) {
            console.error('Failed to save settings:', e);
        }
    }

    saveToFile() {
        if (!window.csInterface) return;

        const json = JSON.stringify(this.settings, null, 2);

        try {
            const os = require('os');
            const path = require('path');
            const fs = require('fs');
            let settingsPath;

            if (os.platform() === 'win32') {
                settingsPath = path.join(process.env.APPDATA, 'Adobe', 'CEP', 'extensions', 'com.anchor.snap', 'settings.json');
            } else {
                settingsPath = path.join(os.homedir(), 'Library', 'Application Support', 'Adobe', 'CEP', 'extensions', 'com.anchor.snap', 'settings.json');
            }

            // Ensure directory exists
            const dir = path.dirname(settingsPath);
            if (!fs.existsSync(dir)) {
                fs.mkdirSync(dir, { recursive: true });
            }
            fs.writeFileSync(settingsPath, json, 'utf8');
            console.log('Settings saved to:', settingsPath);
        } catch (e) {
            console.error('Failed to write settings file:', e);
        }
    }

    loadFromFile() {
        // Load settings from file (syncs with C++ plugin changes)
        try {
            const os = require('os');
            const path = require('path');
            const fs = require('fs');
            let settingsPath;

            if (os.platform() === 'win32') {
                settingsPath = path.join(process.env.APPDATA, 'Adobe', 'CEP', 'extensions', 'com.anchor.snap', 'settings.json');
            } else {
                settingsPath = path.join(os.homedir(), 'Library', 'Application Support', 'Adobe', 'CEP', 'extensions', 'com.anchor.snap', 'settings.json');
            }

            if (fs.existsSync(settingsPath)) {
                const json = fs.readFileSync(settingsPath, 'utf8');
                const parsed = JSON.parse(json);

                // Only update mode settings (what C++ can change)
                if (typeof parsed.useCompMode === 'boolean') {
                    this.settings.useCompMode = parsed.useCompMode;
                }
                if (typeof parsed.useMaskRecognition === 'boolean') {
                    this.settings.useMaskRecognition = parsed.useMaskRecognition;
                }

                this.applyToUI();
                console.log('Settings loaded from file');
            }
        } catch (e) {
            console.error('Failed to load settings from file:', e);
        }
    }

    get(key) {
        return this.settings[key];
    }

    set(key, value) {
        this.settings[key] = value;
        this.save();
    }

    applyToUI() {
        // Grid size
        const gridWidth = document.getElementById('grid-width');
        const gridHeight = document.getElementById('grid-height');
        if (gridWidth) gridWidth.textContent = this.settings.gridWidth;
        if (gridHeight) gridHeight.textContent = this.settings.gridHeight;

        // Grid scale
        const gridScale = document.getElementById('grid-scale');
        const scaleDisplay = document.getElementById('scale-display');
        if (gridScale) gridScale.value = this.settings.gridScale;
        if (scaleDisplay) {
            const scaleValues = ['-20%', '-10%', '0%', '+10%', '+20%', '+30%', '+40%', '+50%', '+60%', '+70%'];
            scaleDisplay.textContent = scaleValues[this.settings.gridScale];
        }

        // Mode buttons
        this.updateModeButtons();

        // Opacity sliders
        const gridOpacity = document.getElementById('grid-opacity');
        const cellOpacity = document.getElementById('cell-opacity');
        if (gridOpacity) {
            gridOpacity.value = this.settings.gridOpacity;
            document.getElementById('grid-opacity-value').textContent = this.settings.gridOpacity + '%';
        }
        if (cellOpacity) {
            cellOpacity.value = this.settings.cellOpacity;
            document.getElementById('cell-opacity-value').textContent = this.settings.cellOpacity + '%';
        }

        // Language
        const langSelect = document.getElementById('language-select');
        if (langSelect) langSelect.value = this.settings.language;

        // Build preview grid
        this.buildPreviewGrid();
    }

    updateModeButtons() {
        const btnSelection = document.getElementById('btn-selection');
        const btnComposition = document.getElementById('btn-composition');
        const btnMaskOn = document.getElementById('btn-mask-on');
        const btnMaskOff = document.getElementById('btn-mask-off');

        if (btnSelection && btnComposition) {
            btnSelection.classList.toggle('active', !this.settings.useCompMode);
            btnComposition.classList.toggle('active', this.settings.useCompMode);
        }

        if (btnMaskOn && btnMaskOff) {
            btnMaskOn.classList.toggle('active', this.settings.useMaskRecognition);
            btnMaskOff.classList.toggle('active', !this.settings.useMaskRecognition);
        }
    }

    buildPreviewGrid() {
        const container = document.getElementById('preview-grid');
        if (!container) return;

        const w = this.settings.gridWidth;
        const h = this.settings.gridHeight;

        container.style.gridTemplateColumns = `repeat(${w}, 1fr)`;
        container.style.gridTemplateRows = `repeat(${h}, 1fr)`;
        container.innerHTML = '';

        for (let y = 0; y < h; y++) {
            for (let x = 0; x < w; x++) {
                const cell = document.createElement('div');
                cell.className = 'preview-cell';
                cell.dataset.x = x;
                cell.dataset.y = y;

                // Add mark indicator (CSS will render crosshair)
                const mark = document.createElement('span');
                mark.className = 'mark';
                // No text - CSS ::before and ::after create the crosshair
                cell.appendChild(mark);

                cell.addEventListener('click', () => {
                    this.onCellClick(x, y);
                });

                container.appendChild(cell);
            }
        }
    }

    onCellClick(x, y) {
        // Trigger anchor apply via ExtendScript
        if (window.csInterface) {
            const script = `setLayerAnchor(${x}, ${y}, ${this.settings.gridWidth}, ${this.settings.gridHeight})`;
            csInterface.evalScript(script);
        }
    }

    bindEvents() {
        // Grid size drag
        this.bindSizeControl('grid-width', 'gridWidth');
        this.bindSizeControl('grid-height', 'gridHeight');

        // Grid scale slider
        const gridScale = document.getElementById('grid-scale');
        if (gridScale) {
            gridScale.addEventListener('input', (e) => {
                const value = parseInt(e.target.value);
                this.set('gridScale', value);
                const scaleValues = ['-20%', '-10%', '0%', '+10%', '+20%', '+30%', '+40%', '+50%', '+60%', '+70%'];
                document.getElementById('scale-display').textContent = scaleValues[value];
            });
        }

        // Mode buttons
        document.getElementById('btn-selection')?.addEventListener('click', () => {
            this.set('useCompMode', false);
            this.updateModeButtons();
        });

        document.getElementById('btn-composition')?.addEventListener('click', () => {
            this.set('useCompMode', true);
            this.updateModeButtons();
        });

        document.getElementById('btn-mask-on')?.addEventListener('click', () => {
            this.set('useMaskRecognition', true);
            this.updateModeButtons();
        });

        document.getElementById('btn-mask-off')?.addEventListener('click', () => {
            this.set('useMaskRecognition', false);
            this.updateModeButtons();
        });

        // Opacity sliders
        document.getElementById('grid-opacity')?.addEventListener('input', (e) => {
            const value = parseInt(e.target.value);
            this.set('gridOpacity', value);
            document.getElementById('grid-opacity-value').textContent = value + '%';
        });

        document.getElementById('cell-opacity')?.addEventListener('input', (e) => {
            const value = parseInt(e.target.value);
            this.set('cellOpacity', value);
            document.getElementById('cell-opacity-value').textContent = value + '%';
        });

        // Language
        document.getElementById('language-select')?.addEventListener('change', (e) => {
            this.set('language', e.target.value);
            if (window.i18n) {
                i18n.setLanguage(e.target.value);
            }
        });
    }

    bindSizeControl(elementId, settingKey) {
        const element = document.getElementById(elementId);
        if (!element) {
            console.warn('Element not found:', elementId);
            return;
        }
        console.log('Binding size control:', elementId, 'for', settingKey);

        let startX = 0;
        let startY = 0;
        let startValue = 0;
        let isDragging = false;
        const isHeight = settingKey.toLowerCase().includes('height');

        const self = this; // Capture this for closures

        element.addEventListener('mousedown', (e) => {
            isDragging = true;
            startX = e.clientX;
            startY = e.clientY;
            startValue = self.settings[settingKey];
            document.body.style.cursor = 'ew-resize'; // Always horizontal
            element.style.color = 'var(--blue)';
            e.preventDefault();
            console.log('Mousedown on', elementId, 'startValue:', startValue);
        });

        document.addEventListener('mousemove', (e) => {
            if (!isDragging) return;

            // Both width and height use X axis (horizontal drag)
            const diff = Math.floor((e.clientX - startX) / 20);
            let newValue = startValue + diff;
            const min = parseInt(element.dataset.min) || 3;
            const max = parseInt(element.dataset.max) || 7;
            newValue = Math.max(min, Math.min(max, newValue));

            if (newValue !== self.settings[settingKey]) {
                self.set(settingKey, newValue);
                element.textContent = newValue;
                if (self.buildPreviewGrid) self.buildPreviewGrid();
                // Also update custom anchor grid
                if (window.customAnchor && window.customAnchor.updateUI) {
                    window.customAnchor.updateUI();
                }
                console.log('Value changed:', settingKey, '=', newValue);
            }
        });

        document.addEventListener('mouseup', () => {
            if (isDragging) {
                isDragging = false;
                document.body.style.cursor = '';
                element.style.color = '';
                console.log('Mouseup, saved value:', self.settings[settingKey]);
            }
        });

        // Click to edit
        element.addEventListener('dblclick', () => {
            const current = self.settings[settingKey];
            const input = prompt(`Enter value (3-7):`, current);
            if (input !== null) {
                let val = parseInt(input);
                if (!isNaN(val)) {
                    val = Math.max(3, Math.min(7, val));
                    self.set(settingKey, val);
                    element.textContent = val;
                    if (self.buildPreviewGrid) self.buildPreviewGrid();
                }
            }
        });
    }
}

// Export
window.Settings = Settings;
