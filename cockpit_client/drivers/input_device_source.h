#ifndef INPUT_DEVICE_SOURCE_H
#define INPUT_DEVICE_SOURCE_H

#include <functional>  // For optional callbacks
#include <memory>      // For shared_ptr to WebrtcManager
#include <string>
#include <vector>

// Include the interface for the WebRTC manager (dependency)
#include "webrtc/webrtc_manager.h"

// Include generated protobuf headers for command types produced by this source
#include "control/proto/control_command.pb.h"
#include "control/proto/emergency_command.pb.h"

// Forward declare configuration struct if defined elsewhere
// struct InputDeviceConfig;

// Use namespace for better organization
namespace autodev {
namespace remote {
namespace drivers {  // Or sensors, depending on how input devices are
                     // categorized

// Interface for a physical input device source (e.g., steering wheel,
// joystick). This source reads device state, converts it into structured
// commands (Protobuf), and sends them directly to the vehicle via the
// WebrtcManager. It acts like a sensor source, but produces commands instead of
// telemetry.
class IInputDeviceSource {
 public:
  virtual ~IInputDeviceSource() = default;

  // Initializes the input device source.
  // Needs access to the WebrtcManager to send commands to the vehicle.
  // Also needs configuration parameters to specify the device or its settings.
  // The implementation should store the provided shared_ptr and config.
  //
  // webrtc_manager: Shared pointer to the WebrtcManager instance.
  // control_channel_label: The DataChannel label for sending control commands.
  // target_peer_id: The ID of the vehicle peer to send commands to.
  // config: Configuration specific to the input device (e.g., device node
  // path). Returns true on success, false on failure.
  virtual bool init(
      std::shared_ptr<autodev::remote::webrtc::WebrtcManager> webrtc_manager,
      const std::string& control_channel_label,
      const std::string& target_peer_id,
      /* const InputDeviceConfig& config */) = 0;

  // Starts the input device polling and command sending loop.
  // Implementations typically run a background thread or integrate with an
  // event loop to read device state periodically or when events occur, convert
  // to commands, and send via the WebrtcManager. Returns true on success (e.g.,
  // polling thread started). Must be called after init(). If polling is already
  // active, calling this might return false.
  virtual bool startPolling() = 0;

  // Stops the input device polling and command sending loop.
  // This method should block until the polling thread (if any) has stopped.
  // Safe to call even if polling is not active or already stopped.
  virtual void stopPolling() = 0;

  // Optional: Add an error handler callback from the source (e.g., device
  // disconnect). using OnErrorHandler = std::function<void(const std::string&
  // error_msg)>; virtual void setOnErrorHandler(OnErrorHandler handler) = 0;
};

}  // namespace drivers
}  // namespace remote
}  // namespace autodev

#endif  // INPUT_DEVICE_SOURCE_H
