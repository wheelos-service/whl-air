#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <string>

#include "control/proto/control_command.pb.h"
#include "control/proto/emergency_command.pb.h"

// Forward declare configuration struct if defined elsewhere
// struct ControllerConfig;

// Placeholder for ActuatorState struct/message
// In a real system, this would be a specific struct or Protobuf message
namespace autodev {
namespace remote {
namespace control {

struct ActuatorState {
  // Define actuator state members here (e.g., current speed, steering angle,
  // throttle, brake)
  double current_speed_mps = 0.0;
  double steering_angle_rad = 0.0;
  // ... other relevant states
};

// Placeholder for ControllerConfig struct/message
// In a real system, this would be a specific struct or Protobuf message
struct ControllerConfig {
  // Define controller configuration parameters here (e.g., PID gains, vehicle
  // parameters)
  double max_speed_mps = 10.0;
  double max_steering_angle_rad = 0.5;  // Example
                                        // ... other relevant config
};

}  // namespace control
}  // namespace remote
}  // namespace autodev

// Interface for vehicle control execution
// Placed in the appropriate namespace
namespace autodev {
namespace remote {
namespace control {

class IController {
 public:
  virtual ~IController() = default;

  // Initializes the controller with necessary configuration.
  // This method should be called after object creation and before processing
  // commands. Returns true on success, false on failure.
  virtual bool init(const ControllerConfig& config) = 0;

  // Processes a standard control command (e.g., from joystick/gamepad).
  // This method is responsible for converting the command to actuator signals
  // and sending them to the vehicle's low-level control interfaces.
  // This method may be called from a different thread than the main application
  // thread (e.g., a data channel handler thread). Implementations must be
  // thread-safe. The method should ideally be non-blocking or have a very short
  // execution time.
  virtual void processControlCommand(
      const autodev::remote::control::ControlCommand& command) = 0;

  // Processes an emergency control command.
  // These commands should have higher priority and potentially trigger safety
  // systems regardless of the current state or other pending commands.
  // This method may be called from a different thread. Implementations must be
  // thread-safe.
  virtual void processEmergencyCommand(
      const autodev::remote::control::EmergencyCommand& command) = 0;

  // Optional: Method to get the current state of actuators if needed for
  // feedback/telemetry. This should be thread-safe if the controller updates
  // its state asynchronously.
  virtual ActuatorState getActuatorState() const = 0;

  // Optional: Add a method to reset the controller state or clear commands
  // virtual void reset() = 0;
};

}  // namespace control
}  // namespace remote
}  // namespace autodev

#endif  // CONTROLLER_H
