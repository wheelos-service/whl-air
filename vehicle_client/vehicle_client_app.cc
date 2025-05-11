#include "vehicle_client_app.h"

// Include concrete implementations (moved to main or factory in ideal scenario)
#include "config/json_config_loader.h"  // Example concrete loader
#include "control/apollo_controller.h"  // Example concrete controller
#include "network_manager/connection_monitor_impl.h"  // Example concrete monitor
#include "sensors/canbus_chassis_source.h"  // Example concrete chassis
#include "sensors/v4l2_camera_source.h"     // Example concrete camera
#include "webrtc/webrtc_manager_impl.h"     // Example concrete webrtc manager

// Placeholder for Protobuf messages (needs actual definition)
// #include "proto/control/control_command.pb.h"
// #include "proto/control/emergency_command.pb.h"
// #include "proto/chassis/chassis_state.pb.h"

#include <chrono>
#include <functional>
#include <iostream>
#include <thread>

// Dummy Protobuf messages for compilation
namespace autodev {
namespace remote {
namespace control {

struct ControlCommand {
  void set_acceleration(double) {}
};
struct EmergencyCommand {
  void set_type(int) {}
  enum Type { EMERGENCY_STOP };
};
}  // namespace control
namespace chassis {
struct Chassis {
  size_t ByteSizeLong() const { return 0; }
  bool SerializeToArray(void*, size_t) const { return true; }
  double speed_mps() const { return 0.0; }
};
}  // namespace chassis
}  // namespace remote
}  // namespace autodev

