/**
 * custom-anchor.js - Custom anchor editor with drag support
 * Handles anchor position editing with mouse drag, shift/ctrl modifiers, and flip buttons
 */

class CustomAnchor {
    constructor(settings) {
        this.settings = settings;
        this.selectedPreset = 0;
        this.isDragging = false;
        this.currentX = 50; // Percent
        this.currentY = 50;

        this.editor = document.getElementById('anchor-editor');
        this.marker = document.getElementById('anchor-marker');
        this.coordX = document.getElementById('anchor-x');
        this.coordY = document.getElementById('anchor-y');

        // Add inner circle to marker
        if (this.marker) {
            const circle = document.createElement('span');
            this.marker.appendChild(circle);
        }

        this.loadPresets();
        this.bindEvents();
        this.updateUI();
    }

    loadPresets() {
        const saved = this.settings.get('customAnchors');
        if (saved && saved.length === 3) {
            this.presets = saved;
        } else {
            this.presets = [
                { x: 50, y: 50 },
                { x: 50, y: 50 },
                { x: 50, y: 50 }
            ];
        }
        this.selectPreset(this.settings.get('selectedPreset') || 0);
    }

    savePresets() {
        this.settings.set('customAnchors', this.presets);
    }

    selectPreset(index) {
        this.selectedPreset = index;
        this.settings.set('selectedPreset', index);

        // Update button states
        document.querySelectorAll('.custom-btn').forEach((btn, i) => {
            btn.classList.toggle('active', i === index);
        });

        // Load preset position
        const preset = this.presets[index];
        this.currentX = preset.x;
        this.currentY = preset.y;
        this.updateUI();
    }

    bindEvents() {
        // Preset buttons
        document.querySelectorAll('.custom-btn').forEach((btn, i) => {
            btn.addEventListener('click', () => this.selectPreset(i));
        });

        // Editor mouse events
        if (this.editor) {
            this.editor.addEventListener('mousedown', (e) => this.onMouseDown(e));
            document.addEventListener('mousemove', (e) => this.onMouseMove(e));
            document.addEventListener('mouseup', () => this.onMouseUp());
        }

        // Flip buttons
        document.getElementById('flip-h')?.addEventListener('click', () => this.flipHorizontal());
        document.getElementById('flip-v')?.addEventListener('click', () => this.flipVertical());
    }

    onMouseDown(e) {
        this.isDragging = true;
        this.updatePositionFromMouse(e);
    }

    onMouseMove(e) {
        if (!this.isDragging) return;
        this.updatePositionFromMouse(e);
    }

    onMouseUp() {
        if (this.isDragging) {
            this.isDragging = false;
            // Save to current preset
            this.presets[this.selectedPreset] = { x: this.currentX, y: this.currentY };
            this.savePresets();
        }
    }

    updatePositionFromMouse(e) {
        if (!this.editor) return;

        const rect = this.editor.getBoundingClientRect();
        let x = ((e.clientX - rect.left) / rect.width) * 100;
        let y = ((e.clientY - rect.top) / rect.height) * 100;

        // Shift: constrain to vertical (keep x at center or current start)
        if (e.shiftKey) {
            x = this.isDragging ? this.currentX : 50;
        }

        // Ctrl: snap to 5x5 grid (0, 25, 50, 75, 100)
        if (e.ctrlKey || e.metaKey) {
            x = Math.round(x / 25) * 25;
            y = Math.round(y / 25) * 25;
        }

        // Clamp to -20 to 120 (allow overflow)
        x = Math.max(-20, Math.min(120, x));
        y = Math.max(-20, Math.min(120, y));

        this.currentX = Math.round(x);
        this.currentY = Math.round(y);
        this.updateUI();
    }

    flipHorizontal() {
        this.currentX = 100 - this.currentX;
        this.presets[this.selectedPreset] = { x: this.currentX, y: this.currentY };
        this.savePresets();
        this.updateUI();
    }

    flipVertical() {
        this.currentY = 100 - this.currentY;
        this.presets[this.selectedPreset] = { x: this.currentX, y: this.currentY };
        this.savePresets();
        this.updateUI();
    }

    updateUI() {
        // Update marker position
        if (this.marker) {
            // Clamp visual position to editor bounds
            const visualX = Math.max(0, Math.min(100, this.currentX));
            const visualY = Math.max(0, Math.min(100, this.currentY));
            this.marker.style.left = visualX + '%';
            this.marker.style.top = visualY + '%';
        }

        // Update coordinates display
        if (this.coordX) this.coordX.textContent = this.currentX;
        if (this.coordY) this.coordY.textContent = this.currentY;
    }

    // Get current anchor as ratio (0-1) for ExtendScript
    getCurrentAnchorRatio() {
        return {
            x: this.currentX / 100,
            y: this.currentY / 100
        };
    }

    // Apply current anchor to selected layers
    applyToLayers() {
        if (!window.csInterface) return;

        const ratio = this.getCurrentAnchorRatio();
        const script = `setCustomAnchor(${ratio.x}, ${ratio.y})`;
        csInterface.evalScript(script);
    }
}

// Export
window.CustomAnchor = CustomAnchor;
