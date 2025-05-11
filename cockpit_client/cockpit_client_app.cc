#include "cockpit_client_app.h"

// Include concrete implementations (creation moved to main)
#include "config/json_config_loader.h"
#include "drivers/input_device_source_impl.h"
#include "drivers/telemetry_handler_impl.h"
#include "drivers/web_command_handler_impl.h"
#include "network_manager/connection_monitor_impl.h"
#include "transport/websocket_transport_server.h"
#include "webrtc/webrtc_manager_impl.h"

// Include Protobuf messages that need deserialization in the app
#include <atomic>
#include <chrono>
#include <csignal>
#include <functional>
#include <iostream>
#include <memory>
#include <thread>

#include "chassis/proto/chassis.pb.h"  // For handleWebrtcDataChannelMessageReceived

// Include event loop library (e.g., Asio)
// #include <boost/asio.hpp>

// Global application instance pointer for signal handling (see comments in .h)
// This is a common pattern for signal handlers, but has limitations.
// Define the global pointer in the global namespace, outside the autodev
// namespace.
CockpitClientApp* global_app_instance =
    nullptr;  // Use forward declaration in .h

// Signal handler function (defined outside namespace if used with global
// pointer)
void signal_handler(int signal) {
  std::cout << "\nReceived signal " << signal << ". Stopping application..."
            << std::endl;
  // Safely check the global instance pointer before calling stop()
  if (global_app_instance) {
    global_app_instance->stop();
  }
}

