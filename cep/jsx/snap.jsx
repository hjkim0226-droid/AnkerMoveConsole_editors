/**
 * anchor.jsx - ExtendScript for After Effects anchor point manipulation
 * Supports grid-based, custom ratio, composition mode, and mask recognition
 */

// Main function: Set anchor by grid position
function setLayerAnchor(gridX, gridY, gridWidth, gridHeight) {
    try {
        var comp = app.project.activeItem;
        if (!comp || !(comp instanceof CompItem)) {
            return "Error: No active composition";
        }
        if (comp.selectedLayers.length === 0) {
            return "Error: No layers selected";
        }

        var px = gridX / (gridWidth - 1);
        var py = gridY / (gridHeight - 1);

        app.beginUndoGroup("Set Anchor Point");

        for (var i = 0; i < comp.selectedLayers.length; i++) {
            var layer = comp.selectedLayers[i];
            setAnchorByRatio(layer, comp, px, py, false);
        }

        app.endUndoGroup();
        return "OK";

    } catch (e) {
        return "Error: " + e.toString();
    }
}

// Set anchor with mask bounds recognition
function setAnchorWithMask(gridX, gridY, gridWidth, gridHeight) {
    try {
        var comp = app.project.activeItem;
        if (!comp || !(comp instanceof CompItem)) {
            return "Error: No active composition";
        }
        if (comp.selectedLayers.length === 0) {
            return "Error: No layers selected";
        }

        var px = gridX / (gridWidth - 1);
        var py = gridY / (gridHeight - 1);

        app.beginUndoGroup("Set Anchor Point (Mask)");

        for (var i = 0; i < comp.selectedLayers.length; i++) {
            var layer = comp.selectedLayers[i];
            setAnchorByRatio(layer, comp, px, py, true);
        }

        app.endUndoGroup();
        return "OK";

    } catch (e) {
        return "Error: " + e.toString();
    }
}

// Set anchor for entire composition
function setCompositionAnchor(gridX, gridY, gridWidth, gridHeight) {
    try {
        var comp = app.project.activeItem;
        if (!comp || !(comp instanceof CompItem)) {
            return "Error: No active composition";
        }

        var px = gridX / (gridWidth - 1);
        var py = gridY / (gridHeight - 1);

        // Composition bounds
        var bounds = {
            left: 0,
            top: 0,
            width: comp.width,
            height: comp.height
        };

        var nx = bounds.left + bounds.width * px;
        var ny = bounds.top + bounds.height * py;

        app.beginUndoGroup("Set Anchor (Composition)");

        for (var i = 0; i < comp.selectedLayers.length; i++) {
            var layer = comp.selectedLayers[i];
            applyAnchorToLayer(layer, comp, nx, ny);
        }

        app.endUndoGroup();
        return "OK";

    } catch (e) {
        return "Error: " + e.toString();
    }
}

// Set custom anchor by ratio (0-1)
function setCustomAnchor(ratioX, ratioY) {
    try {
        var comp = app.project.activeItem;
        if (!comp || !(comp instanceof CompItem)) {
            return "Error: No active composition";
        }
        if (comp.selectedLayers.length === 0) {
            return "Error: No layers selected";
        }

        app.beginUndoGroup("Set Custom Anchor");

        for (var i = 0; i < comp.selectedLayers.length; i++) {
            var layer = comp.selectedLayers[i];
            setAnchorByRatio(layer, comp, ratioX, ratioY, false);
        }

        app.endUndoGroup();
        return "OK";

    } catch (e) {
        return "Error: " + e.toString();
    }
}

// Set custom anchor with mask recognition
function setCustomAnchorWithMask(ratioX, ratioY) {
    try {
        var comp = app.project.activeItem;
        if (!comp || !(comp instanceof CompItem)) {
            return "Error: No active composition";
        }
        if (comp.selectedLayers.length === 0) {
            return "Error: No layers selected";
        }

        app.beginUndoGroup("Set Custom Anchor (Mask)");

        for (var i = 0; i < comp.selectedLayers.length; i++) {
            var layer = comp.selectedLayers[i];
            setAnchorByRatio(layer, comp, ratioX, ratioY, true);
        }

        app.endUndoGroup();
        return "OK";

    } catch (e) {
        return "Error: " + e.toString();
    }
}

