#ifndef WEB_COMMAND_HANDLER_H
#define WEB_COMMAND_HANDLER_H

#include <memory>  // For shared_ptr to WebrtcManager
#include <string>
#include <vector>

// Include the interface for the WebRTC manager (dependency)
#include "webrtc/webrtc_manager.h"

// Include generated protobuf headers for command types
// Note: The handler implementation will use these internally after parsing raw
// data. The interface itself doesn't expose protobuf types directly in its
// primary process method. #include "control/proto/control_command.pb.h"
// #include "control/proto/emergency_command.pb.h"

// Assuming WebSocketConnectionId is defined elsewhere, e.g., int or uint32_t
// Or define a placeholder here if it's only used in this interface
using WebSocketConnectionId =
    uint32_t;  // Example: Must match definition in transport/transport_server.h

// Use namespace for better organization
namespace autodev {
namespace remote {
namespace drivers {

// Interface for handling raw command data received from the local Web UI via
// WebSocket. Responsible for parsing raw input (e.g., JSON), converting to
// structured commands (Protobuf), and sending them to the vehicle via the
// WebrtcManager.
class IWebCommandHandler {
 public:
  virtual ~IWebCommandHandler() = default;

  // Initializes the handler.
  // Needs access to the WebrtcManager to send commands to the vehicle.
  // Also needs configuration parameters to know where/how to send commands.
  // The handler implementation should store the provided shared_ptr and config.
  //
  // webrtc_manager: Shared pointer to the WebrtcManager instance.
  // control_channel_label: The DataChannel label for sending control commands.
  // target_peer_id: The ID of the vehicle peer to send commands to.
  // Returns true on success, false on failure.
  virtual bool init(
      std::shared_ptr<autodev::remote::webrtc::WebrtcManager> webrtc_manager,
      const std::string& control_channel_label,
      const std::string& target_peer_id) = 0;

  // Processes raw command data received from a local WebSocket.
  // Implementations should:
  // 1. Parse the raw_message_data (e.g., deserialize JSON).
  // 2. Convert the parsed data into structured command types (e.g.,
  // ControlCommand, EmergencyCommand).
  // 3. Serialize the structured command into a byte vector (e.g., Protobuf
  // serialization).
  // 4. Send the serialized command to the target vehicle using the
  // WebrtcManager
  //    via the configured control_channel_label.
  //
  // conn_id: Identifier of the WebSocket connection the message arrived on.
  //          Useful for logging, potentially sending feedback back to the
  //          specific client.
  // raw_message_data: The raw data (e.g., JSON string) from the WebSocket.
  //
  // This method will likely be called from a TransportServer internal thread
  // (e.g., a WebSocket handler thread). Implementations MUST be thread-safe.
  // The method should ideally be non-blocking or have a very short execution
  // time.
  virtual void processRawWebCommand(
      WebSocketConnectionId conn_id,
      const std::vector<char>& raw_message_data) = 0;

  // Optional: Add an error handler callback from the handler (e.g., for parsing
  // failures). using OnErrorHandler = std::function<void(const std::string&
  // error_msg)>; virtual void setOnErrorHandler(OnErrorHandler handler) = 0;
};

}  // namespace drivers
}  // namespace remote
}  // namespace autodev

#endif  // WEB_COMMAND_HANDLER_H