namespace autodev {
namespace remote {
namespace vehicle {

// --- Constructor and Destructor ---

VehicleClientApp::VehicleClientApp() : state_(AppState::Uninitialized) {
  std::cout << "VehicleClientApp created." << std::endl;
}

VehicleClientApp::~VehicleClientApp() {
  std::cout << "VehicleClientApp destroying..." << std::endl;
  stop();  // Ensure stop is called
  std::cout << "VehicleClientApp destroyed." << std::endl;
}

// --- Initialization ---

bool VehicleClientApp::init(
    const VehicleConfig& config, std::unique_ptr<WebrtcManager> webrtcManager,
    std::unique_ptr<IController> controller,
    std::unique_ptr<ICameraSource> cameraSource,
    std::unique_ptr<IChassisSource> chassisSource) {
  if (state_ != AppState::Uninitialized) {
    std::cerr
        << "VehicleClientApp: Already initialized or in a different state."
        << std::endl;
    return false;
  }

  state_ = AppState::Initializing;
  std::cout << "VehicleClientApp: Initializing..." << std::endl;

  // 1. Store Configuration
  config_ = config;
  std::cout << "VehicleClientApp: Config stored." << std::endl;

  // 2. Store Injected Components
  // Transfer ownership of injected components
  webrtcManager_ = std::move(webrtcManager);
  controller_ = std::move(controller);
  cameraSource_ = std::move(cameraSource);
  chassisSource_ = std::move(chassisSource);

  // 3. Setup Components (Configure and connect signals/slots/callbacks)
  // Now just call setup methods on the injected components

  if (!setupWebrtcManager()) {
    std::cerr << "VehicleClientApp: Failed to setup WebRTC Manager."
              << std::endl;
    state_ = AppState::Uninitialized;  // Reset state on failure
    return false;
  }

  if (!setupController()) {
    std::cerr << "VehicleClientApp: Failed to setup Controller." << std::endl;
    state_ = AppState::Uninitialized;  // Reset state on failure
    return false;
  }

  if (!setupSensors()) {
    std::cerr << "VehicleClientApp: Failed to setup Sensors." << std::endl;
    state_ = AppState::Uninitialized;  // Reset state on failure
    return false;
  }

  state_ = AppState::Initialized;
  std::cout << "VehicleClientApp: Initialization successful." << std::endl;
  return true;
}

// --- Running the Application ---

int VehicleClientApp::run() {
  if (state_ != AppState::Initialized) {
    std::cerr << "VehicleClientApp: Cannot run, not in Initialized state."
              << std::endl;
    return 1;  // Indicate error
  }

  state_ = AppState::Running;
  std::cout << "VehicleClientApp: Running main loop..." << std::endl;

  // TODO: Integrate with the actual event loop managed by libraries (e.g.,
  // libwebrtc's signaling thread, boost::asio::io_context). The run() method
  // should typically delegate to the event loop's run method or join its
  // thread. Example with Asio: if (ioContext_) {
  //     ioThread_ = std::make_unique<std::thread>([this]{ ioContext_->run();
  //     });
  // }

  // For skeleton, simulate running and wait for stop signal
  // In a real application, Ctrl+C or other signals should trigger stop().
  // The event loop would prevent the main thread from exiting here.
  while (state_ == AppState::Running) {
    // In a real app, this loop doesn't actively do work, the event loop
    // callbacks do. We need a mechanism to exit this loop when stop() is
    // called. This placeholder will just print and wait.
    std::this_thread::sleep_for(std::chrono::seconds(1));
    // std::cout << "App running..." << std::endl; // Avoid spamming output
  }

  std::cout << "VehicleClientApp: Main loop finished." << std::endl;
  state_ = AppState::Stopped;
  return 0;  // Indicate success
}

// --- Stopping the Application ---

void VehicleClientApp::stop() {
  if (state_ == AppState::Stopping || state_ == AppState::Stopped ||
      state_ == AppState::Uninitialized) {
    std::cout << "VehicleClientApp: Already stopping, stopped, or "
                 "uninitialized. Skipping stop."
              << std::endl;
    return;
  }

  state_ = AppState::Stopping;
  std::cout << "VehicleClientApp: Stopping..." << std::endl;

  // TODO: Signal event loop to stop if it's running on a separate thread.
  // Example with Asio:
  // if (ioContext_) {
  //     ioContext_->stop();
  // }

  // Stop sensor data flow
  if (chassisSource_) {
    chassisSource_->stopUpdates();
    std::cout << "VehicleClientApp: Chassis Source stopped." << std::endl;
  }
  if (cameraSource_) {
    cameraSource_->stopCapture();
    std::cout << "VehicleClientApp: Camera Source stopped." << std::endl;
  }

  // Stop WebRTC gracefully
  if (webrtcManager_) {
    webrtcManager_->stop();
    std::cout << "VehicleClientApp: WebRTC Manager stopped." << std::endl;
  }

  // The controller doesn't typically have a 'stop' method in this context,
  // it just stops processing commands when the app stops.

  // TODO: Join event loop thread if it was started in run().
  // Example with Asio:
  // if (ioThread_ && ioThread_->joinable()) {
  //     ioThread_->join();
  //     std::cout << "VehicleClientApp: I/O thread joined." << std::endl;
  // }

  std::cout << "VehicleClientApp: All components stopped." << std::endl;
  state_ = AppState::Stopped;
}

// --- Component Setup Methods (now simpler) ---

bool VehicleClientApp::setupWebrtcManager() {
  std::cout << "VehicleClientApp: Setting up WebrtcManager callbacks..."
            << std::endl;
  if (!webrtcManager_) {
    std::cerr << "VehicleClientApp: WebrtcManager not injected!" << std::endl;
    return false;
  }

  // Use lambdas for cleaner binding
  webrtcManager_->onPeerConnected(
      [this](const std::string& peer_id) { handlePeerConnected(peer_id); });
  webrtcManager_->onPeerDisconnected(
      [this](const std::string& peer_id, const std::string& reason) {
        handlePeerDisconnected(peer_id, reason);
      });
  webrtcManager_->onControlMessageReceived(
      [this](const std::string& peer_id, const std::vector<char>& message) {
        handleControlMessageReceived(peer_id, message);
      });
  webrtcManager_->onTelemetryMessageReceived(
      [this](const std::string& peer_id, const std::vector<char>& message) {
        handleTelemetryMessageReceived(peer_id, message);
      });
  webrtcManager_->onError(
      [this](const std::string& error_msg) { handleWebrtcError(error_msg); });

  // TODO: Initialize the webrtcManager using the config
  // webrtcManager_->init(config_.webrtc); // Assuming WebrtcManager has an init
  // with relevant config
  std::cout << "VehicleClientApp: WebrtcManager setup complete." << std::endl;
  return true;
}

bool VehicleClientApp::setupController() {
  std::cout << "VehicleClientApp: Setting up Controller..." << std::endl;
  if (!controller_) {
    std::cerr << "VehicleClientApp: Controller not injected!" << std::endl;
    return false;
  }
  // TODO: Initialize the controller if it requires config
  // controller_->init(config_.control); // Assuming Controller has an init
  // method
  std::cout << "VehicleClientApp: Controller setup complete." << std::endl;
  return true;
}

bool VehicleClientApp::setupSensors() {
  std::cout << "VehicleClientApp: Setting up Sensors..." << std::endl;
  if (!cameraSource_ || !chassisSource_) {
    std::cerr << "VehicleClientApp: Sensor sources not injected!" << std::endl;
    return false;
  }

  // Init sensors with config
  if (!cameraSource_->init(
          config_.sensors.camera_device, config_.sensors.camera_width,
          config_.sensors.camera_height, config_.sensors.camera_fps)) {
    std::cerr << "VehicleClientApp: Failed to initialize camera source."
              << std::endl;
    return false;
  }

  // TODO: Pass chassis config to init
  if (!chassisSource_->init(/* config_.sensors.can_interface */)) {
    std::cerr << "VehicleClientApp: Failed to initialize chassis source."
              << std::endl;
    return false;
  }

  // Connect sensor outputs to handlers (Handlers will route data via WebRTC)
  // Note: This assumes sensor sources provide a mechanism to register callbacks
  // on frame capture or state update.
  // Example using lambda capturing 'this':
  cameraSource_->setOnFrameCapturedHandler([this](/* frame data */) {
    handleCameraFrameCaptured(/* frame data */);
  });

  chassisSource_->setOnStateUpdatedHandler(
      [this](const autodev::remote::chassis::Chassis& state) {
        handleChassisStateUpdated(state);
      });

  std::cout << "VehicleClientApp: Sensors setup complete." << std::endl;
  return true;
}

// --- Handlers for WebrtcManager events ---

void VehicleClientApp::handlePeerConnected(const std::string& peer_id) {
  std::cout << "App: Peer " << peer_id << " connected via WebRTC." << std::endl;
  // TODO: Start sending data/video for this peer
  // Sensors should start capturing/updating now that a peer is available.
  // Actual sending will happen in sensor data handlers or by piping sensor
  // output directly into WebRTC tracks/data channels.

  if (cameraSource_) {
    // startCapture needs a handler for frames - already set in setupSensors
    cameraSource_->startCapture(/* pass parameters like peer_id if needed */);
  }
  if (chassisSource_) {
    // startUpdates needs a handler for state - already set in setupSensors
    chassisSource_->startUpdates(/* pass parameters like peer_id if needed */);
  }
}

void VehicleClientApp::handlePeerDisconnected(const std::string& peer_id,
                                              const std::string& reason) {
  std::cout << "App: Peer " << peer_id << " disconnected. Reason: " << reason
            << std::endl;
  // TODO: Stop sending data/video specific to this peer if not handled
  // automatically. Clean up any peer-specific resources.

  // Policy Decision: Should sensors stop if ALL peers disconnect?
  // Current skeleton keeps them running. A production app might stop sensors
  // to save resources if no one is viewing/receiving data.
}

void VehicleClientApp::handleControlMessageReceived(
    const std::string& peer_id, const std::vector<char>& message) {
  // std::cout << "App: Received control message from " << peer_id << ", size="
  // << message.size() << std::endl;
  if (!controller_) {
    std::cerr
        << "App: Received control message but controller is not available!"
        << std::endl;
    return;
  }

  // TODO: Deserialize protobuf message and pass to controller
  // Needs to distinguish ControlCommand vs EmergencyCommand based on content or
  // a wrapper. Example (simplified): autodev::remote::control::ControlCommand
  // command; if (command.ParseFromArray(message.data(), message.size())) {
  //      controller_->processControlCommand(command);
  //      return; // Successfully processed as ControlCommand
  // }

  // // Try deserializing as EmergencyCommand
  // autodev::remote::control::EmergencyCommand emergency_command;
  // if (emergency_command.ParseFromArray(message.data(), message.size())) {
  //      controller_->processEmergencyCommand(emergency_command);
  //      return; // Successfully processed as EmergencyCommand
  // }

  // std::cerr << "App: Failed to parse control/emergency message from " <<
  // peer_id << std::endl;
  // TODO: Handle parsing error - maybe send NACK or log error details

  // Simulate processing by calling dummy controller methods
  // autodev::remote::control::ControlCommand dummy_cmd;
  // dummy_cmd.set_acceleration(1.0); // Set dummy data based on message content
  // if possible controller_->processControlCommand(dummy_cmd);

  // autodev::remote::control::EmergencyCommand dummy_ecmd;
  // dummy_ecmd.set_type(autodev::remote::control::EMERGENCY_STOP); // Set dummy
  // type based on message content
  // controller_->processEmergencyCommand(dummy_ecmd);

  // Placeholder print for received message
  // std::cout << "App: Processing control message (placeholder)." << std::endl;
}

void VehicleClientApp::handleTelemetryMessageReceived(
    const std::string& peer_id, const std::vector<char>& message) {
  // std::cout << "App: Received telemetry message from " << peer_id << ",
  // size=" << message.size() << std::endl; This might be loopback or commands
  // on the telemetry channel
  // TODO: Handle incoming telemetry data if applicable (e.g., for logging,
  // monitoring, diagnostics) Telemetry channel is usually for sending vehicle
  // data OUT, receiving IN is less common for control. If receiving,
  // deserialize message and process. std::cout << "App: Processing telemetry
  // message (placeholder)." << std::endl;
}

void VehicleClientApp::handleWebrtcError(const std::string& error_msg) {
  std::cerr << "App: WebRTC Error: " << error_msg << std::endl;
  // TODO: Handle errors (logging, retry logic, potentially trigger emergency
  // stop) Depending on the severity, this might necessitate stopping the
  // application or taking safety actions. Example: Triggering safety stop on
  // critical WebRTC failure if (controller_) {
  //     autodev::remote::control::EmergencyCommand emergency_command;
  //     emergency_command.set_type(autodev::remote::control::EMERGENCY_STOP);
  //     // emergency_command.set_reason("WebRTC Critical Error");
  //     controller_->processEmergencyCommand(emergency_command);
  // }
}

// --- Handlers for Sensor Events ---

void VehicleClientApp::handleCameraFrameCaptured(
    /* frame data */) {  // Needs actual frame data type, e.g., const FrameData&
  // std::cout << "App: Camera frame captured (placeholder)." << std::endl;
  // TODO: Send the frame data via WebRTC video track(s).
  // This requires access to the WebRTC PeerConnection(s) and video track
  // sender(s). The camera source might push directly into a WebRTC track
  // source, or this handler receives the frame and manually pushes it.

  // Example (Conceptual):
  // if (webrtcManager_) {
  //     webrtcManager_->sendVideoFrameToAllPeers(frame_data); // Needs
  //     implementation in WebrtcManager
  // }
}

void VehicleClientApp::handleChassisStateUpdated(
    const autodev::remote::chassis::Chassis& state) {
  // std::cout << "App: Chassis state updated (placeholder): Speed=" <<
  // state.speed_mps() << std::endl;
  // TODO: Serialize the Protobuf message
  std::vector<char> serialized_data(state.ByteSizeLong());
  // if (!state.SerializeToArray(serialized_data.data(),
  // serialized_data.size())) {
  //     std::cerr << "App: Failed to serialize Chassis state." << std::endl;
  //     // TODO: Handle serialization error
  //     return;
  // }

  // Send serialized data via the telemetry DataChannel to all connected peers
  if (webrtcManager_) {
    // Use a defined constant for the data channel label
    const std::string telemetry_channel_label = "telemetry";
    // webrtcManager_->sendDataChannelMessageToAllPeers(telemetry_channel_label,
    // serialized_data); // Needs implementation
  }
  // Placeholder print for state
  // std::cout << "App: Chassis state updated (placeholder)." << std::endl;
}

// --- Handlers for ConnectionMonitor Events ---

void VehicleClientApp::handleNetworkUp(const std::string& peer_id) {
  std::cout << "App: Network is UP with peer " << peer_id << std::endl;
  // TODO: Application logic upon network up (e.g., enable driving capability,
  // switch control modes) Inform controller? Enable a flag?
}

void VehicleClientApp::handleNetworkDown(const std::string& peer_id,
                                         const std::string& reason) {
  std::cerr << "App: Network is DOWN with peer " << peer_id
            << ". Reason: " << reason << std::endl;
  // TODO: Application logic upon network down (e.g., disable driving
  // capability, activate safety stop) This is critical! Trigger emergency stop.
  if (controller_) {
    autodev::remote::control::EmergencyCommand emergency_command;
    emergency_command.set_type(
        autodev::remote::control::EMERGENCY_STOP);  // Assuming EMERGENCY_STOP
                                                    // is a valid type
    // emergency_command.set_reason("Network down: " + reason); // Add reason
    controller_->processEmergencyCommand(emergency_command);
  }
}

void VehicleClientApp::handleHeartbeatLost(const std::string& peer_id) {
  std::cerr << "App: Heartbeat lost from peer " << peer_id << std::endl;
  // TODO: Trigger a safety action. Heartbeat loss is a strong indicator of
  // connectivity issue. Typically leads to a safety stop.
  if (controller_) {
    autodev::remote::control::EmergencyCommand emergency_command;
    emergency_command.set_type(
        autodev::remote::control::EMERGENCY_STOP);  // Assuming EMERGENCY_STOP
                                                    // is a valid type
    // emergency_command.set_reason("Heartbeat lost from peer " + peer_id); //
    // Add reason
    controller_->processEmergencyCommand(emergency_command);
  }
}

}  // namespace vehicle
}  // namespace remote
}  // namespace autodev

