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

async function init() {
    try {
        // Initialize CSInterface
        csInterface = new CSInterface();
        window.csInterface = csInterface;

        // Load ExtendScript
        const jsxPath = csInterface.getSystemPath(SystemPath.EXTENSION) + '/jsx/anchor.jsx';
        csInterface.evalScript(`$.evalFile("${jsxPath}")`);

        // Initialize i18n
        if (window.i18n) {
            i18n.init();
        }

        // Initialize modules
        settings = new Settings();
        customAnchor = new CustomAnchor(settings);
        grid = new AnchorGrid(settings);
        grid.build();

        // Apply theme
        applyTheme();

        console.log('Anchor Grid panel initialized');

    } catch (error) {
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
