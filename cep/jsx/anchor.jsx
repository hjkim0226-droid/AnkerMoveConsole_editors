/**
 * Anchor Grid - ExtendScript
 * Handles actual anchor point manipulation in After Effects
 */

/**
 * Set anchor point of the selected layer based on grid position
 * @param {number} gridX - Grid X coordinate (0 to gridWidth-1)
 * @param {number} gridY - Grid Y coordinate (0 to gridHeight-1)
 * @param {number} gridWidth - Width of the grid (3-7)
 * @param {number} gridHeight - Height of the grid (3-7)
 * @param {boolean} maskEnabled - Whether to consider mask bounds
 * @returns {string} Result message
 */
function setLayerAnchor(gridX, gridY, gridWidth, gridHeight, maskEnabled) {
    try {
        $.writeln("setLayerAnchor: gridX=" + gridX + ", gridY=" + gridY + ", gridW=" + gridWidth + ", gridH=" + gridHeight);

        var comp = app.project.activeItem;

        if (!comp || !(comp instanceof CompItem)) {
            return 'Error: No active composition';
        }

        if (comp.selectedLayers.length === 0) {
            return 'Error: No layer selected';
        }

        app.beginUndoGroup('Set Anchor Point');

        var results = [];

        for (var i = 0; i < comp.selectedLayers.length; i++) {
            var layer = comp.selectedLayers[i];
            var percentX = gridX / (gridWidth - 1);
            var percentY = gridY / (gridHeight - 1);
            var result = setAnchorByRatio(layer, percentX, percentY, comp.time, maskEnabled);
            results.push(result);
        }

        app.endUndoGroup();

        return results.join(', ');

    } catch (e) {
        return 'Error: ' + e.toString();
    }
}

/**
 * Set custom anchor point by ratio (0-1)
 * @param {number} ratioX - X ratio (0-1)
 * @param {number} ratioY - Y ratio (0-1)
 */
function setLayerCustomAnchor(ratioX, ratioY) {
    try {
        var comp = app.project.activeItem;

        if (!comp || !(comp instanceof CompItem)) {
            return 'Error: No active composition';
        }

        if (comp.selectedLayers.length === 0) {
            return 'Error: No layer selected';
        }

        app.beginUndoGroup('Set Custom Anchor');

        var results = [];

        for (var i = 0; i < comp.selectedLayers.length; i++) {
            var layer = comp.selectedLayers[i];
            var result = setAnchorByRatio(layer, ratioX, ratioY, comp.time, true);
            results.push(result);
        }

        app.endUndoGroup();

        return results.join(', ');

    } catch (e) {
        return 'Error: ' + e.toString();
    }
}

/**
 * Set anchor for composition (applies to all layers proportionally)
 */
function setCompositionAnchor(gridX, gridY, gridWidth, gridHeight) {
    try {
        var comp = app.project.activeItem;

        if (!comp || !(comp instanceof CompItem)) {
            return 'Error: No active composition';
        }

        var percentX = gridX / (gridWidth - 1);
        var percentY = gridY / (gridHeight - 1);

        return setCompositionCustomAnchor(percentX, percentY);

    } catch (e) {
        return 'Error: ' + e.toString();
    }
}

/**
 * Set composition anchor by ratio
 */
function setCompositionCustomAnchor(ratioX, ratioY) {
    try {
        var comp = app.project.activeItem;

        if (!comp || !(comp instanceof CompItem)) {
            return 'Error: No active composition';
        }

        app.beginUndoGroup('Set Composition Anchor');

        var results = [];

        // Apply to all layers in composition
        for (var i = 1; i <= comp.numLayers; i++) {
            var layer = comp.layer(i);
            var result = setAnchorByRatio(layer, ratioX, ratioY, comp.time, false);
            results.push(result);
        }

        app.endUndoGroup();

        return 'Applied to ' + comp.numLayers + ' layers';

    } catch (e) {
        return 'Error: ' + e.toString();
    }
}

/**
 * Set anchor point for a single layer by ratio
 */
