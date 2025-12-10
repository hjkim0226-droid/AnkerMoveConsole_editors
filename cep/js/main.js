/**
 * Anchor Grid - Main Entry Point
 * Handles CEP initialization and C++ plugin communication
 */

(function () {
    'use strict';

    let grid = null;
    let csInterface = null;

    /**
     * Initialize the CEP panel
     */
    function init() {
        csInterface = new CSInterface();
        grid = new AnchorGrid();

        // Listen for commands from C++ plugin
        csInterface.addEventListener('com.anchor.grid.showRadialMenu', onShowRadialMenu);
        csInterface.addEventListener('com.anchor.grid.hideRadialMenu', onHideRadialMenu);
        csInterface.addEventListener('com.anchor.grid.toggleClickMode', onToggleClickMode);

        console.log('Anchor Grid CEP panel initialized');
    }

    /**
     * Handle show radial menu command (Y key pressed)
     */
    function onShowRadialMenu(event) {
        if (!grid.isVisible) {
            grid.show(false); // Hold mode
        }
    }

    /**
     * Handle hide radial menu command (Y key released)
     */
    function onHideRadialMenu(event) {
        if (grid.isVisible && !grid.isClickMode) {
            grid.applySelectionAndHide();
        }
    }

    /**
     * Handle toggle click mode (Option+Y pressed)
     */
    function onToggleClickMode(event) {
        if (grid.isVisible) {
            grid.hide();
        } else {
            grid.show(true); // Click mode
        }
    }

    /**
     * Get current grid selection (called from C++ via ExtendScript)
     */
    window.getCurrentGridSelection = function () {
        const selection = grid ? grid.getSelection() : null;
        if (selection) {
            return JSON.stringify(selection);
        }
        return null;
    };

    // Initialize when DOM is ready
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', init);
    } else {
        init();
    }
})();
