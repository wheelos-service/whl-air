#ifndef TELEMETRY_HANDLER_H
#define TELEMETRY_HANDLER_H

#include <memory>  // For shared_ptr to TransportServer
#include <string>
#include <vector>

#include "transport/transport_server.h"

// Include the generated protobuf header for telemetry data
// Ensure this path is correct for your build system
#include "chassis/proto/chassis.pb.h"  // Example: For Chassis telemetry

// Use namespace for better organization
namespace autodev {
namespace remote {
namespace drivers {

// Interface for handling incoming telemetry data from the vehicle
// Responsible for receiving deserialized data and forwarding it to the UI
// via the TransportServer.
class ITelemetryHandler {
 public:
  virtual ~ITelemetryHandler() = default;

  // Initializes the handler.
  // Needs access to the TransportServer to send data to the UI.
  // The handler implementation should store the provided shared_ptr.
  // Returns true on success, false on failure.
  virtual bool init(
      std::shared_ptr<autodev::remote::transport::ITransportServer>
          transport_server) = 0;

  // Optional: Method to handle incoming video tracks from the vehicle
  // Implementations should attach a VideoSink to this track to get frames.
  // This method will be called from a WebRTC internal thread; MUST be
  // thread-safe.
  virtual void addVehicleVideoTrack(
      const std::string& peer_id,
      rtc::scoped_refptr<webrtc::VideoTrackInterface> track) = 0;

  // Processes a deserialized telemetry message received from the vehicle via
  // WebRTC DataChannel.
  // This method is the entry point for incoming telemetry data that has already
  // been parsed (e.g., from a Protobuf message).
  // Implementations should format this data (e.g., convert to JSON) and send it
  // to one or more connected UI clients via the ITransportServer.
  //
  // peer_id: The ID of the peer (vehicle) the telemetry came from. Useful if
  //          handling multiple vehicles or for logging.
  // telemetry_data: The deserialized Chassis protobuf message.
  //
  // This method will likely be called from a WebRTC internal thread (e.g., the
  // data channel thread). Implementations MUST be thread-safe.
  // The method should ideally be non-blocking or have a very short execution
  // time.
  virtual void processIncomingTelemetry(
      const std::string& peer_id,
      const autodev::remote::chassis::Chassis& telemetry_data) = 0;

  // Optional: Add specific methods for processing other types of telemetry,
  // or use a generic wrapper message if handling multiple types.
  // virtual void processIncomingSensorData(const std::string& peer_id, const
  // autodev::remote::sensors::SensorData& data) = 0;

  // Optional: Notify UI about connection status changes
  // Implementations would use the TransportServer to send a status message to
  // the UI. virtual void notifyConnectionStatus(const std::string& peer_id,
  // bool is_connected) = 0;

  // Optional: Add an error handler callback from the TelemetryHandler itself?
  // (Less common) using OnErrorHandler = std::function<void(const std::string&
  // error_msg)>; virtual void setOnErrorHandler(OnErrorHandler handler) = 0;
};

}  // namespace drivers
}  // namespace remote
}  // namespace autodev

#endif  // TELEMETRY_HANDLER_H
