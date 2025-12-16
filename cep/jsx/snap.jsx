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
