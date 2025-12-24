/**
 * tabs.js - Tab and Accordion navigation
 */

(function() {
    'use strict';

    // Tab switching
    function initTabs() {
        const tabBtns = document.querySelectorAll('.tab-btn');
        const tabContents = document.querySelectorAll('.tab-content');

        tabBtns.forEach(btn => {
            btn.addEventListener('click', () => {
                const targetTab = btn.dataset.tab;

                // Update buttons
                tabBtns.forEach(b => b.classList.remove('active'));
                btn.classList.add('active');

                // Update content
                tabContents.forEach(content => {
                    content.classList.remove('active');
                    if (content.id === 'tab-' + targetTab) {
                        content.classList.add('active');
                    }
                });

                // Save last active tab
                try {
                    localStorage.setItem('anchorsnap-active-tab', targetTab);
                } catch (e) {}
            });
        });

        // Restore last active tab
        try {
            const lastTab = localStorage.getItem('anchorsnap-active-tab');
            if (lastTab) {
                const btn = document.querySelector(`.tab-btn[data-tab="${lastTab}"]`);
                if (btn) btn.click();
            }
        } catch (e) {}
    }

    // Accordion toggle
    function initAccordion() {
        const accordionHeaders = document.querySelectorAll('.accordion-header');

        accordionHeaders.forEach(header => {
            header.addEventListener('click', () => {
                const item = header.parentElement;
                const isOpen = item.classList.contains('open');

                // Close all accordions in the same group (optional - for single open)
                // const siblings = item.parentElement.querySelectorAll('.accordion-item');
                // siblings.forEach(s => s.classList.remove('open'));

                // Toggle current
                if (isOpen) {
                    item.classList.remove('open');
                } else {
                    item.classList.add('open');
                }
            });
        });
    }

    // Initialize on DOM ready
    document.addEventListener('DOMContentLoaded', () => {
        initTabs();
        initAccordion();
    });

    // Export for external use
    window.TabsModule = {
        switchTo: function(tabName) {
            const btn = document.querySelector(`.tab-btn[data-tab="${tabName}"]`);
            if (btn) btn.click();
        },
        openAccordion: function(moduleName) {
            const item = document.querySelector(`.accordion-item[data-module="${moduleName}"]`);
            if (item) item.classList.add('open');
        },
        closeAccordion: function(moduleName) {
            const item = document.querySelector(`.accordion-item[data-module="${moduleName}"]`);
            if (item) item.classList.remove('open');
        }
    };
})();