// Helper: Set anchor by ratio with optional mask recognition
function setAnchorByRatio(layer, comp, ratioX, ratioY, useMask) {
    var bounds;

    if (useMask) {
        bounds = getMaskBounds(layer, comp.time);
    }

    if (!bounds) {
        bounds = layer.sourceRectAtTime(comp.time, false);
    }

    if (!bounds || bounds.width <= 0 || bounds.height <= 0) {
        return;
    }

    var nx = bounds.left + bounds.width * ratioX;
    var ny = bounds.top + bounds.height * ratioY;

    applyAnchorToLayer(layer, comp, nx, ny);
}

// Helper: Get mask bounds
function getMaskBounds(layer, time) {
    try {
        var masks = layer.property("ADBE Mask Parade");
        if (!masks || masks.numProperties === 0) {
            return null;
        }

        var minX = Infinity, minY = Infinity;
        var maxX = -Infinity, maxY = -Infinity;

        for (var m = 1; m <= masks.numProperties; m++) {
            var mask = masks.property(m);
            var path = mask.property("ADBE Mask Shape").valueAtTime(time, false);
            if (!path || !path.vertices) continue;

            var verts = path.vertices;
            for (var v = 0; v < verts.length; v++) {
                var pt = verts[v];
                if (pt[0] < minX) minX = pt[0];
                if (pt[0] > maxX) maxX = pt[0];
                if (pt[1] < minY) minY = pt[1];
                if (pt[1] > maxY) maxY = pt[1];
            }
        }

        if (minX === Infinity) {
            return null;
        }

        return {
            left: minX,
            top: minY,
            width: maxX - minX,
            height: maxY - minY
        };

    } catch (e) {
        return null;
    }
}

// Helper: Apply anchor to layer with position compensation
function applyAnchorToLayer(layer, comp, newAnchorX, newAnchorY) {
    var ap = layer.property("ADBE Transform Group").property("ADBE Anchor Point");
    var pp = layer.property("ADBE Transform Group").property("ADBE Position");

    if (!ap || !pp) return;

    var oldAnchor = ap.value;
    var pos = pp.value;

    var dx = newAnchorX - oldAnchor[0];
    var dy = newAnchorY - oldAnchor[1];

    // Get scale and rotation
    var scale = layer.property("ADBE Transform Group").property("ADBE Scale").value;
    var sx = scale[0] / 100;
    var sy = scale[1] / 100;
    var rot = layer.property("ADBE Transform Group").property("ADBE Rotate Z").value * Math.PI / 180;

    // Transform delta by rotation and scale
    var rdx = (dx * Math.cos(rot) - dy * Math.sin(rot)) * sx;
    var rdy = (dx * Math.sin(rot) + dy * Math.cos(rot)) * sy;

    // Apply
    ap.setValue([newAnchorX, newAnchorY]);

    if (pos.length === 3) {
        pp.setValue([pos[0] + rdx, pos[1] + rdy, pos[2]]);
    } else {
        pp.setValue([pos[0] + rdx, pos[1] + rdy]);
    }
}

// Get anchor ratio of selected layer (for copy feature)
function getLayerAnchorRatio() {
    try {
        var comp = app.project.activeItem;
        if (!comp || !(comp instanceof CompItem)) {
            return "Error: No active composition";
        }
        if (comp.selectedLayers.length === 0) {
            return "Error: No layers selected";
        }

        var layer = comp.selectedLayers[0];
        var bounds = layer.sourceRectAtTime(comp.time, false);
        if (!bounds || bounds.width <= 0 || bounds.height <= 0) {
            return "Error: Invalid layer bounds";
        }

        var ap = layer.property("ADBE Transform Group").property("ADBE Anchor Point").value;
        var rx = (ap[0] - bounds.left) / bounds.width;
        var ry = (ap[1] - bounds.top) / bounds.height;

        return JSON.stringify({ x: rx, y: ry });

    } catch (e) {
        return "Error: " + e.toString();
    }
}

// Global cache for PlugPlugExternalObject
var _plugPlugLib = null;

/**
 * Dispatch CSXS Event to CEP panel
 * Called from C++ to notify CEP of mode toggle changes
 */
