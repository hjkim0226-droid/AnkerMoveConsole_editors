/**
 * Anchor Grid - Settings Module
 * Handles grid settings, modes, and persistence
 */

class Settings {
    constructor() {
        this.defaults = {
            gridWidth: 3,
            gridHeight: 3,
            compMode: false,
            maskRecognition: true,
            gridOpacity: 75,
            cellOpacity: 50
        };

        this.current = { ...this.defaults };
        this.onSettingsChange = null;

        this.load();
        this.bindEvents();
        this.applyOpacity();
    }

    /**
     * Load settings from localStorage
     */
    load() {
        try {
            const saved = localStorage.getItem('anchorGridSettings');
            if (saved) {
                const parsed = JSON.parse(saved);
                this.current = { ...this.defaults, ...parsed };
            }
        } catch (e) {
            console.error('Failed to load settings:', e);
        }

        this.updateUI();
    }

    /**
     * Save settings to localStorage
     */
    save() {
        try {
            localStorage.setItem('anchorGridSettings', JSON.stringify(this.current));
        } catch (e) {
            console.error('Failed to save settings:', e);
        }

        if (this.onSettingsChange) {
            this.onSettingsChange(this.current);
        }
    }

    /**
     * Update UI elements to reflect current settings
     */
    updateUI() {
        const widthInput = document.getElementById('grid-width');
        const heightInput = document.getElementById('grid-height');
        const compToggle = document.getElementById('toggle-comp-mode');
        const maskToggle = document.getElementById('toggle-mask');

        if (widthInput) widthInput.value = this.current.gridWidth;
        if (heightInput) heightInput.value = this.current.gridHeight;

        if (compToggle) {
            compToggle.classList.toggle('active', this.current.compMode);
        }
        if (maskToggle) {
            maskToggle.classList.toggle('active', this.current.maskRecognition);
        }
    }

    /**
     * Bind UI event handlers
     */
    bindEvents() {
        // Grid size inputs
        const widthInput = document.getElementById('grid-width');
        const heightInput = document.getElementById('grid-height');

        if (widthInput) {
            widthInput.addEventListener('change', (e) => {
                const val = this.clampGridSize(parseInt(e.target.value, 10));
                e.target.value = val;
                this.current.gridWidth = val;
                this.save();
            });
        }

        if (heightInput) {
            heightInput.addEventListener('change', (e) => {
                const val = this.clampGridSize(parseInt(e.target.value, 10));
                e.target.value = val;
                this.current.gridHeight = val;
                this.save();
            });
        }

        // Toggle switches
        const compToggle = document.getElementById('toggle-comp-mode');
        const maskToggle = document.getElementById('toggle-mask');

        if (compToggle) {
            compToggle.addEventListener('click', () => {
                this.current.compMode = !this.current.compMode;
                compToggle.classList.toggle('active', this.current.compMode);
                this.save();
            });
        }

        if (maskToggle) {
            maskToggle.addEventListener('click', () => {
                this.current.maskRecognition = !this.current.maskRecognition;
                maskToggle.classList.toggle('active', this.current.maskRecognition);
                this.save();
            });
        }
    }

    /**
     * Clamp grid size to valid range (3-7)
     */
    clampGridSize(val) {
        if (isNaN(val)) return 3;
        return Math.max(3, Math.min(7, val));
    }

    /**
     * Get current grid dimensions
     */
    getGridSize() {
        return {
            width: this.current.gridWidth,
            height: this.current.gridHeight
        };
    }

    /**
     * Check if composition mode is enabled
     */
    isCompMode() {
        return this.current.compMode;
    }

    /**
     * Check if mask recognition is enabled
     */
    isMaskEnabled() {
        return this.current.maskRecognition;
    }

    /**
     * Apply opacity settings to CSS variables
     */
    applyOpacity() {
        const gridOpacity = this.current.gridOpacity / 100;
        const cellOpacity = this.current.cellOpacity / 100;

        document.documentElement.style.setProperty('--bg-secondary', `rgba(45, 45, 45, ${gridOpacity})`);
        document.documentElement.style.setProperty('--grid-cell-bg', `rgba(74, 158, 255, ${cellOpacity * 0.15})`);
        document.documentElement.style.setProperty('--grid-cell-hover', `rgba(74, 158, 255, ${cellOpacity * 0.7})`);
        document.documentElement.style.setProperty('--grid-cell-active', `rgba(74, 158, 255, ${cellOpacity})`);

        // Update UI labels
        const gridOpacityValue = document.getElementById('grid-opacity-value');
        const cellOpacityValue = document.getElementById('cell-opacity-value');
        const gridOpacitySlider = document.getElementById('grid-opacity');
        const cellOpacitySlider = document.getElementById('cell-opacity');

        if (gridOpacityValue) gridOpacityValue.textContent = this.current.gridOpacity + '%';
        if (cellOpacityValue) cellOpacityValue.textContent = this.current.cellOpacity + '%';
        if (gridOpacitySlider) gridOpacitySlider.value = this.current.gridOpacity;
        if (cellOpacitySlider) cellOpacitySlider.value = this.current.cellOpacity;
    }

    /**
     * Bind opacity slider events (called after DOM ready)
     */
    bindOpacityEvents() {
        const gridOpacitySlider = document.getElementById('grid-opacity');
        const cellOpacitySlider = document.getElementById('cell-opacity');

        if (gridOpacitySlider) {
            gridOpacitySlider.addEventListener('input', (e) => {
                this.current.gridOpacity = parseInt(e.target.value, 10);
                this.applyOpacity();
                this.save();
            });
        }

        if (cellOpacitySlider) {
            cellOpacitySlider.addEventListener('input', (e) => {
                this.current.cellOpacity = parseInt(e.target.value, 10);
                this.applyOpacity();
                this.save();
            });
        }
    }
}

// Export
window.Settings = Settings;