namespace autodev {
namespace remote {
namespace cockpit {

// --- Constructor and Destructor ---

CockpitClientApp::CockpitClientApp() : state_(AppState::Uninitialized) {
  std::cout << "CockpitClientApp created." << std::endl;
  // TODO: Initialize event loop context if owned by the app
  // ioContext_ = std::make_unique<boost::asio::io_context>();
}

CockpitClientApp::~CockpitClientApp() {
  std::cout << "CockpitClientApp destroying..." << std::endl;
  // Ensure stop is called to clean up components and event loop
  stop();
  // Components will be destroyed automatically by unique_ptr/shared_ptr members
  std::cout << "CockpitClientApp destroyed." << std::endl;
}

// --- Initialization ---

bool CockpitClientApp::init(
    const CockpitConfig& config,
    std::shared_ptr<autodev::remote::webrtc::WebrtcManager> webrtcManager,
    std::shared_ptr<autodev::remote::transport::ITransportServer>
        transportServer,
    std::unique_ptr<autodev::remote::drivers::IWebCommandHandler>
        webCommandHandler,
    std::unique_ptr<autodev::remote::drivers::IInputDeviceSource>
        inputDeviceSource,
    std::unique_ptr<autodev::remote::drivers::ITelemetryHandler>
        telemetryHandler,
    std::unique_ptr<autodev::remote::network_manager::IConnectionMonitor>
        connectionMonitor /*= nullptr*/)
// TODO: Add std::shared_ptr<boost::asio::io_context> ioContext as parameter
{
  // Use atomic state check
  AppState expected = AppState::Uninitialized;
  if (!state_.compare_exchange_strong(expected, AppState::Initializing)) {
    std::cerr
        << "CockpitClientApp: Already initialized or in a different state."
        << std::endl;
    return false;
  }

  std::cout << "CockpitClientApp: Initializing..." << std::endl;

  // 1. Store Configuration
  config_ = config;
  std::cout << "CockpitClientApp: Config stored." << std::endl;

  // 2. Store Injected Components (Transfer ownership/share ownership)
  webrtcManager_ = std::move(webrtcManager);
  transportServer_ = std::move(transportServer);
  webCommandHandler_ =
      std::move(webCommandHandler);  // Store the web command handler
  inputDeviceSource_ =
      std::move(inputDeviceSource);  // Store the input device source
  telemetryHandler_ = std::move(telemetryHandler);
  connectionMonitor_ = std::move(connectionMonitor);
  // TODO: Store ioContext_ = std::move(ioContext);

  // Validate essential components were injected
  if (!webrtcManager_ || !transportServer_ || !webCommandHandler_ ||
      !inputDeviceSource_ || !telemetryHandler_) {
    std::cerr << "CockpitClientApp: Essential components not injected!"
              << std::endl;
    state_ = AppState::Uninitialized;  // Reset state on failure
    return false;
  }
  std::cout << "CockpitClientApp: Components injected and stored." << std::endl;

  // 3. Setup Components (Call init and set callbacks on injected components)

  // Initialize Transport Server
  if (!transportServer_->init(config_.transport_server_address,
                              config_.transport_server_port,
                              config_.display_files_path)) {
    std::cerr << "CockpitClientApp: Failed to initialize Transport Server."
              << std::endl;
    state_ = AppState::Uninitialized;
    return false;
  }
  if (!setupTransportServerCallbacks()) {
    std::cerr << "CockpitClientApp: Failed to setup Transport Server callbacks."
              << std::endl;
    state_ = AppState::Uninitialized;
    return false;
  }
  std::cout
      << "CockpitClientApp: Transport server initialized and callbacks setup."
      << std::endl;

  // Initialize WebRTC Manager
  // TODO: webrtcManager_->init(config_.webrtc_config, ioContext_); // Assuming
  // WebRTC Manager has an init and needs ioContext
  if (!setupWebrtcManagerCallbacks()) {
    std::cerr << "CockpitClientApp: Failed to setup WebRTC Manager callbacks."
              << std::endl;
    state_ = AppState::Uninitialized;
    return false;
  }
  std::cout
      << "CockpitClientApp: WebRTC manager initialized and callbacks setup."
      << std::endl;

  // Initialize Handlers/Sources that handle commands and telemetry
  // They depend on WebrtcManager/TransportServer - already injected/stored
  if (!setupCommandAndInputHandlers()) {
    std::cerr << "CockpitClientApp: Failed to setup Command and Input "
                 "Handlers/Sources."
              << std::endl;
    state_ = AppState::Uninitialized;
    return false;
  }
  std::cout
      << "CockpitClientApp: Command and Input Handlers/Sources initialized."
      << std::endl;

  if (!setupTelemetryHandler()) {
    std::cerr << "CockpitClientApp: Failed to setup Telemetry Handler."
              << std::endl;
    state_ = AppState::Uninitialized;
    return false;
  }
  std::cout << "CockpitClientApp: Telemetry Handler initialized." << std::endl;

  // Setup Connection Monitor callbacks only if monitor is provided
  if (connectionMonitor_) {
    // TODO: connectionMonitor_->init(ioContext_, webrtcManager_,
    // config_.heartbeat_interval_ms); // Initialize monitor with
    // dependencies/config
    if (!setupConnectionMonitorCallbacks()) {
      std::cerr
          << "CockpitClientApp: Failed to setup Connection Monitor callbacks."
          << std::endl;
      state_ = AppState::Uninitialized;
      return false;
    }
    std::cout << "CockpitClientApp: Connection monitor initialized and "
                 "callbacks setup."
              << std::endl;
  } else {
    std::cout << "CockpitClientApp: Connection Monitor not provided."
              << std::endl;
  }

  state_ = AppState::Initialized;
  std::cout << "CockpitClientApp: Initialization successful." << std::endl;
  return true;
}

// --- Running the Application ---

int CockpitClientApp::run() {
  AppState expected = AppState::Initialized;
  if (!state_.compare_exchange_strong(expected, AppState::Running)) {
    std::cerr << "CockpitClientApp: Cannot run, not in Initialized state."
              << std::endl;
    return 1;  // Indicate error
  }

  std::cout << "CockpitClientApp: Running main loop..." << std::endl;

  // 1. Start local transport server (serves UI, handles WS)
  if (!transportServer_->start()) {
    std::cerr << "CockpitClientApp: Failed to start transport server."
              << std::endl;
    // TODO: Decide if app should stop or retry/report error
    state_ = AppState::Stopped;  // Transition to stopped on major failure
    return 1;
  }
  std::cout << "CockpitClientApp: Transport server started." << std::endl;

  // 2. Start WebRTC manager (connects to signaling, handles PC)
  if (!webrtcManager_->start()) {
    std::cerr << "CockpitClientApp: Failed to start WebRTC manager."
              << std::endl;
    // Clean up transport server before exiting
    transportServer_->stop();
    state_ = AppState::Stopped;  // Transition to stopped on major failure
    return 1;
  }
  std::cout << "CockpitClientApp: WebRTC manager started." << std::endl;

  // 3. Start input device polling/sending loop
  if (!inputDeviceSource_->startPolling()) {
    std::cerr
        << "CockpitClientApp: Failed to start input device source polling."
        << std::endl;
    // Decide failure policy: continue without device input? or stop?
    // For critical input, maybe stop. For non-critical, maybe log and continue.
    // Let's assume it's critical for remote driving and stop.
    webrtcManager_->stop();
    transportServer_->stop();
    state_ = AppState::Stopped;  // Transition to stopped on major failure
    return 1;
  }
  std::cout << "CockpitClientApp: Input device source polling started."
            << std::endl;

  // 4. Start connection monitor (if used)
  if (connectionMonitor_) {
    connectionMonitor_->start();
    std::cout << "CockpitClientApp: Connection monitor started." << std::endl;
  }

  // 5. Run the main event loop
  // This is where the application blocks, handling events from WebRTC,
  // WebSocket server, timers, etc.
  // TODO: Implement event loop execution using the injected/owned ioContext
  // The event loop is the heart of the application. It receives events (network
  // data, timer expirations, etc.) and dispatches calls to the appropriate
  // handlers (the callbacks registered in setupXxxCallbacks). The main thread
  // will typically run or join the thread running the event loop.

  // Example using Asio:
  // if (ioContext_) {
  //     std::cout << "CockpitClientApp: Starting Asio io_context..." <<
  //     std::endl;
  //     // Option A: Run on main thread (blocks main)
  //     // ioContext_->run();
  //
  //     // Option B: Run on a separate thread (main thread waits or does other
  //     work)
  //     // ioThread_ = std::make_unique<std::thread>([this](){
  //     //     try { ioContext_->run(); } catch(...) { /* handle exceptions */
  //     }
  //     // });
  //     // ioThread_->join(); // Block main thread until io_context stops
  //     std::cout << "CockpitClientApp: Asio io_context finished." <<
  //     std::endl;
  // } else {
  // Placeholder for simulation if no event loop framework is used
  // This loop needs a way to break when stop() is called, typically
  // by signaling the loop/context or using a condition variable signaled by
  // stop().
  std::cout
      << "CockpitClientApp: Using simulated run loop (No real event loop)..."
      << std::endl;
  while (state_ == AppState::Running) {
    // In a real app, this loop is replaced by event loop.
    // Placeholder sleep prevents high CPU usage in simulation.
    std::this_thread::sleep_for(std::chrono::seconds(1));
    // std::cout << "App running..." << std::endl; // Avoid spamming output
  }
  std::cout << "CockpitClientApp: Simulated run loop finished." << std::endl;
  // }

  std::cout << "CockpitClientApp: Run method exiting." << std::endl;
  // The state should transition to STOPPED during or after the stop() call,
  // but ensure it's finalized here if the run loop exited for other reasons.
  if (state_ != AppState::Stopped) {
    state_ = AppState::Stopped;
  }
  return 0;  // Indicate success
}

// --- Stopping the Application ---

void CockpitClientApp::stop() {
  // Use atomic compare_exchange to ensure only one thread performs stopping
  AppState expected = AppState::Running;
  if (!state_.compare_exchange_strong(expected, AppState::Stopping)) {
    expected = AppState::Initialized;  // Also allow stopping from Initialized
                                       // state if run() wasn't called
    if (!state_.compare_exchange_strong(expected, AppState::Stopping)) {
      std::cout << "CockpitClientApp: Already stopping, stopped, or "
                   "uninitialized. Skipping stop."
                << std::endl;
      return;
    }
  }

  std::cout << "CockpitClientApp: Stopping..." << std::endl;

  // 1. Signal event loop to stop if it's running (must be done before joining
  // thread)
  // TODO: if (ioContext_) { ioContext_->stop(); }

  // 2. Stop components in a safe order (dependent components first)
  // Stop connection monitoring first (depends on WebRTC Manager)
  if (connectionMonitor_) {
    connectionMonitor_->stop();
    std::cout << "CockpitClientApp: Connection Monitor stopped." << std::endl;
  }
  // Stop input device polling (might have a background thread)
  if (inputDeviceSource_) {
    inputDeviceSource_->stopPolling();
    std::cout << "CockpitClientApp: Input Device Source stopped." << std::endl;
  }
  // Stop WebRTC manager (closes connections, stops threads/tasks)
  if (webrtcManager_) {
    webrtcManager_->stop();
    std::cout << "CockpitClientApp: WebRTC Manager stopped." << std::endl;
  }
  // Stop local transport server (closes WS, stops HTTP, stops threads/tasks)
  if (transportServer_) {
    transportServer_->stop();
    std::cout << "CockpitClientApp: Transport Server stopped." << std::endl;
  }

  // Handlers (webCommandHandler_, telemetryHandler_) don't typically have
  // explicit stop methods; they just become inactive when their dependencies
  // (TransportServer, WebrtcManager) stop.

  // 3. Join event loop thread if it was started in run()
  // TODO: if (ioThread_ && ioThread_->joinable()) {
  //     ioThread_->join();
  //     std::cout << "CockpitClientApp: I/O thread joined." << std::endl;
  // }

  std::cout << "CockpitClientApp: All components stopped." << std::endl;
  state_ = AppState::Stopped;  // Final state
}

// --- Component Setup Methods ---

bool CockpitClientApp::setupWebrtcManagerCallbacks() {
  std::cout << "CockpitClientApp: Setting up WebrtcManager callbacks..."
            << std::endl;
  // webrtcManager_ is validated in init
  // Use lambdas for cleaner binding, capturing 'this'
  webrtcManager_->onPeerConnected([this](const std::string& peer_id) {
    // Ensure this method is thread-safe! Called from WebRTC thread.
    handlePeerConnected(peer_id);
  });
  webrtcManager_->onPeerDisconnected(
      [this](const std::string& peer_id, const std::string& reason) {
        // Ensure this method is thread-safe! Called from WebRTC thread.
        handlePeerDisconnected(peer_id, reason);
      });
  // Route DataChannel messages based on label
  webrtcManager_->onDataChannelMessage(
      [this](const std::string& peer_id, const std::string& label,
             const std::vector<char>& message) {
        // Ensure this method is thread-safe! Called from WebRTC thread.
        handleWebrtcDataChannelMessageReceived(peer_id, label, message);
      });
  webrtcManager_->onError([this](const std::string& error_msg) {
    // Ensure this method is thread-safe! Called from WebRTC thread.
    handleWebrtcError(error_msg);
  });
  // TODO: If C++ handles video, register video track handler:
  // webrtcManager_->onVideoTrackReceived([this](const std::string& peer_id,
  // rtc::scoped_refptr<webrtc::VideoTrackInterface> track) {
  // // Ensure this method is thread-safe! Called from WebRTC thread.
  // handleWebrtcVideoTrackReceived(peer_id, track);
  // });

  std::cout << "CockpitClientApp: WebrtcManager callbacks setup complete."
            << std::endl;
  return true;
}

bool CockpitClientApp::setupTransportServerCallbacks() {
  std::cout << "CockpitClientApp: Setting up Transport Server callbacks..."
            << std::endl;
  // transportServer_ is validated in init
  // Use lambda capturing 'this'
  transportServer_->onWebSocketConnected([this](WebSocketConnectionId conn_id) {
    // Ensure this method is thread-safe! Called from TransportServer thread.
    handleWsConnected(conn_id);
  });
  transportServer_->onWebSocketDisconnected(
      [this](WebSocketConnectionId conn_id) {
        // Ensure this method is thread-safe! Called from TransportServer
        // thread.
        handleWsDisconnected(conn_id);
      });
  transportServer_->onWebSocketMessageReceived(
      [this](WebSocketConnectionId conn_id, const std::vector<char>& message) {
        // Ensure this method is thread-safe! Called from TransportServer
        // thread.
        handleWsMessageReceived(conn_id, message);
      });
  transportServer_->onServerError([this](const std::string& error_msg) {
    // Ensure this method is thread-safe! Called from TransportServer thread.
    handleTransportServerError(error_msg);
  });

  std::cout << "CockpitClientApp: Transport Server callbacks setup complete."
            << std::endl;
  return true;
}

bool CockpitClientApp::setupCommandAndInputHandlers() {
  std::cout
      << "CockpitClientApp: Setting up Command and Input Handlers/Sources..."
      << std::endl;
  // Handlers/Sources require shared_ptrs to WebrtcManager and config, which are
  // injected. webrtcManager_ is validated in init before this is called.

  // Initialize WebCommandHandler
  if (!webCommandHandler_->init(webrtcManager_, config_.control_channel_label,
                                config_.target_vehicle_id)) {
    std::cerr << "CockpitClientApp: Failed to initialize Web Command Handler."
              << std::endl;
    return false;
  }
  std::cout << "CockpitClientApp: Web Command Handler initialized."
            << std::endl;

  // Initialize InputDeviceSource
  // TODO: Pass specific input device config from CockpitConfig
  if (!inputDeviceSource_->init(
          webrtcManager_, config_.control_channel_label,
          config_.target_vehicle_id /*, config_.input_device_config */)) {
    std::cerr << "CockpitClientApp: Failed to initialize Input Device Source."
              << std::endl;
    return false;
  }
  std::cout << "CockpitClientApp: Input Device Source initialized."
            << std::endl;

  std::cout
      << "CockpitClientApp: Command and Input Handlers/Sources setup complete."
      << std::endl;
  return true;
}

bool CockpitClientApp::setupTelemetryHandler() {
  std::cout << "CockpitClientApp: Setting up Telemetry Handler..." << std::endl;
  // TelemetryHandler requires shared_ptr to TransportServer, which is injected.
  // transportServer_ is validated in init before this is called.
  if (!telemetryHandler_->init(transportServer_)) {
    std::cerr << "CockpitClientApp: Failed to initialize Telemetry Handler."
              << std::endl;
    return false;
  }
  std::cout << "CockpitClientApp: Telemetry Handler initialized." << std::endl;
  return true;
}

bool CockpitClientApp::setupConnectionMonitorCallbacks() {
  std::cout << "CockpitClientApp: Setting up Connection Monitor callbacks..."
            << std::endl;
  if (!connectionMonitor_)
    return true;  // Optional, just return true if not provided
  // connectionMonitor_ is validated in init if it exists

  // Set application-level handlers on the monitor
  // Use lambda capturing 'this'
  connectionMonitor_->onNetworkUp([this](const std::string& peer_id) {
    // Ensure this method is thread-safe! Called from ConnectionMonitor thread.
    handleNetworkUp(peer_id);
  });
  connectionMonitor_->onNetworkDown(
      [this](const std::string& peer_id, const std::string& reason) {
        // Ensure this method is thread-safe! Called from ConnectionMonitor
        // thread.
        handleNetworkDown(peer_id, reason);
      });
  connectionMonitor_->onHeartbeatLost([this](const std::string& peer_id) {
    // Ensure this method is thread-safe! Called from ConnectionMonitor thread.
    handleHeartbeatLost(peer_id);
  });

  std::cout << "CockpitClientApp: Connection Monitor callbacks setup complete."
            << std::endl;
  return true;
}

// --- Handlers for WebrtcManager Events (Called by WebrtcManager threads) ---
// These methods receive events from WebRTC and route them to the appropriate
// Handlers or update app state/UI.

void CockpitClientApp::handlePeerConnected(const std::string& peer_id) {
  // This handler is called from WebRTC internal thread. MUST BE THREAD-SAFE.
  std::cout << "App: Peer " << peer_id << " (Vehicle) connected via WebRTC."
            << std::endl;
  // Role: Getting vehicle state -> Notify UI via TelemetryHandler
  // Role: Sending commands -> Maybe enable controls in UI?
  // TODO: Update UI status via TelemetryHandler
  // telemetryHandler_->notifyConnectionStatus(peer_id, true); // Assuming
  // TelemetryHandler has this method

  // If using ConnectionMonitor, it might trigger NetworkUp handler
  // If the app auto-connects, maybe trigger offer/answer here?
  // webrtcManager_->createOffer(...); // Example, depends on WebRTC connection
  // flow
}

void CockpitClientApp::handlePeerDisconnected(const std::string& peer_id,
                                              const std::string& reason) {
  // This handler is called from WebRTC internal thread. MUST BE THREAD-SAFE.
  std::cout << "App: Peer " << peer_id
            << " (Vehicle) disconnected. Reason: " << reason << std::endl;
  // Role: Getting vehicle state -> Notify UI via TelemetryHandler
  // Role: Sending commands -> Disable controls in UI?
  // TODO: Update UI status via TelemetryHandler
  // telemetryHandler_->notifyConnectionStatus(peer_id, false); // Assuming
  // TelemetryHandler has this method
  // TODO: Maybe try to reconnect?

  // If using ConnectionMonitor, it might trigger NetworkDown handler
}

void CockpitClientApp::handleWebrtcDataChannelMessageReceived(
    const std::string& peer_id, const std::string& label,
    const std::vector<char>& message) {
  // This handler is called from a WebRTC internal thread. MUST BE THREAD-SAFE.
  // Role: Getting vehicle state -> Receives raw data, deserializes, and routes
  // to TelemetryHandler std::cout << "App: Received DataChannel message from "
  // << peer_id << ", label=" << label << ", size=" << message.size() <<
  // std::endl;

  if (label == config_.telemetry_channel_label) {
    // Deserialize Protobuf and pass to TelemetryHandler
    autodev::remote::chassis::Chassis telemetry_data;
    if (telemetry_data.ParseFromArray(message.data(), message.size())) {
      // std::cout << "App: Parsed incoming telemetry." << std::endl;
      // Call TelemetryHandler to process and forward to UI.
      // TelemetryHandler::processIncomingTelemetry MUST BE THREAD-SAFE.
      telemetryHandler_->processIncomingTelemetry(peer_id, telemetry_data);
    } else {
      std::cerr << "App: Failed to parse telemetry message from " << peer_id
                << " on label " << label << std::endl;
      // TODO: Handle parsing error (log, notify UI?)
    }
  } else if (label == config_.control_channel_label) {
    // This might be loopback if the vehicle echoes control commands
    // Role: Sending commands -> Is this an echo of commands we sent?
    // TODO: Decide if loopback needs processing or can be ignored. Typically
    // ignored. std::cout << "App: Received loopback control message from " <<
    // peer_id << " (ignored)." << std::endl;
  } else {
    std::cout << "App: Received DataChannel message on unknown label: " << label
              << " from " << peer_id << std::endl;
    // TODO: Log unexpected message
  }
}

void CockpitClientApp::handleWebrtcError(const std::string& error_msg) {
  // This handler is called from a WebRTC internal thread. MUST BE THREAD-SAFE.
  std::cerr << "App: WebRTC Error: " << error_msg << std::endl;
  // TODO: Handle errors (logging, updating UI status, retry logic).
  // Critical errors might warrant stopping the app or attempting reconnection.
  // telemetryHandler_->notifyWebrtcError(error_msg); // Assuming
  // TelemetryHandler can notify UI
}

// --- Optional: Handler for incoming video tracks if C++ processes them (Called
// by WebRTC threads) --- Role: Serving UI/Video/Telemetry -> Receive video
// track and send frames to UI void
// CockpitClientApp::handleWebrtcVideoTrackReceived(const std::string& peer_id,
// rtc::scoped_refptr<webrtc::VideoTrackInterface> track) {
//     // This handler is called from a WebRTC internal thread. MUST BE
//     THREAD-SAFE. std::cout << "App: Received video track from " << peer_id <<
//     std::endl;
//     // TODO: Implement logic to get frames from the track and send them to
//     // the display via TransportServer. This is complex and often involves a
//     custom
//     // VideoSink implementation that attaches to the track and uses the
//     TransportServer
//     // (or a dedicated video streaming part of it) to send frames to the UI.
//     // telemetryHandler_->addVehicleVideoTrack(peer_id, track); // Assuming
//     TelemetryHandler handles video display via TransportServer
// }

// --- Handlers for TransportServer (WebSocket) Events (Called by
// TransportServer threads) --- These methods receive events from the local UI
// via WebSocket and route messages to the CommandHandler.

void CockpitClientApp::handleWsConnected(WebSocketConnectionId conn_id) {
  // This handler is called from a TransportServer internal thread. MUST BE
  // THREAD-SAFE.
  std::cout << "App: WebSocket client connected (ID " << conn_id << ")"
            << std::endl;
  // Role: Serving UI/Video/Telemetry -> Manage connected UI clients, send
  // initial state
  // TODO: If only one display client is supported, store this ID. If multiple,
  // track them (e.g., in TelemetryHandler).
  // telemetryHandler_->addClient(conn_id); // Assuming TelemetryHandler tracks
  // clients Send initial state/config to the newly connected UI client via
  // TelemetryHandler. telemetryHandler_->sendInitialState(conn_id); // Assuming
  // TelemetryHandler has this method
}

void CockpitClientApp::handleWsDisconnected(WebSocketConnectionId conn_id) {
  // This handler is called from a TransportServer internal thread. MUST BE
  // THREAD-SAFE.
  std::cout << "App: WebSocket client disconnected (ID " << conn_id << ")"
            << std::endl;
  // Role: Serving UI/Video/Telemetry -> Manage connected UI clients
  // TODO: Remove from active connections map in
  // TelemetryHandler/TransportServer. telemetryHandler_->removeClient(conn_id);
  // // Assuming TelemetryHandler tracks clients
}

void CockpitClientApp::handleWsMessageReceived(
    WebSocketConnectionId conn_id, const std::vector<char>& message) {
  // This handler is called from a TransportServer internal thread. MUST BE
  // THREAD-SAFE. Role: Sending commands -> Receives raw message from UI and
  // routes to WebCommandHandler std::cout << "App: Received WebSocket message
  // from " << conn_id << ", size=" << message.size() << std::endl; Call
  // WebCommandHandler to process the raw message.
  // WebCommandHandler::processRawWebCommand MUST BE THREAD-SAFE.
  webCommandHandler_->processRawWebCommand(conn_id, message);
}

void CockpitClientApp::handleTransportServerError(
    const std::string& error_msg) {
  // This handler is called from a TransportServer internal thread. MUST BE
  // THREAD-SAFE.
  std::cerr << "App: Transport Server Error: " << error_msg << std::endl;
  // Role: Serving UI/Video/Telemetry -> Report critical server errors
  // TODO: Handle server errors (logging, attempt restart? Notify UI?).
  // A critical transport server error might mean the UI can no longer connect.
  // Consider stopping the application gracefully or transitioning to an error
  // state. telemetryHandler_->notifyServerError(error_msg); // Assuming
  // TelemetryHandler can notify UI stop(); // Example: stop the app on severe
  // server error
}

// --- Handlers for ConnectionMonitor Events (Optional, Called by
// ConnectionMonitor thread) --- These methods receive network status updates
// and route them to the UI.

void CockpitClientApp::handleNetworkUp(const std::string& peer_id) {
  // This handler is called from the ConnectionMonitor thread. MUST BE
  // THREAD-SAFE.
  std::cout << "App: Network is UP with peer " << peer_id << std::endl;
  // Role: Getting vehicle state -> Update UI status via TelemetryHandler
  // telemetryHandler_->notifyNetworkStatus(peer_id, true); // Assuming
  // TelemetryHandler has this method
}

void CockpitClientApp::handleNetworkDown(const std::string& peer_id,
                                         const std::string& reason) {
  // This handler is called from the ConnectionMonitor thread. MUST BE
  // THREAD-SAFE.
  std::cerr << "App: Network is DOWN with peer " << peer_id
            << ". Reason: " << reason << std::endl;
  // Role: Getting vehicle state -> Update UI status via TelemetryHandler
  // Role: Sending commands -> Disable controls in UI, potentially trigger
  // safety action on vehicle? The safety action should ideally be handled by
  // the vehicle itself based on heartbeat loss, but the cockpit might also
  // signal it or update UI state.
  // telemetryHandler_->notifyNetworkStatus(peer_id, false, reason); // Assuming
  // TelemetryHandler has this method commandHandler_->disableControls("Network
  // Down"); // Assuming CommandHandler can send a safety command or disable UI
  // controls internally - Note: This would call webCommandHandler_ or
  // inputDeviceSource_ depending on which can trigger safety actions? Or
  // perhaps a new safety_handler? Rethink safety action trigger flow.
}

void CockpitClientApp::handleHeartbeatLost(const std::string& peer_id) {
  // This handler is called from the ConnectionMonitor thread. MUST BE
  // THREAD-SAFE.
  std::cerr << "App: Heartbeat lost from peer " << peer_id << std::endl;
  // Role: Getting vehicle state -> Update UI status via TelemetryHandler
  // Role: Sending commands -> Trigger safety action, disable controls
  // telemetryHandler_->notifyHeartbeatStatus(peer_id, false); // Assuming
  // TelemetryHandler has this method
  // commandHandler_->triggerEmergencyStop("Heartbeat Lost"); // Assuming
  // CommandHandler can send emergency command or disable UI controls internally
  // - Similar note as NetworkDown regarding safety trigger.
}

}  // namespace cockpit
}  // namespace remote
}  // namespace autodev

