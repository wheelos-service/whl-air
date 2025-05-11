// display/public/js/status_panel.js

/**
 * Manages the display of vehicle status information.
 */
class StatusPanel {
  constructor(panelId = 'statusPanel', telemetryStatusId = 'telemetryStatus') {
      this.panelElement = document.getElementById(panelId);
      this.telemetryStatusElement = document.getElementById(telemetryStatusId);
      if (!this.panelElement || !this.telemetryStatusElement) {
          console.error("StatusPanel: Required DOM elements not found!");
      } else {
          console.log("StatusPanel initialized.");
      }
  }

  /**
   * Updates the status panel with new telemetry data.
   * @param {object} telemetryData - An object containing vehicle status fields (e.g., { speed_mps: 5.2, gear: 1 }).
   * Assumes JSON data structure from C++ backend.
   */
  updateStatus(telemetryData) {
      // Find the specific span elements by their IDs within the panel
      const speedSpan = this.panelElement ? this.panelElement.querySelector('#speed') : null;
      const gearSpan = this.panelElement ? this.panelElement.querySelector('#gear') : null;
      // Add references for other telemetry fields here

      if (speedSpan && telemetryData.hasOwnProperty('speed_mps')) {
          speedSpan.textContent = telemetryData.speed_mps.toFixed(1); // Display speed with one decimal place
      }
      if (gearSpan && telemetryData.hasOwnProperty('gear')) {
          // Map gear integer to string if necessary (matches protobuf enum)
          const gearMap = { 0: 'Neutral', 1: 'Drive', 2: 'Reverse', 3: 'Park' };
          gearSpan.textContent = gearMap[telemetryData.gear] || 'Unknown';
      }
      // Update other elements similarly

      this.setTelemetryStatus('Connected'); // Assuming receiving data means connected
  }

  /**
   * Sets the connection status text.
   * @param {string} statusText - The status text to display (e.g., "Connected", "Disconnected", "Receiving").
   */
  setTelemetryStatus(statusText) {
      if (this.telemetryStatusElement) {
          this.telemetryStatusElement.textContent = statusText;
          // Optionally change color based on statusText
          // this.telemetryStatusElement.style.color = (statusText === 'Connected') ? 'green' : 'red';
      }
  }

  /**
   * Clears the displayed status fields.
   */
  clearStatus() {
       const speedSpan = this.panelElement ? this.panelElement.querySelector('#speed') : null;
       const gearSpan = this.panelElement ? this.panelElement.querySelector('#gear') : null;

       if (speedSpan) speedSpan.textContent = '--';
       if (gearSpan) gearSpan.textContent = '--';
       // Clear other elements

      this.setTelemetryStatus('Disconnected');
  }
}

// Export the class
// In a real module system (like Webpack), use: export { StatusPanel };
// For simple script tags, attach to window or a global namespace:
// window.StatusPanel = StatusPanel;
