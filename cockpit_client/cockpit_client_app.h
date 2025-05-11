#ifndef COCKPIT_CLIENT_APP_H
#define COCKPIT_CLIENT_APP_H

#include <atomic>    // For std::atomic
#include <csignal>   // For signal handling
#include <iostream>  // Temporarily for debug prints
#include <memory>    // For unique_ptr, shared_ptr
#include <string>
#include <thread>  // For running event loop in a thread (if needed)
#include <vector>

// Include configuration
#include "config/cockpit_config.h"  // Configuration structure

// Include component interfaces with their namespaces
#include "drivers/input_device_source.h"  // autodev::remote::drivers::IInputDeviceSource
#include "drivers/telemetry_handler.h"  // autodev::remote::drivers::ITelemetryHandler
#include "drivers/web_command_handler.h"  // autodev::remote::drivers::IWebCommandHandler
#include "network_manager/connection_monitor.h"  // autodev::remote::network_manager::IConnectionMonitor (Optional)
#include "transport/transport_server.h"  // autodev::remote::transport::ITransportServer
#include "webrtc/webrtc_manager.h"  // autodev::remote::webrtc::WebrtcManager

// Include Protobuf messages used directly or indirectly
// Ensure these paths are correct for your build system
#include "chassis/proto/chassis.pb.h"  // For incoming telemetry (used by TelemetryHandler)
// No need to include command protobufs here, Handlers/Sources handle them
// internally

// Forward declarations for event loop (if using Asio)
// namespace boost { namespace asio { class io_context; }}

// Define WebSocketConnectionId if not defined in transport/transport_server.h
// using WebSocketConnectionId = uint32_t; // Example: Must match definition in
// transport/transport_server.h

namespace autodev {
namespace remote {
namespace cockpit {

// Define application states
enum class AppState {
  Uninitialized,
  Initializing,
  Initialized,
  Running,
  Stopping,
  Stopped
};

// Main application class for the Cockpit client.
// Orchestrates communication between local UI (via TransportServer),
// Input Devices (via InputDeviceSource), and remote Vehicle (via
// WebrtcManager).
class CockpitClientApp {
 public:
  // Constructor - lightweight, does not perform complex initialization.
  CockpitClientApp();

  // Destructor - ensures graceful stopping and resource cleanup.
  virtual ~CockpitClientApp();

  // Initializes the application with loaded configuration and component
  // dependencies. This method sets up the application's state and internal
  // connections between components. Returns true on success, false on failure.
  bool init(
      const CockpitConfig& config,
      std::shared_ptr<autodev::remote::webrtc::WebrtcManager> webrtcManager,
      std::shared_ptr<autodev::remote::transport::ITransportServer>
          transportServer,
      std::unique_ptr<autodev::remote::drivers::IWebCommandHandler>
          webCommandHandler,  // Handles commands from Web UI
      std::unique_ptr<autodev::remote::drivers::IInputDeviceSource>
          inputDeviceSource,  // Handles commands from Input Devices
      std::unique_ptr<autodev::remote::drivers::ITelemetryHandler>
          telemetryHandler,
      std::unique_ptr<autodev::remote::network_manager::IConnectionMonitor>
          connectionMonitor = nullptr
      // TODO: Pass event loop context shared_ptr here if managed externally
      // e.g., std::shared_ptr<boost::asio::io_context> ioContext
  );

  // Runs the main application loop.
  // This method typically starts the underlying event loop and blocks
  // until the application is stopped.
  // Returns 0 on graceful exit, non-zero on error.
  int run();

  // Stops the application gracefully.
  // Signals the event loop and components to shut down. Safe to call multiple
  // times.
  void stop();

  // Allow signal handler access to stop (using a global instance pattern).
  // Note: Integrating signal handling with the event loop is generally
  // preferred.
  friend void ::signal_handler(
      int signal);  // Assumes signal_handler is in global namespace

 private:
  // Application State
  std::atomic<AppState> state_{
      AppState::Uninitialized};  // Use atomic for thread-safe state checks

  // Configuration
  CockpitConfig config_;

