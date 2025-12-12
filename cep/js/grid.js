/**
 * Anchor Grid - Grid UI Component
 * Handles grid creation, interaction, and selection
 * Supports asymmetric grids (different width/height)
 */

class AnchorGrid {
    constructor(settings) {
        this.settings = settings;
        this.gridWidth = settings ? settings.getGridSize().width : 3;
        this.gridHeight = settings ? settings.getGridSize().height : 3;
        this.gridContainer = document.getElementById('anchor-grid');
        this.overlay = document.getElementById('grid-overlay');
        this.selectedCell = null;
        this.isVisible = false;
        this.isClickMode = false;
        this.onSelectionChange = null;

        this.buildGrid();
        this.bindEvents();

        // Listen for settings changes
        if (settings) {
            settings.onSettingsChange = (newSettings) => {
                this.gridWidth = newSettings.gridWidth;
                this.gridHeight = newSettings.gridHeight;
                this.buildGrid();
            };
        }
    }

    /**
     * Build the grid cells based on current size
     */
    buildGrid() {
        this.gridContainer.innerHTML = '';
        this.gridContainer.style.gridTemplateColumns = `repeat(${this.gridWidth}, 1fr)`;
        this.gridContainer.style.gridTemplateRows = `repeat(${this.gridHeight}, 1fr)`;

        // Calculate center - for even grids, center is between cells
        const centerX = (this.gridWidth - 1) / 2;
        const centerY = (this.gridHeight - 1) / 2;
        const isEvenWidth = this.gridWidth % 2 === 0;
        const isEvenHeight = this.gridHeight % 2 === 0;

        for (let y = 0; y < this.gridHeight; y++) {
            for (let x = 0; x < this.gridWidth; x++) {
                const cell = document.createElement('div');
                cell.className = 'grid-cell';
                cell.dataset.x = x;
                cell.dataset.y = y;

                // Mark center cell(s)
                if (isEvenWidth || isEvenHeight) {
                    // For even grids, mark the 4 (or 2) center cells
                    const isCenterX = isEvenWidth
                        ? (x === Math.floor(centerX) || x === Math.ceil(centerX))
                        : x === Math.floor(centerX);
                    const isCenterY = isEvenHeight
                        ? (y === Math.floor(centerY) || y === Math.ceil(centerY))
                        : y === Math.floor(centerY);

                    if (isCenterX && isCenterY) {
                        cell.classList.add('center-zone');
                    }
                } else {
                    // Odd grid - single center
                    if (x === Math.floor(centerX) && y === Math.floor(centerY)) {
                        cell.classList.add('center');
                    }
                }

                // Hover events
                cell.addEventListener('mouseenter', () => this.onCellHover(cell, x, y));
                cell.addEventListener('mouseleave', () => this.onCellLeave(cell));
                cell.addEventListener('click', () => this.onCellClick(x, y));

                this.gridContainer.appendChild(cell);
            }
        }
    }

    /**
     * Bind global events
     */
    bindEvents() {
        // Track mouse leaving grid area
        this.overlay.addEventListener('mousemove', (e) => {
            const gridRect = this.gridContainer.getBoundingClientRect();
            const isOutside =
                e.clientX < gridRect.left - 20 ||
                e.clientX > gridRect.right + 20 ||
                e.clientY < gridRect.top - 20 ||
                e.clientY > gridRect.bottom + 20;

            if (isOutside) {
                this.overlay.classList.add('outside-grid');
                this.selectedCell = null;
                this.notifySelectionChange(null);
            } else {
                this.overlay.classList.remove('outside-grid');
            }
        });

        // Settings button in toolbar
        const settingsBtn = document.getElementById('btn-settings');
        if (settingsBtn) {
            settingsBtn.addEventListener('click', () => {
                this.hide();
                // Show settings panel
                const panel = document.getElementById('settings-panel');
                if (panel) panel.classList.add('visible');
            });
        }
    }

