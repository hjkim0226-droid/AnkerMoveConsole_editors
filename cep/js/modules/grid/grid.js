/**
 * grid.js - Grid preview functionality
 * Handles the interactive grid preview in the settings panel
 */

class AnchorGrid {
    constructor(settings) {
        this.settings = settings;
        this.container = document.getElementById('preview-grid');
    }

    build() {
        if (!this.container) return;

        const w = this.settings.get('gridWidth');
        const h = this.settings.get('gridHeight');

        this.container.style.gridTemplateColumns = `repeat(${w}, 1fr)`;
        this.container.style.gridTemplateRows = `repeat(${h}, 1fr)`;
        this.container.innerHTML = '';

        for (let y = 0; y < h; y++) {
            for (let x = 0; x < w; x++) {
                const cell = this.createCell(x, y, w, h);
                this.container.appendChild(cell);
            }
        }
    }

    createCell(x, y, gridW, gridH) {
        const cell = document.createElement('div');
        cell.className = 'preview-cell';
        cell.dataset.x = x;
        cell.dataset.y = y;

        // Determine mark type
        const isLeft = x === 0;
        const isRight = x === gridW - 1;
        const isTop = y === 0;
        const isBottom = y === gridH - 1;
        const isCorner = (isLeft || isRight) && (isTop || isBottom);
        const isEdge = (isLeft || isRight || isTop || isBottom) && !isCorner;

        let markChar = '+';
        if (isCorner) {
            if (isTop && isLeft) markChar = '┌';
            else if (isTop && isRight) markChar = '┐';
            else if (isBottom && isLeft) markChar = '└';
            else markChar = '┘';
        } else if (isEdge) {
            if (isTop) markChar = '┬';
            else if (isBottom) markChar = '┴';
            else if (isLeft) markChar = '├';
            else markChar = '┤';
        }

        const mark = document.createElement('span');
        mark.className = 'mark';
        mark.textContent = markChar;
        cell.appendChild(mark);

        cell.addEventListener('click', () => {
            this.onCellClick(x, y, gridW, gridH);
        });

        return cell;
    }

    onCellClick(x, y, gridW, gridH) {
        if (!window.csInterface) {
            console.log(`Grid click: ${x}, ${y}`);
            return;
        }

        const script = `setLayerAnchor(${x}, ${y}, ${gridW}, ${gridH})`;
        csInterface.evalScript(script);
    }
}

// Export
window.AnchorGrid = AnchorGrid;
