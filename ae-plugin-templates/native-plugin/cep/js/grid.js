/**
 * Anchor Grid - Grid UI Component
 * Handles grid creation, interaction, and selection
 */

class AnchorGrid {
    constructor() {
        this.gridSize = 3;
        this.gridContainer = document.getElementById('anchor-grid');
        this.overlay = document.getElementById('grid-overlay');
        this.selectedCell = null;
        this.isVisible = false;
        this.isClickMode = false;
        this.onSelectionChange = null; // Callback for state file updates

        this.loadSettings();
        this.buildGrid();
        this.bindEvents();
    }

    /**
     * Load grid settings from localStorage
     */
    loadSettings() {
        const savedSize = localStorage.getItem('anchorGridSize');
        if (savedSize) {
            this.gridSize = parseInt(savedSize, 10);
            const sizeSelect = document.getElementById('grid-size');
            if (sizeSelect) {
                sizeSelect.value = this.gridSize;
            }
        }
    }

    /**
     * Save grid settings to localStorage
     */
    saveSettings() {
        localStorage.setItem('anchorGridSize', this.gridSize.toString());
    }

    /**
     * Build the grid cells based on current size
     */
    buildGrid() {
        this.gridContainer.innerHTML = '';
        this.gridContainer.style.gridTemplateColumns = `repeat(${this.gridSize}, 1fr)`;
        this.gridContainer.style.gridTemplateRows = `repeat(${this.gridSize}, 1fr)`;

        const centerIndex = Math.floor(this.gridSize / 2);

        for (let y = 0; y < this.gridSize; y++) {
            for (let x = 0; x < this.gridSize; x++) {
                const cell = document.createElement('div');
                cell.className = 'grid-cell';
                cell.dataset.x = x;
                cell.dataset.y = y;

                // Mark center cell
                if (x === centerIndex && y === centerIndex) {
                    cell.classList.add('center');
                }

                // Hover events
                cell.addEventListener('mouseenter', () => this.onCellHover(cell, x, y));
                cell.addEventListener('mouseleave', () => this.onCellLeave(cell));
                cell.addEventListener('click', () => this.onCellClick(x, y));

                this.gridContainer.appendChild(cell);
            }
        }

        // Add cancel zone indicator
        const cancelZone = document.createElement('div');
        cancelZone.className = 'cancel-zone';
        cancelZone.textContent = 'Release to cancel';
        this.gridContainer.appendChild(cancelZone);
    }

    /**
     * Bind global events
     */
    bindEvents() {
        // Grid size change
        const sizeSelect = document.getElementById('grid-size');
        if (sizeSelect) {
            sizeSelect.addEventListener('change', (e) => {
                this.gridSize = parseInt(e.target.value, 10);
                this.saveSettings();
                this.buildGrid();
            });
        }

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
        // Remove active from all cells
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
        this.overlay.classList.remove('hidden');
        this.overlay.classList.remove('outside-grid');

        // Reset all cells
        document.querySelectorAll('.grid-cell').forEach(c => c.classList.remove('active'));

        // Notify initial state
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
        const script = `setLayerAnchor(${gridX}, ${gridY}, ${this.gridSize})`;

        // Debug: Show what coordinates are being sent
        console.log('Applying anchor:', gridX, gridY, 'gridSize:', this.gridSize);

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
