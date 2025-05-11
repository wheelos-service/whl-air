#ifndef VEHICLE_CLIENT_APP_H
#define VEHICLE_CLIENT_APP_H

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "config/config_loader.h"
#include "config/vehicle_config.h"
#include "control/controller.h"
#include "sensors/camera.h"
#include "sensors/chassis.h"
#include "webrtc/webrtc_manager.h"

namespace autodev {
namespace remote {
namespace vehicle {

// Define application states
enum class AppState {
  Uninitialized,
  Initializing,
  Initialized,
  Running,
  Stopping,
  Stopped
};

class VehicleClientApp {
 public:
  // Constructor - should be lightweight
  VehicleClientApp();

  // Destructor - ensures cleanup
  virtual ~VehicleClientApp();

  // Initializes the application with loaded configuration and dependencies.
  // Returns true on success, false on failure.
  bool init(const VehicleConfig& config,
            std::unique_ptr<WebrtcManager> webrtcManager,
            std::unique_ptr<IController> controller,
            std::unique_ptr<ICameraSource> cameraSource,
            std::unique_ptr<IChassisSource> chassisSource);

  // Runs the main application loop.
  // This method should block until stop() is called or an error occurs.
  // Returns 0 on graceful exit, non-zero on error.
  int run();

  // Stops the application gracefully. Can be called multiple times.
  void stop();

 private:
  // Application State
  AppState state_ = AppState::Uninitialized;

  // Configuration
  VehicleConfig config_;

  // Core components - owned by the app
  std::unique_ptr<WebrtcManager> webrtcManager_;
  std::unique_ptr<IController> controller_;
  std::unique_ptr<ICameraSource> cameraSource_;
  std::unique_ptr<IChassisSource> chassisSource_;

  // Internal setup methods (now simpler due to dependency injection)
  bool setupWebrtcManager();
  bool setupController();
  bool setupSensors();

  // Handlers for WebrtcManager events
  void handlePeerConnected(const std::string& peer_id);
  void handlePeerDisconnected(const std::string& peer_id,
                              const std::string& reason);
  void handleControlMessageReceived(const std::string& peer_id,
                                    const std::vector<char>& message);
  void handleTelemetryMessageReceived(const std::string& peer_id,
                                      const std::vector<char>& message);
  void handleWebrtcError(const std::string& error_msg);

  // Handlers for Sensor events
  void handleCameraFrameCaptured(
      /* frame data */);  // Needs actual frame data type
  void handleChassisStateUpdated(const autodev::remote::chassis::Chassis&
                                     state);  // Assume Protobuf message

  // Handlers for ConnectionMonitor events (if separate)
  void handleNetworkUp(const std::string& peer_id);
  void handleNetworkDown(const std::string& peer_id, const std::string& reason);
  void handleHeartbeatLost(const std::string& peer_id);

  // Prevent copying
  VehicleClientApp(const VehicleClientApp&) = delete;
  VehicleClientApp& operator=(const VehicleClientApp&) = delete;
};

}  // namespace vehicle
}  // namespace remote
}  // namespace autodev

#endif  // VEHICLE_CLIENT_APP_H