// --- Main Application Entry Point ---

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <config_file_path>" << std::endl;
    return 1;
  }

  std::string config_path = argv[1];

  // 1. Load Configuration (Done outside the App for dependency injection)
  autodev::remote::config::JsonConfigLoader config_loader;
  auto loaded_config = config_loader.loadConfig(config_path);
  if (!loaded_config) {
    std::cerr << "Failed to load configuration from " << config_path
              << std::endl;
    return 1;
  }
  autodev::remote::vehicle::VehicleConfig app_config = *loaded_config;
  std::cout << "Configuration loaded successfully." << std::endl;

  // 2. Create Concrete Component Instances (Done outside the App)
  // These would ideally be based on the configuration (e.g., config says use
  // V4L2, Apollo, etc.) Using concrete classes directly for this example.
  auto webrtc_manager =
      std::make_unique<autodev::remote::webrtc::WebrtcManagerImpl>();
  auto controller =
      std::make_unique<autodev::remote::control::ApolloController>();
  auto camera_source =
      std::make_unique<autodev::remote::sensors::V4L2CameraSource>();
  auto chassis_source =
      std::make_unique<autodev::remote::sensors::CanBusChassisSource>();

  // Create Connection Monitor only if heartbeat is configured
  std::unique_ptr<autodev::remote::network_manager::IConnectionMonitor>
      connection_monitor = nullptr;
  if (app_config.heartbeat_interval_ms > 0) {
    // DANGER: Passing webrtc_manager.get() requires careful lifetime
    // management. The monitor *must* be stopped before the webrtc_manager is
    // destroyed. This is handled in VehicleClientApp::stop().
    connection_monitor = std::make_unique<
        autodev::remote::network_manager::ConnectionMonitorImpl>(
        webrtc_manager.get(),
        app_config.heartbeat_interval_ms);  // Assuming MonitorImpl takes raw
                                            // pointer and interval
    std::cout << "Connection Monitor created." << std::endl;
  } else {
    std::cout << "Heartbeat interval <= 0, Connection Monitor not created."
              << std::endl;
  }

  // 3. Create and Initialize the Application with Dependencies
  autodev::remote::vehicle::VehicleClientApp app;

  if (!app.init(app_config, std::move(webrtc_manager), std::move(controller),
                std::move(camera_source), std::move(chassis_source),
                std::move(connection_monitor)))  // Pass optional monitor
  {
    std::cerr << "Failed to initialize vehicle client application."
              << std::endl;
    return 1;
  }

  std::cout << "Vehicle client initialized. Running..." << std::endl;

  // 4. Run the Application (delegates to event loop)
  int return_code = app.run();

  std::cout << "Vehicle client stopped." << std::endl;

  return return_code;
}