function dispatchCEPEvent(eventType, eventData) {
    try {
        // Cache the ExternalObject to avoid repeated loading issues
        if (!_plugPlugLib) {
            _plugPlugLib = new ExternalObject("lib:PlugPlugExternalObject");
        }
        if (_plugPlugLib) {
            var event = new CSXSEvent();
            event.type = eventType;
            event.data = JSON.stringify(eventData);
            event.dispatch();
            return "OK";
        }
        return "Error: PlugPlugExternalObject not available";
    } catch (e) {
        return "Error: " + e.toString();
    }
}

/**
 * Notify CEP of mode change (called by C++ plugin)
 */
function notifyModeChange(useCompMode, useMaskRecognition) {
    return dispatchCEPEvent("anchorGridModeChanged", {
        useCompMode: useCompMode,
        useMaskRecognition: useMaskRecognition
    });
}

// =========================================================================
// Control Module - Effect Controls Functions
// =========================================================================

/**
 * Open Effect Controls for the selected layer
 * Command ID 2163 = Effect Controls panel
 */
function openEffectControls() {
    try {
        var comp = app.project.activeItem;
        if (!comp || !(comp instanceof CompItem)) {
            return "Error: No active composition";
        }
        if (comp.selectedLayers.length === 0) {
            return "Error: No layers selected";
        }

        // Open Effect Controls panel
        app.executeCommand(2163);

        return "OK";
    } catch (e) {
        return "Error: " + e.toString();
    }
}

/**
 * Open a new locked Effect Controls window for selected layer
 * More reliable method: uses layer.openInViewer() to ensure correct panel
 */
function openNewLockedECWindow() {
    try {
        var comp = app.project.activeItem;
        if (!comp || !(comp instanceof CompItem)) {
            return "Error: No active composition";
        }
        if (comp.selectedLayers.length === 0) {
            return "Error: No layers selected";
        }

        var layer = comp.selectedLayers[0];

        // Method 1: Try opening layer in viewer (creates Layer panel, not EC)
        // This doesn't work for EC, so we use command approach

        // Method 2: Open EC then split
        // First, ensure EC is open
        app.executeCommand(2163);  // Open Effect Controls

        // Wait a moment for EC to become active (AE needs time)
        // Note: ExtendScript doesn't have async, so we use a sync delay
        var startTime = new Date().getTime();
        while (new Date().getTime() - startTime < 50) {
            // Busy wait ~50ms
        }

        // Now split with new locked viewer
        // 3700 = "New Viewer" / Split locked viewer command
        app.executeCommand(3700);

        return "OK";
    } catch (e) {
        return "Error: " + e.toString();
    }
}

/**
 * Get list of effects on the selected layer
 * Returns: "name|matchName|index;name|matchName|index;..."
 */
function getLayerEffects() {
    try {
        var comp = app.project.activeItem;
        if (!comp || !(comp instanceof CompItem)) {
            return "Error: No active composition";
        }
        if (comp.selectedLayers.length === 0) {
            return "Error: No layers selected";
        }

        var layer = comp.selectedLayers[0];
        var fx = layer.property("ADBE Effect Parade");

        if (!fx || fx.numProperties === 0) {
            return "";  // No effects
        }

        var result = [];
        for (var i = 1; i <= fx.numProperties; i++) {
            var effect = fx.property(i);
            result.push(effect.name + "|" + effect.matchName + "|" + (i - 1));
        }

        return result.join(";");
    } catch (e) {
        return "Error: " + e.toString();
    }
}

/**
 * Add effect to the selected layer by matchName
 */
function addEffectToLayer(matchName) {
    try {
        var comp = app.project.activeItem;
        if (!comp || !(comp instanceof CompItem)) {
            return "Error: No active composition";
        }
        if (comp.selectedLayers.length === 0) {
            return "Error: No layers selected";
        }

        app.beginUndoGroup("Add Effect");

        for (var i = 0; i < comp.selectedLayers.length; i++) {
            var layer = comp.selectedLayers[i];
            var fx = layer.property("ADBE Effect Parade");

            if (fx.canAddProperty(matchName)) {
                fx.addProperty(matchName);
            }
        }

        app.endUndoGroup();
        return "OK";
    } catch (e) {
        return "Error: " + e.toString();
    }
}

/**
 * Remove effect from the selected layer by index (0-based)
 */
