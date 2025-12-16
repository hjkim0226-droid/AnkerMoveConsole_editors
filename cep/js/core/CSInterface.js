/**
 * CSInterface - CEP 11.0
 * Adobe Creative Suite Extension Interface
 * Minimal implementation for Anchor Grid
 */

function CSInterface() { }

/**
 * HostEnvironment holds info about the host app
 */
CSInterface.prototype.hostEnvironment = {};

/**
 * Evaluates a JavaScript script in the script context of the host application
 */
CSInterface.prototype.evalScript = function (script, callback) {
    if (callback === undefined) {
        callback = function (result) { };
    }

    if (window.__adobe_cep__) {
        window.__adobe_cep__.evalScript(script, callback);
    } else {
        console.warn('CEP not available - running in browser mode');
        callback('Browser mode - no script execution');
    }
};

/**
 * Registers an event callback function for a CEP event
 */
CSInterface.prototype.addEventListener = function (type, listener, obj) {
    if (window.__adobe_cep__) {
        window.__adobe_cep__.addEventListener(type, listener, obj);
    } else {
        // Fallback for browser testing
        document.addEventListener(type, listener);
    }
};

/**
 * Removes an event listener
 */
CSInterface.prototype.removeEventListener = function (type, listener, obj) {
    if (window.__adobe_cep__) {
        window.__adobe_cep__.removeEventListener(type, listener, obj);
    } else {
        document.removeEventListener(type, listener);
    }
};

/**
 * Dispatches a CEP event
 */
CSInterface.prototype.dispatchEvent = function (event) {
    if (window.__adobe_cep__) {
        window.__adobe_cep__.dispatchEvent(event);
    } else {
        document.dispatchEvent(new CustomEvent(event.type, { detail: event.data }));
    }
};

/**
 * Returns the API version
 */
CSInterface.prototype.getCurrentApiVersion = function () {
    return { major: 11, minor: 0, micro: 0 };
};

/**
 * Gets the host environment data
 */
CSInterface.prototype.getHostEnvironment = function () {
    if (window.__adobe_cep__) {
        var env = window.__adobe_cep__.getHostEnvironment();
        return JSON.parse(env);
    }
    return {
        appName: 'AEFT',
        appVersion: '25.0',
        appLocale: 'en_US'
    };
};

/**
 * Gets the system path
 * @param {string} pathType - The type of path to get
 */
CSInterface.prototype.getSystemPath = function (pathType) {
    if (window.__adobe_cep__) {
        return window.__adobe_cep__.getSystemPath(pathType);
    }
    return '';
};

/**
 * Opens a URL in the default browser
 */
CSInterface.prototype.openURLInDefaultBrowser = function (url) {
    if (window.__adobe_cep__) {
        window.__adobe_cep__.openURLInDefaultBrowser(url);
    } else {
        window.open(url, '_blank');
    }
};

/**
 * Closes the extension
 */
CSInterface.prototype.closeExtension = function () {
    if (window.__adobe_cep__) {
        window.__adobe_cep__.closeExtension();
    }
};

/**
 * CSEvent class for dispatching events
 */
function CSEvent(type, scope, appId, extensionId) {
    this.type = type;
    this.scope = scope;
    this.appId = appId;
    this.extensionId = extensionId;
    this.data = '';
}

// Path type constants
CSInterface.prototype.EXTENSION_ID = 'extensionId';
SystemPath = {
    USER_DATA: 'userData',
    COMMON_FILES: 'commonFiles',
    MY_DOCUMENTS: 'myDocuments',
    APPLICATION: 'application',
    EXTENSION: 'extension',
    HOST_APPLICATION: 'hostApplication'
};
