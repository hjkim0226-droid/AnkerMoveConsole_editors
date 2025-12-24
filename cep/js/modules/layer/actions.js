/**
 * actions.js - Layer Module Dynamic Actions Manager
 * Manages layer-type specific action configurations
 */

(function() {
    'use strict';

    // Master action definitions (matches C++ ActionDefinition)
    const ALL_ACTIONS = {
        // Text actions
        typewriter:     { label: 'Typewriter',      desc: 'Animate text typing',        types: ['text'] },
        fadeIn:         { label: 'Fade In',         desc: 'Fade in characters',         types: ['text'] },
        scale:          { label: 'Scale',           desc: 'Scale characters',           types: ['text'] },
        blur:           { label: 'Blur',            desc: 'Blur characters',            types: ['text'] },
        tracking:       { label: 'Tracking',        desc: 'Animate tracking',           types: ['text'] },

        // Shape actions
        trimPath:       { label: 'Trim Path',       desc: 'Add trim paths',             types: ['shape'] },
        repeater:       { label: 'Repeater',        desc: 'Add repeater',               types: ['shape'] },
        wigglePath:     { label: 'Wiggle Path',     desc: 'Add wiggle paths',           types: ['shape'] },
        wiggleTransform:{ label: 'Wiggle Transform',desc: 'Add wiggle transform',       types: ['shape'] },

        // Solid actions
        changeColor:    { label: 'Change Color',    desc: 'Change solid color',         types: ['solid'] },
        fitToComp:      { label: 'Fit to Comp',     desc: 'Match comp dimensions',      types: ['solid'] },

        // Footage actions
        loopCycle:      { label: 'Loop (Cycle)',    desc: 'Loop with cycle',            types: ['footage'] },
        loopPingPong:   { label: 'Loop (Ping Pong)',desc: 'Loop back and forth',        types: ['footage'] },
        lastFrameHold:  { label: 'Last Frame Hold', desc: 'Freeze last frame',          types: ['footage'] },

        // Precomp actions
        unPrecompose:   { label: 'Un-Precompose',   desc: 'Extract layers from precomp',types: ['precomp'] },
        deepCopy:       { label: 'Deep Copy',       desc: 'Duplicate comp hierarchy',   types: ['precomp'] },
        fitToLayers:    { label: 'Fit to Layers',   desc: 'Fit comp to layer range',    types: ['precomp'] },

        // Common actions
        resetTransform: { label: 'Reset Transform', desc: 'Reset pos/scale/rot',        types: ['text', 'shape', 'solid', 'footage', 'precomp', 'null'] },
        resetPosition:  { label: 'Reset Position',  desc: 'Reset position only',        types: ['camera', 'light'] },
    };

    // Default configurations (order matters)
    const DEFAULT_CONFIG = {
        text: [
            { id: 'typewriter', enabled: true },
            { id: 'fadeIn', enabled: true },
            { id: 'scale', enabled: true },
            { id: 'blur', enabled: true },
            { id: 'tracking', enabled: true },
        ],
        shape: [
            { id: 'trimPath', enabled: true },
            { id: 'repeater', enabled: true },
            { id: 'wigglePath', enabled: true },
            { id: 'wiggleTransform', enabled: true },
        ],
        solid: [
            { id: 'changeColor', enabled: true },
            { id: 'fitToComp', enabled: true },
            { id: 'resetTransform', enabled: true },
        ],
        footage: [
            { id: 'loopCycle', enabled: true },
            { id: 'loopPingPong', enabled: true },
            { id: 'lastFrameHold', enabled: true },
            { id: 'resetTransform', enabled: true },
        ],
        precomp: [
            { id: 'unPrecompose', enabled: true },
            { id: 'deepCopy', enabled: true },
            { id: 'fitToLayers', enabled: true },
            { id: 'resetTransform', enabled: true },
        ],
        null: [
            { id: 'resetTransform', enabled: true },
        ],
        camera: [
            { id: 'resetPosition', enabled: true },
        ],
        light: [
            { id: 'resetPosition', enabled: true },
        ]
    };

    // Current configuration (will be loaded from file or use default)
    let currentConfig = JSON.parse(JSON.stringify(DEFAULT_CONFIG));

    // UI Elements
    let layerTypeSelect = null;
    let actionListEl = null;
    let resetBtn = null;
    let saveBtn = null;

    /**
     * Get actions for a layer type
     */
    function getActionsForType(layerType) {
        const config = currentConfig[layerType] || [];
        return config.map((item, index) => {
            const def = ALL_ACTIONS[item.id];
            return {
                id: item.id,
                enabled: item.enabled,
                label: def ? def.label : item.id,
                desc: def ? def.desc : '',
                shortcut: index + 1
            };
        });
    }

    /**
     * Create action item element using safe DOM methods
     */
    function createActionItem(action, index, layerType, enabledIndex) {
        const shortcut = action.enabled ? enabledIndex : '-';

        const item = document.createElement('div');
        item.className = 'action-item' + (action.enabled ? '' : ' disabled');
        item.dataset.id = action.id;
        item.dataset.index = index;

        // Checkbox
        const checkbox = document.createElement('input');
        checkbox.type = 'checkbox';
        checkbox.className = 'action-checkbox';
        checkbox.checked = action.enabled;
        checkbox.addEventListener('change', function() {
            currentConfig[layerType][index].enabled = this.checked;
            renderActionList(layerType);
        });
        item.appendChild(checkbox);

        // Drag handle
        const drag = document.createElement('span');
        drag.className = 'action-drag';
        drag.textContent = '≡';
        item.appendChild(drag);

        // Action name
        const name = document.createElement('span');
        name.className = 'action-name';
        name.textContent = action.label;
        item.appendChild(name);

        // Shortcut badge
        const shortcutEl = document.createElement('span');
        shortcutEl.className = 'action-shortcut';
        shortcutEl.textContent = shortcut;
        item.appendChild(shortcutEl);

        // Move buttons container
        const moveBtns = document.createElement('div');
        moveBtns.className = 'action-move-btns';

        // Up button
        const upBtn = document.createElement('button');
        upBtn.className = 'action-move-btn';
        upBtn.textContent = '▲';
        upBtn.addEventListener('click', function() {
            moveAction(layerType, index, 'up');
        });
        moveBtns.appendChild(upBtn);

        // Down button
        const downBtn = document.createElement('button');
        downBtn.className = 'action-move-btn';
        downBtn.textContent = '▼';
        downBtn.addEventListener('click', function() {
            moveAction(layerType, index, 'down');
        });
        moveBtns.appendChild(downBtn);

        item.appendChild(moveBtns);

        return item;
    }

    /**
     * Render action list UI
     */
    function renderActionList(layerType) {
        if (!actionListEl) return;

        const actions = getActionsForType(layerType);

        // Clear existing items
        while (actionListEl.firstChild) {
            actionListEl.removeChild(actionListEl.firstChild);
        }

        let enabledIndex = 0;
        actions.forEach((action, index) => {
            if (action.enabled) enabledIndex++;
            const item = createActionItem(action, index, layerType, enabledIndex);
            actionListEl.appendChild(item);
        });
    }

    /**
     * Move action up or down
     */
    function moveAction(layerType, index, direction) {
        const config = currentConfig[layerType];
        if (!config) return;

        const newIndex = direction === 'up' ? index - 1 : index + 1;
        if (newIndex < 0 || newIndex >= config.length) return;

        // Swap
        const temp = config[index];
        config[index] = config[newIndex];
        config[newIndex] = temp;

        renderActionList(layerType);
    }

    /**
     * Reset to default configuration
     */
    function resetToDefault(layerType) {
        if (DEFAULT_CONFIG[layerType]) {
            currentConfig[layerType] = JSON.parse(JSON.stringify(DEFAULT_CONFIG[layerType]));
            renderActionList(layerType);
        }
    }

    /**
     * Save configuration to file and notify C++
     */
    function saveConfig() {
        var layerType = layerTypeSelect ? layerTypeSelect.value : 'text';

        // Save to localStorage for now (will be replaced with file save)
        try {
            localStorage.setItem('anchorsnap-layer-config', JSON.stringify(currentConfig));
        } catch (e) {
            console.error('Failed to save config:', e);
        }

        // Notify C++ plugin
        if (window.csInterface) {
            var configJson = JSON.stringify(currentConfig);
            window.csInterface.evalScript(
                'setLayerActionConfig(' + JSON.stringify(configJson) + ')'
            );
        }

        // Visual feedback
        if (saveBtn) {
            var originalText = saveBtn.textContent;
            saveBtn.textContent = 'Saved!';
            saveBtn.style.background = '#4aff8a';
            setTimeout(function() {
                saveBtn.textContent = originalText;
                saveBtn.style.background = '';
            }, 1000);
        }
    }

    /**
     * Load configuration from storage
     */
    function loadConfig() {
        try {
            var saved = localStorage.getItem('anchorsnap-layer-config');
            if (saved) {
                var parsed = JSON.parse(saved);
                // Merge with defaults to ensure all types exist
                Object.keys(DEFAULT_CONFIG).forEach(function(type) {
                    if (parsed[type]) {
                        currentConfig[type] = parsed[type];
                    }
                });
            }
        } catch (e) {
            console.error('Failed to load config:', e);
        }
    }

    /**
     * Initialize the module
     */
    function init() {
        layerTypeSelect = document.getElementById('layer-type-select');
        actionListEl = document.getElementById('layer-action-list');
        resetBtn = document.getElementById('layer-reset-btn');
        saveBtn = document.getElementById('layer-save-btn');

        if (!layerTypeSelect || !actionListEl) return;

        // Load saved config
        loadConfig();

        // Initial render
        renderActionList(layerTypeSelect.value);

        // Layer type change
        layerTypeSelect.addEventListener('change', function() {
            renderActionList(layerTypeSelect.value);
        });

        // Reset button
        if (resetBtn) {
            resetBtn.addEventListener('click', function() {
                resetToDefault(layerTypeSelect.value);
            });
        }

        // Save button
        if (saveBtn) {
            saveBtn.addEventListener('click', saveConfig);
        }
    }

    // Initialize on DOM ready
    document.addEventListener('DOMContentLoaded', init);

    // Export for external use
    window.LayerActionsModule = {
        getConfig: function() { return currentConfig; },
        getActionsForType: getActionsForType,
        saveConfig: saveConfig,
        loadConfig: loadConfig,
        resetAll: function() {
            currentConfig = JSON.parse(JSON.stringify(DEFAULT_CONFIG));
            if (layerTypeSelect) renderActionList(layerTypeSelect.value);
        }
    };
})();