function removeEffectFromLayer(index) {
    try {
        var comp = app.project.activeItem;
        if (!comp || !(comp instanceof CompItem)) {
            return "Error: No active composition";
        }
        if (comp.selectedLayers.length === 0) {
            return "Error: No layers selected";
        }

        app.beginUndoGroup("Remove Effect");

        var layer = comp.selectedLayers[0];
        var fx = layer.property("ADBE Effect Parade");

        // Convert 0-based to 1-based index
        var aeIndex = index + 1;

        if (fx && aeIndex >= 1 && aeIndex <= fx.numProperties) {
            fx.property(aeIndex).remove();
        }

        app.endUndoGroup();
        return "OK";
    } catch (e) {
        return "Error: " + e.toString();
    }
}

// =========================================================================
// Effect Expand/Collapse Functions
// =========================================================================

/**
 * Expand a specific effect and collapse all others
 * This selects the effect in Effect Controls panel
 * @param {number} effectIndex - 0-based index of the effect to expand
 */
function expandEffect(effectIndex) {
    try {
        var comp = app.project.activeItem;
        if (!comp || !(comp instanceof CompItem)) {
            return "Error: No active composition";
        }
        if (comp.selectedLayers.length === 0) {
            return "Error: No layers selected";
        }

        var layer = comp.selectedLayers[0];
        var fx = layer.property("ADBE Effect Parade");

        if (!fx || fx.numProperties === 0) {
            return "Error: No effects on layer";
        }

        // Convert 0-based to 1-based index
        var aeIndex = effectIndex + 1;

        if (aeIndex < 1 || aeIndex > fx.numProperties) {
            return "Error: Invalid effect index";
        }

        // Collapse all effects first
        for (var i = 1; i <= fx.numProperties; i++) {
            fx.property(i).selected = false;
        }

        // Expand the selected effect
        fx.property(aeIndex).selected = true;

        return "OK";
    } catch (e) {
        return "Error: " + e.toString();
    }
}

// =========================================================================
// Effect Preset Functions
// =========================================================================

/**
 * Get all properties of an effect for saving as preset
 * @param {number} effectIndex - 0-based index of the effect
 * @returns {string} JSON string of effect preset data
 */
function getEffectPresetData(effectIndex) {
    try {
        var comp = app.project.activeItem;
        if (!comp || !(comp instanceof CompItem)) {
            return "Error: No active composition";
        }
        if (comp.selectedLayers.length === 0) {
            return "Error: No layers selected";
        }

        var layer = comp.selectedLayers[0];
        var fx = layer.property("ADBE Effect Parade");

        if (!fx || fx.numProperties === 0) {
            return "Error: No effects on layer";
        }

        var aeIndex = effectIndex + 1;
        if (aeIndex < 1 || aeIndex > fx.numProperties) {
            return "Error: Invalid effect index";
        }

        var effect = fx.property(aeIndex);
        var preset = {
            matchName: effect.matchName,
            name: effect.name,
            properties: [],
            expressions: []
        };

        // Get layer name for expression conversion
        var layerName = layer.name;

        // Iterate through effect properties
        for (var i = 1; i <= effect.numProperties; i++) {
            var prop = effect.property(i);

            // Only get properties that can have values
            if (prop.propertyValueType !== PropertyValueType.NO_VALUE) {
                try {
                    var propData = {
                        index: i,
                        name: prop.name,
                        matchName: prop.matchName
                    };

                    // Get value if possible
                    if (prop.numKeys === 0) {
                        // Static value
                        propData.value = prop.value;
                    } else {
                        // Has keyframes - get value at current time
                        propData.value = prop.valueAtTime(comp.time, false);
                        propData.hasKeyframes = true;
                    }

                    // Check for expression
                    if (prop.expression && prop.expression.length > 0) {
                        var expr = prop.expression;

                        // Auto-convert self-references to thisLayer
                        // Pattern: thisComp.layer("layerName") -> thisLayer
                        var selfRefPattern = new RegExp('thisComp\\.layer\\(["\']' + escapeRegExp(layerName) + '["\']\\)', 'g');
                        expr = expr.replace(selfRefPattern, 'thisLayer');

                        preset.expressions.push({
                            index: i,
                            expression: expr,
                            enabled: prop.expressionEnabled
                        });
                    }

                    preset.properties.push(propData);
                } catch (propErr) {
                    // Skip properties that can't be read
                }
            }
        }

        return JSON.stringify(preset);
    } catch (e) {
        return "Error: " + e.toString();
    }
}