  // Core components - owned by the app (some shared due to dependencies)
  std::shared_ptr<autodev::remote::webrtc::WebrtcManager>
      webrtcManager_;  // Shared because Handlers/Sources use it
  std::shared_ptr<autodev::remote::transport::ITransportServer>
      transportServer_;  // Shared because TelemetryHandler uses it

  // Handlers/Sources based on input type
  std::unique_ptr<autodev::remote::drivers::IWebCommandHandler>
      webCommandHandler_;  // Processes raw commands from WebSocket
  std::unique_ptr<autodev::remote::drivers::IInputDeviceSource>
      inputDeviceSource_;  // Reads/sends commands from physical devices
  std::unique_ptr<autodev::remote::drivers::ITelemetryHandler>
      telemetryHandler_;  // Processes telemetry from vehicle

  std::unique_ptr<autodev::remote::network_manager::IConnectionMonitor>
      connectionMonitor_;  // Optional

  // Event loop context (e.g., boost::asio::io_context) - owned by the app or
  // shared All async operations (WebRTC, WebSocket server, timers) should use
  // this context. std::shared_ptr<boost::asio::io_context> ioContext_; // Use
  // shared_ptr if passed in init std::unique_ptr<std::thread> ioThread_; // If
  // running io_context in a separate thread

  // --- Internal Setup Methods ---
  // Removed setupConfig as config is injected
  bool setupWebrtcManagerCallbacks();    // Set application's handlers on the
                                         // WebRTC manager
  bool setupTransportServerCallbacks();  // Set application's handlers on the
                                         // Transport Server
  bool setupCommandAndInputHandlers();   // Initialize command & input device
                                         // handlers with dependencies
  bool
  setupTelemetryHandler();  // Initialize telemetry handler with dependencies
  bool setupConnectionMonitorCallbacks();  // Set application's handlers on the
                                           // Connection Monitor (if present)

  // --- Handlers for WebrtcManager Events (Called by WebrtcManager threads) ---
  // These methods are called from WebRTC internal threads; MUST be thread-safe.
  void handlePeerConnected(const std::string& peer_id);
  void handlePeerDisconnected(const std::string& peer_id,
                              const std::string& reason);
  void handleWebrtcDataChannelMessageReceived(
      const std::string& peer_id, const std::string& label,
      const std::vector<char>&
          message);  // Deserializes and routes to TelemetryHandler
  void handleWebrtcError(const std::string& error_msg);
  // TODO: Add handleWebrtcVideoTrackReceived(const std::string& peer_id,
  // rtc::scoped_refptr<webrtc::VideoTrackInterface> track); // If C++ handles
  // video, routes to TelemetryHandler or VideoHandler

  // --- Handlers for TransportServer (WebSocket) Events (Called by
  // TransportServer threads) --- These methods are called from TransportServer
  // internal threads; MUST be thread-safe.
  void handleWsConnected(
      WebSocketConnectionId
          conn_id);  // Notifies TelemetryHandler or manages client list
  void handleWsDisconnected(
      WebSocketConnectionId
          conn_id);  // Notifies TelemetryHandler or manages client list
  void handleWsMessageReceived(
      WebSocketConnectionId conn_id,
      const std::vector<char>&
          message);  // Routes raw message to WebCommandHandler
  void handleTransportServerError(
      const std::string& error_msg);  // Logs or triggers application shutdown

  // --- Handlers for ConnectionMonitor Events (Optional, Called by
  // ConnectionMonitor thread) --- These methods are called from
  // ConnectionMonitor internal thread; MUST be thread-safe.
  void handleNetworkUp(
      const std::string& peer_id);  // Updates UI status via TelemetryHandler
  void handleNetworkDown(
      const std::string& peer_id,
      const std::string&
          reason);  // Updates UI status, potentially disables controls
  void handleHeartbeatLost(
      const std::string&
          peer_id);  // Updates UI status, potentially disables controls

  // Prevent copying
  CockpitClientApp(const CockpitClientApp&) = delete;
  CockpitClientApp& operator=(const CockpitClientApp&) = delete;
};

}  // namespace cockpit
}  // namespace remote
}  // namespace autodev

#endif  // COCKPIT_CLIENT_APP_H
