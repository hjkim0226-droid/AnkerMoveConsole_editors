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
 * Open Effect Controls for selected layer
 * Uses 3734 (OpenEffectControls) - opens EC without toggle behavior
 */
function openEffectControlsForLayer() {
    try {
        var comp = app.project.activeItem;
        if (!comp || !(comp instanceof CompItem)) {
            return "Error: No active composition";
        }
        if (comp.selectedLayers.length === 0) {
            return "Error: No layers selected";
        }

        // 3734: Open Effect Controls (not toggle, adds to EC history)
        app.executeCommand(3734);

        return "OK";
    } catch (e) {
        return "Error: " + e.toString();
    }
}

/**
 * Get all available effects from After Effects
 * Returns localized names (Korean in Korean AE, English in English AE)
 * Format: "displayName|matchName|category;displayName|matchName|category;..."
 */
function getAllEffects() {
    try {
        var result = [];
        for (var i = 0; i < app.effects.length; i++) {
            var eff = app.effects[i];
            // Skip synthetic/internal effects with empty category
            if (eff.category === "") continue;
            result.push(eff.displayName + "|" + eff.matchName + "|" + eff.category);
        }
        return result.join(";");
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

// =========================================================================
// Keyframe Module - Keyframe Easing Functions
// =========================================================================

/**
 * Get information about selected keyframes for easing
 * Returns JSON with property info and keyframe pairs
 */
function getSelectedKeyframeInfo() {
    try {
        var comp = app.project.activeItem;
        if (!comp || !(comp instanceof CompItem)) {
            return JSON.stringify({error: "No active composition"});
        }

        var props = comp.selectedProperties;
        if (!props || props.length === 0) {
            return JSON.stringify({error: "No properties selected"});
        }

        var result = [];

        for (var i = 0; i < props.length; i++) {
            var prop = props[i];

            // Check if property has keyframes and at least 2 are selected
            if (!prop.selectedKeys || prop.selectedKeys.length < 2) continue;

            var keys = prop.selectedKeys;
            for (var j = 0; j < keys.length - 1; j++) {
                var k1 = keys[j];
                var k2 = keys[j + 1];

                // Get interpolation types for keyframes
                // KeyframeInterpolationType: LINEAR=6612, BEZIER=6613, HOLD=6614
                var outInterp = prop.keyOutInterpolationType(k1);  // How k1 leaves
                var inInterp = prop.keyInInterpolationType(k2);    // How k2 enters

                // Determine if linear (6612) or hold (6614)
                var isLinearOut = (outInterp === KeyframeInterpolationType.LINEAR);
                var isLinearIn = (inInterp === KeyframeInterpolationType.LINEAR);
                var isHoldOut = (outInterp === KeyframeInterpolationType.HOLD);
                var isHoldIn = (inInterp === KeyframeInterpolationType.HOLD);

                // Get current easing info
                var outEase = prop.keyOutTemporalEase(k1);  // Ease leaving k1
                var inEase = prop.keyInTemporalEase(k2);    // Ease entering k2

                // Calculate average speed for normalization
                var t1 = prop.keyTime(k1);
                var t2 = prop.keyTime(k2);
                var v1 = prop.keyValue(k1);
                var v2 = prop.keyValue(k2);
                var duration = t2 - t1;

                // Calculate value change (handle multi-dimensional properties)
                var valueChange = 0;
                if (typeof v1 === "number" && typeof v2 === "number") {
                    // 1D property (Opacity, Rotation, etc.)
                    valueChange = Math.abs(v2 - v1);
                } else if (v1 instanceof Array && v2 instanceof Array) {
                    // Multi-dimensional (Position, Scale, etc.)
                    // Use magnitude of change vector
                    var sumSq = 0;
                    for (var d = 0; d < v1.length; d++) {
                        var delta = v2[d] - v1[d];
                        sumSq += delta * delta;
                    }
                    valueChange = Math.sqrt(sumSq);
                }

                var avgSpeed = (duration > 0.0001) ? (valueChange / duration) : 0;

                // For linear/hold keyframes, normalize speed values
                var outSpeed = outEase[0].speed;
                var outInfluence = outEase[0].influence;
                var inSpeed = inEase[0].speed;
                var inInfluence = inEase[0].influence;

                // Linear keyframes: speed should equal avgSpeed for diagonal line
                if (isLinearOut && avgSpeed > 0) {
                    outSpeed = avgSpeed;
                    outInfluence = 33.33;
                }
                if (isLinearIn && avgSpeed > 0) {
                    inSpeed = avgSpeed;
                    inInfluence = 33.33;
                }

                // Hold keyframes: speed = 0, special handling
                if (isHoldOut) {
                    outSpeed = 0;
                    outInfluence = 100;
                }
                if (isHoldIn) {
                    inSpeed = 0;
                    inInfluence = 100;
                }

                result.push({
                    propName: prop.name,
                    propMatchName: prop.matchName,
                    keyIndex1: k1,
                    keyIndex2: k2,
                    time1: t1,
                    time2: t2,
                    value1: (typeof v1 === "number") ? v1 : v1[0],  // First value for display
                    value2: (typeof v2 === "number") ? v2 : v2[0],
                    avgSpeed: avgSpeed,  // Calculated average speed
                    // Keyframe types (1=linear, 2=bezier, 3=hold)
                    outType: isHoldOut ? 3 : (isLinearOut ? 1 : 2),
                    inType: isHoldIn ? 3 : (isLinearIn ? 1 : 2),
                    // Flat structure for C++ simple JSON parser
                    outSpeed: outSpeed,
                    outInfluence: outInfluence,
                    inSpeed: inSpeed,
                    inInfluence: inInfluence
                });
            }
        }

        return JSON.stringify(result);
    } catch (e) {
        return JSON.stringify({error: e.toString()});
    }
}

/**
 * Apply keyframe easing to selected keyframes
 * @param {object} data - Object with outSpeed, outInfluence, inSpeed, inInfluence
 * @returns {string} "OK" or error message
 */
function applyKeyframeEasing(data) {
    try {
        var comp = app.project.activeItem;
        if (!comp || !(comp instanceof CompItem)) {
            return "Error: No active composition";
        }

        var props = comp.selectedProperties;
        if (!props || props.length === 0) {
            return "Error: No properties selected";
        }

        app.beginUndoGroup("Apply Keyframe Easing");

        for (var i = 0; i < props.length; i++) {
            var prop = props[i];

            // Check if property has keyframes and at least 2 are selected
            if (!prop.selectedKeys || prop.selectedKeys.length < 2) continue;

            var keys = prop.selectedKeys;

            // Get number of dimensions for this property
            var numDims = 1;
            if (prop.propertyValueType === PropertyValueType.TwoD ||
                prop.propertyValueType === PropertyValueType.TwoD_SPATIAL) {
                numDims = 2;
            } else if (prop.propertyValueType === PropertyValueType.ThreeD ||
                       prop.propertyValueType === PropertyValueType.ThreeD_SPATIAL) {
                numDims = 3;
            } else if (prop.propertyValueType === PropertyValueType.COLOR) {
                numDims = 4;
            }

            // Create ease objects for each dimension
            var outEaseArr = [];
            var inEaseArr = [];
            for (var d = 0; d < numDims; d++) {
                outEaseArr.push(new KeyframeEase(data.outSpeed, data.outInfluence));
                inEaseArr.push(new KeyframeEase(data.inSpeed, data.inInfluence));
            }

            // Apply to each keyframe pair
            for (var j = 0; j < keys.length; j++) {
                var k = keys[j];

                // Apply out ease to this keyframe (affects transition to next key)
                if (j < keys.length - 1) {
                    try {
                        prop.setTemporalEaseAtKey(k, prop.keyInTemporalEase(k), outEaseArr);
                    } catch (e1) {
                        // Some properties don't support temporal ease
                    }
                }

                // Apply in ease to this keyframe (affects transition from previous key)
                if (j > 0) {
                    try {
                        prop.setTemporalEaseAtKey(k, inEaseArr, prop.keyOutTemporalEase(k));
                    } catch (e2) {
                        // Some properties don't support temporal ease
                    }
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
 * Get velocity at a specific time for a property
 * Uses numerical differentiation
 * @param {Property} prop - The property to measure
 * @param {number} time - Time in seconds
 * @returns {number} Velocity (rate of change)
 */
function calculateVelocity(prop, time) {
    try {
        var dt = 0.001;  // Small time delta
        var v1 = prop.valueAtTime(time - dt, false);
        var v2 = prop.valueAtTime(time + dt, false);

        // Handle scalar vs array values
        if (typeof v1 === "number") {
            return (v2 - v1) / (2 * dt);
        } else if (v1 instanceof Array) {
            // Calculate magnitude of velocity vector
            var sum = 0;
            for (var i = 0; i < v1.length; i++) {
                sum += Math.pow(v2[i] - v1[i], 2);
            }
            return Math.sqrt(sum) / (2 * dt);
        }
        return 0;
    } catch (e) {
        return 0;
    }
}

/**
 * Apply preset easing curve (like Flow plugin)
 * @param {string} presetName - Name of preset ("linear", "easeIn", "easeOut", "easeInOut", "easeOutIn")
 * @returns {string} "OK" or error message
 */
function applyEasingPreset(presetName) {
    try {
        var presets = {
            "linear": {outSpeed: 0, outInfluence: 33.33, inSpeed: 0, inInfluence: 33.33},
            "easeIn": {outSpeed: 0, outInfluence: 100, inSpeed: 0, inInfluence: 100},
            "easeOut": {outSpeed: 0, outInfluence: 0.1, inSpeed: 0, inInfluence: 100},
            "easeInOut": {outSpeed: 0, outInfluence: 100, inSpeed: 0, inInfluence: 100},
            "easeOutIn": {outSpeed: 0, outInfluence: 0.1, inSpeed: 0, inInfluence: 0.1}
        };

        var preset = presets[presetName];
        if (!preset) {
            return "Error: Unknown preset '" + presetName + "'";
        }

        return applyKeyframeEasing(preset);
    } catch (e) {
        return "Error: " + e.toString();
    }
}
