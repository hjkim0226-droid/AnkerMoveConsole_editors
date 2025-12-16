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
        // Deep copy to ensure settings gets a new array
        const copy = this.presets.map(p => ({ x: p.x, y: p.y }));
        this.settings.set('customAnchors', copy);
        console.log('Saved presets:', copy);
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

        // Paste button - paste from clipboard (stored anchor)
        document.getElementById('paste-anchor')?.addEventListener('click', () => this.pasteAnchor());

        // Coordinate input handlers - both change and drag
        this.coordX?.addEventListener('change', (e) => this.setCoordX(parseInt(e.target.value)));
        this.coordY?.addEventListener('change', (e) => this.setCoordY(parseInt(e.target.value)));

        // Add drag-to-change functionality for coordinates
        this.bindCoordDrag(this.coordX, 'x');
        this.bindCoordDrag(this.coordY, 'y');
    }

    bindCoordDrag(element, axis) {
        if (!element) return;

        let isDragging = false;
        let startX = 0;
        let startValue = 0;
        const self = this;

        element.style.cursor = 'ew-resize';

        element.addEventListener('mousedown', (e) => {
            isDragging = true;
            startX = e.clientX;
            startValue = parseInt(element.value) || 50;
            document.body.style.cursor = 'ew-resize';
            e.preventDefault();
        });

        document.addEventListener('mousemove', (e) => {
            if (!isDragging) return;

            const diff = Math.floor((e.clientX - startX) / 3); // 3px per 1%
            let newValue = startValue + diff;
            newValue = Math.max(-20, Math.min(120, newValue));

            element.value = newValue;
            if (axis === 'x') {
                self.setCoordX(newValue);
            } else {
                self.setCoordY(newValue);
            }
        });

        document.addEventListener('mouseup', () => {
            if (isDragging) {
                isDragging = false;
                document.body.style.cursor = '';
            }
        });
    }

    setCoordX(val) {
        if (isNaN(val)) return;
        this.currentX = Math.max(-20, Math.min(120, val));
        this.presets[this.selectedPreset] = { x: this.currentX, y: this.currentY };
        this.savePresets();
        this.updateUI();
    }

    setCoordY(val) {
        if (isNaN(val)) return;
        this.currentY = Math.max(-20, Math.min(120, val));
        this.presets[this.selectedPreset] = { x: this.currentX, y: this.currentY };
        this.savePresets();
        this.updateUI();
    }

    pasteAnchor() {
        // Reload settings from file to get latest clipboard anchor from C++
        this.loadClipboardFromFile();

        const clipboard = this.settings.get('clipboardAnchor');
        console.log('Clipboard data:', clipboard);

        // Validate clipboard has valid values
        if (clipboard && typeof clipboard.x === 'number' && typeof clipboard.y === 'number') {
            // Convert 0-1 ratio to 0-100 percent, allow -20 to 120 range (overflow)
            let x = Math.round(clipboard.x * 100);
            let y = Math.round(clipboard.y * 100);

            // Clamp to allowed range (-20 to 120)
            x = Math.max(-20, Math.min(120, x));
            y = Math.max(-20, Math.min(120, y));

            this.currentX = x;
            this.currentY = y;
            this.presets[this.selectedPreset] = { x: this.currentX, y: this.currentY };
            this.savePresets();
            this.updateUI();
            console.log('Pasted anchor:', this.currentX, this.currentY);
        } else {
            console.log('No clipboard anchor available');
        }
    }

    loadClipboardFromFile() {
        // Read clipboard anchor directly from settings.json
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

            console.log('Loading clipboard from:', settingsPath);

            if (fs.existsSync(settingsPath)) {
                const json = fs.readFileSync(settingsPath, 'utf8');
                const parsed = JSON.parse(json);
                console.log('Parsed clipboardAnchor:', parsed.clipboardAnchor);
                if (parsed.clipboardAnchor && parsed.clipboardAnchor.x !== undefined) {
                    this.settings.settings.clipboardAnchor = parsed.clipboardAnchor;
                    console.log('Updated clipboard in settings');
                }
            }
        } catch (e) {
            console.error('Failed to load clipboard from file:', e);
        }
    }

    onMouseDown(e) {
        this.isDragging = true;
        // Store drag start position
        this.dragStartX = this.currentX;
        this.dragStartY = this.currentY;
        this.dragAxis = null; // Will be determined on first move with shift
        this.updatePositionFromMouse(e);
    }

    onMouseMove(e) {
        if (!this.isDragging) return;
        this.updatePositionFromMouse(e);
    }

    onMouseUp() {
        if (this.isDragging) {
            this.isDragging = false;
            this.dragAxis = null;
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

        // Shift: constrain to axis (auto-detect from drag direction)
        if (e.shiftKey && this.isDragging) {
            const deltaX = Math.abs(x - this.dragStartX);
            const deltaY = Math.abs(y - this.dragStartY);

            // Determine axis on first significant movement
            if (!this.dragAxis && (deltaX > 5 || deltaY > 5)) {
                this.dragAxis = deltaX > deltaY ? 'horizontal' : 'vertical';
            }

            // Constrain based on determined axis
            if (this.dragAxis === 'horizontal') {
                y = this.dragStartY;
            } else if (this.dragAxis === 'vertical') {
                x = this.dragStartX;
            }
        }

        // Ctrl: snap to grid (use settings gridWidth/gridHeight)
        if (e.ctrlKey || e.metaKey) {
            // Get grid size from settings
            const gridWidth = this.settings?.get('gridWidth') || 3;
            const gridHeight = this.settings?.get('gridHeight') || 3;

            // Calculate snap intervals
            const snapX = 100 / (gridWidth - 1);
            const snapY = 100 / (gridHeight - 1);

            x = Math.round(x / snapX) * snapX;
            y = Math.round(y / snapY) * snapY;
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

        // Update coordinates display (now inputs)
        if (this.coordX) this.coordX.value = this.currentX;
        if (this.coordY) this.coordY.value = this.currentY;

        // Update grid lines based on settings
        // Grid cells = setting - 1 (e.g., 3x3 setting -> 2x2 grid cells)
        if (this.editor && this.settings) {
            const gridWidth = this.settings.get('gridWidth') || 3;
            const gridHeight = this.settings.get('gridHeight') || 3;
            // Grid cells = gridWidth - 1, so lines = gridWidth - 1 divisions
            const cellW = 100 / (gridWidth - 1);
            const cellH = 100 / (gridHeight - 1);
            this.editor.style.backgroundSize = `${cellW}% ${cellH}%`;
        }
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