// --- Main Application Entry Point ---

// Define the global pointer in the global namespace, outside the autodev
// namespace. This pointer is used by the signal handler to access the
// application instance.
CockpitClientApp* global_app_instance =
    nullptr;  // Use forward declaration in .h

int main(int argc, char* argv[]) {
  // Set up signal handlers for graceful shutdown (Ctrl+C, etc.)
  // Using a global instance is one common pattern for this.
  // Consider integrating signal handling directly with your event loop (e.g.,
  // Asio's signal_set) for a potentially cleaner approach, especially in
  // multi-threaded apps.
  std::signal(SIGINT, signal_handler);   // Ctrl+C
  std::signal(SIGTERM, signal_handler);  // Termination signal
  // Ignore SIGPIPE if writing to disconnected sockets (e.g., broken WebSocket
  // connection)
  std::signal(SIGPIPE, SIG_IGN);

  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <config_file_path>" << std::endl;
    return 1;
  }

  std::string config_path = argv[1];

  // 1. Load Configuration (Done outside the App for dependency injection)
  // Use the concrete loader implementation
  autodev::remote::config::JsonConfigLoader config_loader;
  auto loaded_config = config_loader.loadConfig(
      config_path);  // Assuming loader can load CockpitConfig
  if (!loaded_config) {
    std::cerr << "Failed to load configuration from " << config_path
              << std::endl;
    return 1;
  }
  // Get the loaded config structure
  autodev::remote::cockpit::CockpitConfig app_config = *loaded_config;
  std::cout << "Configuration loaded successfully." << std::endl;

  // TODO: 2. Create Event Loop Context (if owned by main or passed to
  // components) auto io_context = std::make_shared<boost::asio::io_context>();
  // // Or unique_ptr if app owns it

  // 3. Create Concrete Component Instances (Done outside the App)
  // Instantiate concrete classes based on configuration or default.
  // Use appropriate smart pointers as defined in CockpitClientApp.h
  // Dependencies are passed to init, their configuration might come from
  // app_config.
  auto webrtc_manager = std::make_shared<
      autodev::remote::webrtc::WebrtcManagerImpl>();  // Needs config and
                                                      // io_context?
  auto transport_server = std::make_shared<
      autodev::remote::transport::WebSocketTransportServer>();  // Needs config
                                                                // and
                                                                // io_context?

  // Create Handlers/Sources for commands and telemetry
  // They need shared_ptrs to webrtc_manager and transport_server, and
  // configuration
  auto web_command_handler = std::make_unique<
      autodev::remote::drivers::WebCommandHandlerImpl>();  // Needs
                                                           // webrtc_manager,
                                                           // config
  // TODO: Create InputDeviceSourceImpl based on config_.input_device_config
  auto input_device_source = std::make_unique<
      autodev::remote::drivers::InputDeviceSourceImpl>();  // Needs
                                                           // webrtc_manager,
                                                           // config, specific
                                                           // device config
  auto telemetry_handler = std::make_unique<
      autodev::remote::drivers::TelemetryHandlerImpl>();  // Needs
                                                          // transport_server

  // Create Connection Monitor only if heartbeat is configured and WebrtcManager
  // is available
  std::unique_ptr<autodev::remote::network_manager::IConnectionMonitor>
      connection_monitor = nullptr;
  if (app_config.heartbeat_interval_ms > 0 && webrtc_manager) {
    // Connection Monitor needs WebrtcManager and config
    // TODO: It also needs io_context to run its timer/logic
    connection_monitor = std::make_unique<
        autodev::remote::network_manager::ConnectionMonitorImpl>(
        webrtc_manager,
        app_config
            .heartbeat_interval_ms);  // Assuming MonitorImpl takes shared_ptr
                                      // to WebrtcManager and interval
    std::cout << "Connection Monitor created." << std::endl;
  } else {
    std::cout << "Heartbeat interval <= 0 or WebrtcManager not available, "
                 "Connection Monitor not created."
              << std::endl;
  }

  // 4. Create and Initialize the Application with Dependencies
  autodev::remote::cockpit::CockpitClientApp app;
  // Set global instance BEFORE init, in case init fails and destructor is
  // called
  global_app_instance = &app;

  if (!app.init(app_config, std::move(webrtc_manager),
                std::move(transport_server),
                std::move(web_command_handler),  // Pass web command handler
                std::move(input_device_source),  // Pass input device source
                std::move(telemetry_handler), std::move(connection_monitor)
                // TODO: Pass io_context here
                )) {
    std::cerr << "Failed to initialize cockpit client application."
              << std::endl;
    global_app_instance = nullptr;  // Clear global pointer on failure
    return 1;
  }

  std::cout << "Cockpit client initialized. Running..." << std::endl;

  // 5. Run the Application (delegates to event loop)
  // This method should block and manage the event loop.
  int return_code = app.run();

  std::cout << "Cockpit client stopped." << std::endl;
  global_app_instance = nullptr;  // Clear global pointer on exit
  return return_code;
}