function setAnchorByRatio(layer, percentX, percentY, time, useMask) {
    try {
        // Get layer bounds at current time
        var bounds = layer.sourceRectAtTime(time, false);

        // Check for mask bounds if enabled
        if (useMask && layer.property("ADBE Mask Parade") && layer.property("ADBE Mask Parade").numProperties > 0) {
            var maskBounds = getMaskBounds(layer, time);
            if (maskBounds) {
                bounds = maskBounds;
            }
        }

        // Calculate new anchor point in layer's local coordinates
        var newAnchorX = bounds.left + (bounds.width * percentX);
        var newAnchorY = bounds.top + (bounds.height * percentY);

        // Get current anchor and position
        var anchorProp = layer.property('ADBE Transform Group').property('ADBE Anchor Point');
        var positionProp = layer.property('ADBE Transform Group').property('ADBE Position');

        var oldAnchor = anchorProp.value;
        var position = positionProp.value;

        // Calculate position offset to maintain visual position
        var deltaX = newAnchorX - oldAnchor[0];
        var deltaY = newAnchorY - oldAnchor[1];

        // Apply scale and rotation to delta
        var scaleProp = layer.property('ADBE Transform Group').property('ADBE Scale');
        var scale = scaleProp.value;
        var scaleX = scale[0] / 100;
        var scaleY = scale[1] / 100;

        var rotationProp = layer.property('ADBE Transform Group').property('ADBE Rotate Z');
        var rotation = rotationProp.value * Math.PI / 180;

        // Rotate and scale the delta
        var rotatedDeltaX = (deltaX * Math.cos(rotation) - deltaY * Math.sin(rotation)) * scaleX;
        var rotatedDeltaY = (deltaX * Math.sin(rotation) + deltaY * Math.cos(rotation)) * scaleY;

        // Set new anchor point
        anchorProp.setValue([newAnchorX, newAnchorY]);

        // Adjust position to compensate
        if (position.length === 3) {
            positionProp.setValue([position[0] + rotatedDeltaX, position[1] + rotatedDeltaY, position[2]]);
        } else {
            positionProp.setValue([position[0] + rotatedDeltaX, position[1] + rotatedDeltaY]);
        }

        return layer.name + ': OK';

    } catch (e) {
        return layer.name + ': ' + e.toString();
    }
}

/**
 * Get bounding box of the first mask on a layer
 */
function getMaskBounds(layer, time) {
    try {
        var maskGroup = layer.property("ADBE Mask Parade");
        if (!maskGroup || maskGroup.numProperties === 0) {
            return null;
        }

        var mask = maskGroup.property(1);
        var maskPath = mask.property("ADBE Mask Shape").valueAtTime(time, false);
        var vertices = maskPath.vertices;

        if (vertices.length === 0) {
            return null;
        }

        var minX = vertices[0][0];
        var maxX = vertices[0][0];
        var minY = vertices[0][1];
        var maxY = vertices[0][1];

        for (var i = 1; i < vertices.length; i++) {
            var x = vertices[i][0];
            var y = vertices[i][1];
            if (x < minX) minX = x;
            if (x > maxX) maxX = x;
            if (y < minY) minY = y;
            if (y > maxY) maxY = y;
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

/**
 * Get anchor ratio of currently selected layer
 * Returns JSON with x, y ratios (0-1)
 */
function getLayerAnchorRatio() {
    try {
        var comp = app.project.activeItem;

        if (!comp || !(comp instanceof CompItem)) {
            return 'null';
        }

        if (comp.selectedLayers.length === 0) {
            return 'null';
        }

        var layer = comp.selectedLayers[0];
        var bounds = layer.sourceRectAtTime(comp.time, false);
        var anchor = layer.property('ADBE Transform Group').property('ADBE Anchor Point').value;

        // Calculate ratios
        var ratioX = (anchor[0] - bounds.left) / bounds.width;
        var ratioY = (anchor[1] - bounds.top) / bounds.height;

        // Clamp to 0-1 range
        ratioX = Math.max(0, Math.min(1, ratioX));
        ratioY = Math.max(0, Math.min(1, ratioY));

        return JSON.stringify({ x: ratioX, y: ratioY });

    } catch (e) {
        return 'null';
    }
}

/**
 * Get info about currently selected layers (for debugging)
 */
function getSelectedLayerInfo() {
    try {
        var comp = app.project.activeItem;

        if (!comp || !(comp instanceof CompItem)) {
            return 'No active composition';
        }

        if (comp.selectedLayers.length === 0) {
            return 'No layer selected';
        }

        var info = [];
        for (var i = 0; i < comp.selectedLayers.length; i++) {
            var layer = comp.selectedLayers[i];
            var anchor = layer.property('ADBE Transform Group').property('ADBE Anchor Point').value;
            var bounds = layer.sourceRectAtTime(comp.time, false);

            info.push({
                name: layer.name,
                anchor: anchor,
                bounds: bounds
            });
        }

        return JSON.stringify(info);

    } catch (e) {
        return 'Error: ' + e.toString();
    }
}
