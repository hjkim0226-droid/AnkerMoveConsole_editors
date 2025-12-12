/**
 * Anchor Grid - Main Entry Point
 * Handles CEP initialization and C++ plugin communication via file-based IPC
 */

(function () {
    'use strict';

    const fs = require('fs');
    const path = require('path');
    const os = require('os');

    let grid = null;
    let csInterface = null;
    let commandFilePath = '';
    let stateFilePath = '';
    let watchInterval = null;
    let lastCommand = '';

    /**
     * Get IPC directory path based on OS
     */
    function getIPCDirectory() {
        if (process.platform === 'darwin') {
            // macOS
            return path.join(os.homedir(), 'Library/Application Support/AnchorRadialMenu');
        } else {
            // Windows
            return path.join(process.env.APPDATA || 'C:\\Temp', 'AnchorRadialMenu');
        }
    }

    /**
     * Ensure IPC directory exists
     */
    function ensureIPCDirectory() {
        const dir = getIPCDirectory();
        if (!fs.existsSync(dir)) {
            fs.mkdirSync(dir, { recursive: true });
        }
        return dir;
    }

    /**
     * Initialize the CEP panel
     */
    function init() {
        csInterface = new CSInterface();
        grid = new AnchorGrid();

        // Setup IPC paths
        const ipcDir = ensureIPCDirectory();
        commandFilePath = path.join(ipcDir, 'command.txt');
        stateFilePath = path.join(ipcDir, 'state.txt');

        // Create empty command file if it doesn't exist
        if (!fs.existsSync(commandFilePath)) {
            fs.writeFileSync(commandFilePath, '');
        }

        // Start watching for commands from C++ plugin
        startCommandWatcher();

        // Listen for grid state changes to write to state file
        grid.onSelectionChange = writeStateFile;

        console.log('Anchor Grid CEP panel initialized');
        console.log('IPC Directory:', ipcDir);
    }

    /**
     * Start watching the command file for changes from C++ plugin
     */
    function startCommandWatcher() {
        // Poll the command file every 50ms
        watchInterval = setInterval(() => {
            try {
                if (fs.existsSync(commandFilePath)) {
                    const command = fs.readFileSync(commandFilePath, 'utf8').trim();
                    if (command && command !== lastCommand) {
                        lastCommand = command;
                        processCommand(command);
                        // Clear the command file after processing
                        fs.writeFileSync(commandFilePath, '');
                    }
                }
            } catch (err) {
                console.error('Error reading command file:', err);
            }
        }, 50);
    }

    /**
     * Process a command from the C++ plugin
     */
    function processCommand(command) {
        console.log('Received command:', command);

        switch (command) {
            case 'showRadialMenu':
                if (!grid.isVisible) {
                    grid.show(false); // Hold mode
                }
                break;

            case 'hideRadialMenu':
                if (grid.isVisible && !grid.isClickMode) {
                    grid.applySelectionAndHide();
                }
                break;

            case 'toggleClickMode':
                if (grid.isVisible) {
                    grid.hide();
                } else {
                    grid.show(true); // Click mode
                }
                break;

            default:
                // Handle applyAnchor:X,Y command
                if (command.startsWith('applyAnchor:')) {
                    const coords = command.substring(12).split(',');
                    if (coords.length === 2) {
                        const gridX = parseInt(coords[0], 10);
                        const gridY = parseInt(coords[1], 10);
                        grid.applyAnchor(gridX, gridY);
                    }
                }
                break;
        }
    }

    /**
     * Write current grid state to state file (for C++ plugin to read)
     */
    function writeStateFile(selection) {
        try {
            if (selection) {
                fs.writeFileSync(stateFilePath, `${selection.x},${selection.y}`);
            } else {
                fs.writeFileSync(stateFilePath, 'outside');
            }
        } catch (err) {
            console.error('Error writing state file:', err);
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

    /**
     * Cleanup on unload
     */
    window.addEventListener('unload', () => {
        if (watchInterval) {
            clearInterval(watchInterval);
        }
    });

    // Initialize when DOM is ready
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', init);
    } else {
        init();
    }
})();
