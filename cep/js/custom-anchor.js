/**
 * Anchor Grid - Custom Anchor Module
 * Handles custom anchor positioning and presets
 */

class CustomAnchor {
    constructor() {
        this.presets = [];
        this.currentX = 0.5;
        this.currentY = 0.5;
        this.onApply = null;

        this.load();
        this.bindEvents();
        this.renderPresets();
    }

    /**
     * Load presets from localStorage
     */
    load() {
        try {
            const saved = localStorage.getItem('anchorPresets');
            if (saved) {
                this.presets = JSON.parse(saved);
            }
        } catch (e) {
            console.error('Failed to load presets:', e);
        }
    }

    /**
     * Save presets to localStorage
     */
    save() {
        try {
            localStorage.setItem('anchorPresets', JSON.stringify(this.presets));
        } catch (e) {
            console.error('Failed to save presets:', e);
        }
    }

    /**
     * Bind UI event handlers
     */
    bindEvents() {
        const preview = document.getElementById('anchor-preview');
        const xInput = document.getElementById('anchor-x');
        const yInput = document.getElementById('anchor-y');
        const saveBtn = document.getElementById('btn-save-preset');
        const applyBtn = document.getElementById('btn-apply-custom');
        const copyBtn = document.getElementById('btn-copy-anchor');

        // Click on preview to set anchor position
        if (preview) {
            preview.addEventListener('click', (e) => {
                const rect = preview.getBoundingClientRect();
                this.currentX = Math.max(0, Math.min(1, (e.clientX - rect.left) / rect.width));
                this.currentY = Math.max(0, Math.min(1, (e.clientY - rect.top) / rect.height));
                this.updatePreviewPoint();
                this.updateInputs();
            });
        }

        // Input changes
        if (xInput) {
            xInput.addEventListener('change', () => {
                const val = parseFloat(xInput.value);
                if (!isNaN(val)) {
                    this.currentX = Math.max(0, Math.min(1, val));
                    xInput.value = this.currentX.toFixed(3);
                    this.updatePreviewPoint();
                }
            });
        }

        if (yInput) {
            yInput.addEventListener('change', () => {
                const val = parseFloat(yInput.value);
                if (!isNaN(val)) {
                    this.currentY = Math.max(0, Math.min(1, val));
                    yInput.value = this.currentY.toFixed(3);
                    this.updatePreviewPoint();
                }
            });
        }

        // Save preset button
        if (saveBtn) {
            saveBtn.addEventListener('click', () => {
                this.saveCurrentAsPreset();
            });
        }

        // Apply custom anchor
        if (applyBtn) {
            applyBtn.addEventListener('click', () => {
                if (this.onApply) {
                    this.onApply(this.currentX, this.currentY);
                }
            });
        }

        // Copy anchor ratio from selected layer
        if (copyBtn) {
            copyBtn.addEventListener('click', () => {
                this.copyAnchorFromLayer();
            });
        }
    }

    /**
     * Update the visual anchor point position
     */
    updatePreviewPoint() {
        const point = document.getElementById('anchor-point');
        if (point) {
            point.style.left = `${this.currentX * 100}%`;
            point.style.top = `${this.currentY * 100}%`;
        }
    }

    /**
     * Update input fields with current values
     */
    updateInputs() {
        const xInput = document.getElementById('anchor-x');
        const yInput = document.getElementById('anchor-y');

        if (xInput) xInput.value = this.currentX.toFixed(3);
        if (yInput) yInput.value = this.currentY.toFixed(3);
    }

    /**
     * Save current position as a preset
     */
    saveCurrentAsPreset() {
        const name = prompt('Enter preset name:', `Preset ${this.presets.length + 1}`);
        if (name && name.trim()) {
            this.presets.push({
                name: name.trim(),
                x: this.currentX,
                y: this.currentY
            });
            this.save();
            this.renderPresets();
        }
    }

    /**
     * Add a preset directly (used by copy anchor feature)
     */
    addPreset(x, y, name = null) {
        const presetName = name || `Copied ${this.presets.length + 1}`;
        this.presets.push({
            name: presetName,
            x: x,
            y: y
        });
        this.save();
        this.renderPresets();
    }

    /**
     * Delete a preset by index
     */
    deletePreset(index) {
        if (index >= 0 && index < this.presets.length) {
            this.presets.splice(index, 1);
            this.save();
            this.renderPresets();
        }
    }

    /**
     * Apply a preset
     */
    applyPreset(index) {
        if (index >= 0 && index < this.presets.length) {
            const preset = this.presets[index];
            this.currentX = preset.x;
            this.currentY = preset.y;
            this.updatePreviewPoint();
            this.updateInputs();

            if (this.onApply) {
                this.onApply(this.currentX, this.currentY);
            }
        }
    }

    /**
     * Render preset list in UI
     */
    renderPresets() {
        const list = document.getElementById('preset-list');
        if (!list) return;

        list.innerHTML = '';

        this.presets.forEach((preset, index) => {
            const item = document.createElement('div');
            item.className = 'preset-item';
            item.innerHTML = `
                <span class="preset-name">${this.escapeHtml(preset.name)}</span>
                <span class="preset-value">${preset.x.toFixed(2)}, ${preset.y.toFixed(2)}</span>
                <span class="preset-delete" data-index="${index}">✕</span>
            `;

            // Click to apply
            item.addEventListener('click', (e) => {
                if (!e.target.classList.contains('preset-delete')) {
                    this.applyPreset(index);
                }
            });

            // Delete button
            const deleteBtn = item.querySelector('.preset-delete');
            deleteBtn.addEventListener('click', (e) => {
                e.stopPropagation();
                this.deletePreset(index);
            });

            list.appendChild(item);
        });
    }

    /**
     * Copy anchor ratio from currently selected layer
     */
    copyAnchorFromLayer() {
        const csInterface = new CSInterface();
        csInterface.evalScript('getLayerAnchorRatio()', (result) => {
            if (result && result !== 'EvalScript error.' && result !== 'null') {
                try {
                    const data = JSON.parse(result);
                    if (data && typeof data.x === 'number' && typeof data.y === 'number') {
                        this.currentX = data.x;
                        this.currentY = data.y;
                        this.updatePreviewPoint();
                        this.updateInputs();

                        // Auto-add as preset
                        this.addPreset(data.x, data.y, `Copied (${data.x.toFixed(2)}, ${data.y.toFixed(2)})`);
                    }
                } catch (e) {
                    console.error('Failed to parse anchor ratio:', e);
                }
            } else {
                console.log('No layer selected or failed to get anchor ratio');
            }
        });
    }

    /**
     * Escape HTML to prevent XSS
     */
    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
}

// Export
window.CustomAnchor = CustomAnchor;
