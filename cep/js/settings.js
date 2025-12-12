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
            maskRecognition: true
        };

        this.current = { ...this.defaults };
        this.onSettingsChange = null;

        this.load();
        this.bindEvents();
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
}

// Export
window.Settings = Settings;
