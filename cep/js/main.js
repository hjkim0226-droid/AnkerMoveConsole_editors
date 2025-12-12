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
    let settings = null;
    let customAnchor = null;
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
            return path.join(os.homedir(), 'Library/Application Support/AnchorRadialMenu');
        } else {
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

        // Initialize modules
        settings = new Settings();
        customAnchor = new CustomAnchor();
        grid = new AnchorGrid(settings);

        // Link custom anchor apply to grid
        customAnchor.onApply = (x, y) => {
            grid.applyCustomAnchor(x, y);
        };

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
        watchInterval = setInterval(() => {
            try {
                if (fs.existsSync(commandFilePath)) {
                    const command = fs.readFileSync(commandFilePath, 'utf8').trim();
                    if (command && command !== lastCommand) {
                        lastCommand = command;
                        processCommand(command);
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
                    grid.show(false);
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
                    grid.show(true);
                }
                break;

            case 'openSettings':
                grid.hide();
                const panel = document.getElementById('settings-panel');
                if (panel) panel.classList.add('visible');
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
                // Handle applyCustomAnchor:X,Y command
                else if (command.startsWith('applyCustomAnchor:')) {
                    const coords = command.substring(18).split(',');
                    if (coords.length === 2) {
                        const ratioX = parseFloat(coords[0]);
                        const ratioY = parseFloat(coords[1]);
                        grid.applyCustomAnchor(ratioX, ratioY);
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
            const size = settings.getGridSize();
            const stateData = {
                gridWidth: size.width,
                gridHeight: size.height,
                compMode: settings.isCompMode(),
                maskEnabled: settings.isMaskEnabled()
            };

            if (selection) {
                stateData.selection = `${selection.x},${selection.y}`;
            } else {
                stateData.selection = 'outside';
            }

            fs.writeFileSync(stateFilePath, JSON.stringify(stateData));
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
