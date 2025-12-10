/**
 * Anchor Grid - ExtendScript
 * Handles actual anchor point manipulation in After Effects
 */

/**
 * Set anchor point of the selected layer based on grid position
 * @param {number} gridX - Grid X coordinate (0 to gridSize-1)
 * @param {number} gridY - Grid Y coordinate (0 to gridSize-1)
 * @param {number} gridSize - Size of the grid (3, 5, or 7)
 * @returns {string} Result message
 */
function setLayerAnchor(gridX, gridY, gridSize) {
    try {
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
            var result = setAnchorForLayer(layer, gridX, gridY, gridSize, comp.time);
            results.push(result);
        }

        app.endUndoGroup();

        return results.join(', ');

    } catch (e) {
        return 'Error: ' + e.toString();
    }
}

/**
 * Set anchor point for a single layer
 */
function setAnchorForLayer(layer, gridX, gridY, gridSize, time) {
    try {
        // Get layer bounds at current time
        var bounds = layer.sourceRectAtTime(time, false);

        // Calculate percentage based on grid position
        // gridX=0 means left (0%), gridX=gridSize-1 means right (100%)
        var percentX = gridX / (gridSize - 1);
        var percentY = gridY / (gridSize - 1);

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