    /**
     * Notify parent about selection changes (for state file)
     */
    notifySelectionChange(selection) {
        if (this.onSelectionChange) {
            this.onSelectionChange(selection);
        }
    }

    /**
     * Handle cell hover
     */
    onCellHover(cell, x, y) {
        document.querySelectorAll('.grid-cell').forEach(c => c.classList.remove('active'));
        cell.classList.add('active');
        this.selectedCell = { x, y };
        this.notifySelectionChange(this.selectedCell);
    }

    /**
     * Handle cell leave
     */
    onCellLeave(cell) {
        cell.classList.remove('active');
    }

    /**
     * Handle cell click (for click mode)
     */
    onCellClick(x, y) {
        if (this.isClickMode) {
            this.applyAnchor(x, y);
            this.hide();
        }
    }

    /**
     * Show the grid overlay
     */
    show(clickMode = false) {
        this.isClickMode = clickMode;
        this.isVisible = true;
        this.selectedCell = null;

        // Refresh grid in case settings changed
        if (this.settings) {
            const size = this.settings.getGridSize();
            if (size.width !== this.gridWidth || size.height !== this.gridHeight) {
                this.gridWidth = size.width;
                this.gridHeight = size.height;
                this.buildGrid();
            }
        }

        this.overlay.classList.remove('hidden');
        this.overlay.classList.remove('outside-grid');

        // Reset all cells
        document.querySelectorAll('.grid-cell').forEach(c => c.classList.remove('active'));

        // Hide settings panel when grid is shown
        const panel = document.getElementById('settings-panel');
        if (panel) panel.classList.remove('visible');

        this.notifySelectionChange(null);
    }

    /**
     * Hide the grid overlay
     */
    hide() {
        this.isVisible = false;
        this.overlay.classList.add('hidden');
        this.notifySelectionChange(null);
    }

    /**
     * Get currently selected cell (for hold mode)
     */
    getSelection() {
        return this.selectedCell;
    }

    /**
     * Apply anchor point to selected layer
     */
    applyAnchor(gridX, gridY) {
        const csInterface = new CSInterface();
        const compMode = this.settings ? this.settings.isCompMode() : false;
        const maskEnabled = this.settings ? this.settings.isMaskEnabled() : true;

        const script = compMode
            ? `setCompositionAnchor(${gridX}, ${gridY}, ${this.gridWidth}, ${this.gridHeight})`
            : `setLayerAnchor(${gridX}, ${gridY}, ${this.gridWidth}, ${this.gridHeight}, ${maskEnabled})`;

        console.log('Applying anchor:', gridX, gridY, 'grid:', this.gridWidth, 'x', this.gridHeight, 'compMode:', compMode);

        csInterface.evalScript(script, (result) => {
            console.log('ExtendScript result:', result);
            if (result === 'EvalScript error.') {
                console.error('Failed to apply anchor point');
            } else {
                console.log('Anchor applied:', result);
            }
        });
    }

    /**
     * Apply custom anchor (ratio-based)
     */
    applyCustomAnchor(ratioX, ratioY) {
        const csInterface = new CSInterface();
        const compMode = this.settings ? this.settings.isCompMode() : false;

        const script = compMode
            ? `setCompositionCustomAnchor(${ratioX}, ${ratioY})`
            : `setLayerCustomAnchor(${ratioX}, ${ratioY})`;

        console.log('Applying custom anchor:', ratioX, ratioY, 'compMode:', compMode);

        csInterface.evalScript(script, (result) => {
            console.log('Custom anchor result:', result);
        });
    }

    /**
     * Apply selection and hide (for hold mode release)
     */
    applySelectionAndHide() {
        if (this.selectedCell) {
            this.applyAnchor(this.selectedCell.x, this.selectedCell.y);
        }
        this.hide();
    }
}

// Export for use in main.js
window.AnchorGrid = AnchorGrid;