/**
 * Helper function to escape special regex characters
 */
function escapeRegExp(string) {
    return string.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}

/**
 * Apply a preset to the selected layer
 * @param {string} presetJson - JSON string of preset data
 * @returns {string} "OK" or error message
 */
function applyEffectPreset(presetJson) {
    try {
        var comp = app.project.activeItem;
        if (!comp || !(comp instanceof CompItem)) {
            return "Error: No active composition";
        }
        if (comp.selectedLayers.length === 0) {
            return "Error: No layers selected";
        }

        var preset = JSON.parse(presetJson);
        if (!preset || !preset.matchName) {
            return "Error: Invalid preset data";
        }

        app.beginUndoGroup("Apply Effect Preset");

        for (var layerIdx = 0; layerIdx < comp.selectedLayers.length; layerIdx++) {
            var layer = comp.selectedLayers[layerIdx];
            var fx = layer.property("ADBE Effect Parade");

            // Add the effect
            if (!fx.canAddProperty(preset.matchName)) {
                continue; // Skip if can't add this effect to this layer
            }

            var newEffect = fx.addProperty(preset.matchName);

            // Apply property values
            for (var i = 0; i < preset.properties.length; i++) {
                var propData = preset.properties[i];
                try {
                    var prop = newEffect.property(propData.index);
                    if (prop && prop.propertyValueType !== PropertyValueType.NO_VALUE) {
                        if (!propData.hasKeyframes) {
                            prop.setValue(propData.value);
                        }
                    }
                } catch (setPropErr) {
                    // Skip properties that can't be set
                }
            }

            // Apply expressions
            for (var j = 0; j < preset.expressions.length; j++) {
                var exprData = preset.expressions[j];
                try {
                    var prop = newEffect.property(exprData.index);
                    if (prop) {
                        prop.expression = exprData.expression;
                        prop.expressionEnabled = exprData.enabled;
                    }
                } catch (setExprErr) {
                    // Skip expressions that can't be set
                }
            }
        }

        app.endUndoGroup();
        return "OK";
    } catch (e) {
        return "Error: " + e.toString();
    }
}

/**
 * Get all effects from selected layer as preset data (for multi-effect presets)
 * @returns {string} JSON string of all effects preset data
 */
function getAllEffectsPresetData() {
    try {
        var comp = app.project.activeItem;
        if (!comp || !(comp instanceof CompItem)) {
            return "Error: No active composition";
        }
        if (comp.selectedLayers.length === 0) {
            return "Error: No layers selected";
        }

        var layer = comp.selectedLayers[0];
        var fx = layer.property("ADBE Effect Parade");

        if (!fx || fx.numProperties === 0) {
            return "Error: No effects on layer";
        }

        var allPresets = {
            effects: []
        };

        // Get preset data for each effect
        for (var i = 0; i < fx.numProperties; i++) {
            var presetJson = getEffectPresetData(i);
            if (presetJson.indexOf("Error:") !== 0) {
                allPresets.effects.push(JSON.parse(presetJson));
            }
        }

        return JSON.stringify(allPresets);
    } catch (e) {
        return "Error: " + e.toString();
    }
}

/**
 * Apply multiple effects from a preset
 * @param {string} allPresetsJson - JSON string containing array of effect presets
 * @returns {string} "OK" or error message
 */
function applyMultiEffectPreset(allPresetsJson) {
    try {
        var comp = app.project.activeItem;
        if (!comp || !(comp instanceof CompItem)) {
            return "Error: No active composition";
        }
        if (comp.selectedLayers.length === 0) {
            return "Error: No layers selected";
        }

        var allPresets = JSON.parse(allPresetsJson);
        if (!allPresets || !allPresets.effects || allPresets.effects.length === 0) {
            return "Error: Invalid preset data";
        }

        app.beginUndoGroup("Apply Multi-Effect Preset");

        // Apply each effect preset
        for (var i = 0; i < allPresets.effects.length; i++) {
            var presetJson = JSON.stringify(allPresets.effects[i]);
            applyEffectPreset(presetJson);
        }

        app.endUndoGroup();
        return "OK";
    } catch (e) {
        return "Error: " + e.toString();
    }
}
