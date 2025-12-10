/**
 * Anchor Grid - ScriptUI Version
 * Shows floating anchor grid at mouse position when triggered
 * 
 * Features:
 * - Floating palette at mouse cursor position
 * - 3x3, 5x5, 7x7 grid options
 * - Click to apply anchor point
 * - ESC to cancel
 */

(function () {
    // ========================================================================
    // Configuration
    // ========================================================================
    var config = {
        gridSize: 3,
        cellSize: 50,
        padding: 10,
        backgroundColor: [0.14, 0.14, 0.14],
        cellColor: [0.2, 0.4, 0.7],
        cellHoverColor: [0.3, 0.5, 0.8],
        borderColor: [0.4, 0.4, 0.4]
    };

    // ========================================================================
    // Main Entry Point - Called from C++ Plugin
    // ========================================================================
    function showAnchorGrid(mouseX, mouseY) {
        var win = createGridWindow(mouseX, mouseY);
        if (win) {
            win.show();
        }
    }

    // ========================================================================
    // Grid Window Creation
    // ========================================================================
    function createGridWindow(mouseX, mouseY) {
        // Calculate window size
        var winSize = config.gridSize * config.cellSize + config.padding * 2;

        // Create palette window (floating, no title bar style)
        var win = new Window('palette', 'Anchor Grid', undefined, {
            borderless: true
        });

        // Position at mouse cursor
        win.location = [mouseX - winSize / 2, mouseY - winSize / 2];

        // Style
        win.graphics.backgroundColor = win.graphics.newBrush(
            win.graphics.BrushType.SOLID_COLOR,
            config.backgroundColor
        );

        // Create grid panel
        var gridPanel = win.add('group');
        gridPanel.orientation = 'column';
        gridPanel.spacing = 4;
        gridPanel.margins = config.padding;

        // Build grid buttons
        var buttons = [];
        for (var y = 0; y < config.gridSize; y++) {
            var row = gridPanel.add('group');
            row.orientation = 'row';
            row.spacing = 4;

            for (var x = 0; x < config.gridSize; x++) {
                var btn = row.add('button', undefined, '', {
                    name: 'cell_' + x + '_' + y
                });
                btn.size = [config.cellSize, config.cellSize];
                btn.gridX = x;
                btn.gridY = y;

                // Click handler
                btn.onClick = function () {
                    applyAnchor(this.gridX, this.gridY, config.gridSize);
                    win.close();
                };

                buttons.push(btn);
            }
        }

        // ESC to close
        win.addEventListener('keydown', function (e) {
            if (e.keyName === 'Escape') {
                win.close();
            }
        });

        // Close when clicking outside
        win.addEventListener('blur', function () {
            win.close();
        });

        return win;
    }

    // ========================================================================
    // Anchor Point Application
    // ========================================================================
    function applyAnchor(gridX, gridY, gridSize) {
        var comp = app.project.activeItem;

        if (!comp || !(comp instanceof CompItem)) {
            alert('Error: No active composition');
            return;
        }

        if (comp.selectedLayers.length === 0) {
            alert('Error: No layer selected');
            return;
        }

        app.beginUndoGroup('Set Anchor Point');

        for (var i = 0; i < comp.selectedLayers.length; i++) {
            var layer = comp.selectedLayers[i];
            setAnchorForLayer(layer, gridX, gridY, gridSize, comp.time);
        }

        app.endUndoGroup();
    }

    function setAnchorForLayer(layer, gridX, gridY, gridSize, time) {
        try {
            // Get layer bounds at current time
            var bounds = layer.sourceRectAtTime(time, false);

            // Calculate percentage based on grid position
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

        } catch (e) {
            $.writeln('Error setting anchor: ' + e.toString());
        }
    }

    // ========================================================================
    // Grid Size Configuration
    // ========================================================================
    function setGridSize(size) {
        if (size === 3 || size === 5 || size === 7) {
            config.gridSize = size;
        }
    }

    // ========================================================================
    // Export to global scope for C++ plugin access
    // ========================================================================
    $.global.showAnchorGrid = showAnchorGrid;
    $.global.setAnchorGridSize = setGridSize;

})();
