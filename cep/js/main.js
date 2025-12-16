/**
 * main.js - Main entry point for Anchor Grid CEP panel
 * Initializes all modules and handles IPC with C++ plugin
 */

// Global references
let csInterface = null;
let settings = null;
let customAnchor = null;
let grid = null;

// Initialize when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
    init();
});

// Debug output to UI
function debugLog(msg) {
    console.log(msg);
    const debugEl = document.getElementById('debug-output');
    if (debugEl) {
        debugEl.innerHTML = '[' + new Date().toLocaleTimeString() + '] ' + msg + '<br>' + debugEl.innerHTML;
        debugEl.innerHTML = debugEl.innerHTML.substring(0, 2000); // Limit size
    }
    const errorEl = document.getElementById('error-message');
    if (errorEl && msg.toLowerCase().includes('error')) {
        errorEl.textContent = msg;
        errorEl.classList.add('visible');
    }
}
window.debugLog = debugLog;

async function init() {
    debugLog('Init starting...');
    try {
        // Initialize CSInterface
        if (typeof CSInterface === 'undefined') {
            debugLog('ERROR: CSInterface not loaded!');
            return;
        }
        csInterface = new CSInterface();
        window.csInterface = csInterface;
        debugLog('CSInterface created');

        // Load ExtendScript
        const jsxPath = csInterface.getSystemPath(SystemPath.EXTENSION) + '/jsx/anchor.jsx';
        csInterface.evalScript(`$.evalFile("${jsxPath}")`);

        // Initialize i18n
        if (window.i18n) {
            i18n.init();
        }

        // Initialize modules
        debugLog('Creating Settings...');
        settings = new Settings();
        debugLog('Settings created, gridWidth=' + settings.get('gridWidth'));

        debugLog('Creating CustomAnchor...');
        customAnchor = new CustomAnchor(settings);
        window.customAnchor = customAnchor; // Expose for real-time updates

        debugLog('Creating Grid...');
        grid = new AnchorGrid(settings);
        grid.build();
        debugLog('Grid built');

        // Apply theme
        applyTheme();

        // Listen for mode changes from C++ plugin via CSXSEvent
        csInterface.addEventListener('anchorGridModeChanged', (event) => {
            try {
                // event.data may be string or object depending on source
                const data = typeof event.data === 'string' ? JSON.parse(event.data) : event.data;
                if (typeof data.useCompMode === 'boolean') {
                    settings.settings.useCompMode = data.useCompMode;
                }
                if (typeof data.useMaskRecognition === 'boolean') {
                    settings.settings.useMaskRecognition = data.useMaskRecognition;
                }
                settings.updateModeButtons();
                settings.save(); // Persist to localStorage
                debugLog('Mode synced from popup: compMode=' + data.useCompMode + ', mask=' + data.useMaskRecognition);
            } catch (e) {
                debugLog('Error parsing mode event: ' + e.message);
            }
        });

        // Sync when panel gets focus (catches changes made while panel was in background)
        // Also re-set settingsPanelOpen since panel may have been hidden/shown without reload
        window.addEventListener('focus', () => {
            if (settings) {
                settings.loadFromFile();
                settings.set('settingsPanelOpen', true);
            }
        });

        // Mark settings panel as closed when panel is closing
        window.addEventListener('beforeunload', () => {
            if (settings) {
                settings.set('settingsPanelOpen', false);
            }
        });

        console.log('Anchor Grid panel initialized');

    } catch (error) {
        debugLog('ERROR: ' + error.message);
        showError('Failed to initialize: ' + error.message);
    }
}

function applyTheme() {
    // Get host environment colors (optional)
    if (csInterface) {
        const hostEnv = csInterface.getHostEnvironment();
        const appSkinInfo = csInterface.hostEnvironment?.appSkinInfo;
        if (appSkinInfo) {
            // Could apply AE's native colors here
        }
    }
}

function showError(message) {
    const errorEl = document.getElementById('error-message');
    if (errorEl) {
        errorEl.textContent = message;
        errorEl.classList.add('visible');

        // Auto-hide after 5 seconds
        setTimeout(() => {
            errorEl.classList.remove('visible');
        }, 5000);
    }
    console.error(message);
}

// ExtendScript functions to call from JSX
function setLayerAnchor(x, y, gridW, gridH) {
    if (!csInterface) return;

    const useMask = settings.get('useMaskRecognition');
    const useComp = settings.get('useCompMode');

    let script;
    if (useComp) {
        script = `setCompositionAnchor(${x}, ${y}, ${gridW}, ${gridH})`;
    } else if (useMask) {
        script = `setAnchorWithMask(${x}, ${y}, ${gridW}, ${gridH})`;
    } else {
        script = `setLayerAnchor(${x}, ${y}, ${gridW}, ${gridH})`;
    }

    csInterface.evalScript(script, (result) => {
        if (result && result.startsWith('Error:')) {
            showError(result);
        }
    });
}

function setCustomAnchor(ratioX, ratioY) {
    if (!csInterface) return;

    const useMask = settings.get('useMaskRecognition');
    const script = useMask
        ? `setCustomAnchorWithMask(${ratioX}, ${ratioY})`
        : `setCustomAnchor(${ratioX}, ${ratioY})`;

    csInterface.evalScript(script, (result) => {
        if (result && result.startsWith('Error:')) {
            showError(result);
        }
    });
}

// Export functions for ExtendScript callbacks
window.setLayerAnchor = setLayerAnchor;
window.setCustomAnchor = setCustomAnchor;
window.showError = showError;
