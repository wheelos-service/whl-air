#include <iostream>
#include <string>

#include "include/drivers/telemetry_handler.h"
#include "include/transport/transport_server.h"  // To send to Web UI

// Include protobuf header
// #include "proto/chassis.pb.h"

// Include JSON library (to serialize protobuf to JSON for JS)
// #include <nlohmann/json.hpp>
// using json = nlohmann::json;

class TelemetryHandler : public ITelemetryHandler {
 public:
  TelemetryHandler() {
    std::cout << "TelemetryHandler created (skeleton)" << std::endl;
  }

  bool init(std::shared_ptr<ITransportServer> transport_server) override {
    if (!transport_server) {
      std::cerr << "TelemetryHandler: TransportServer is null." << std::endl;
      return false;
    }
    transportServer_ = transport_server;
    std::cout << "TelemetryHandler: Initialized." << std::endl;
    return true;
  }

  void processIncomingTelemetry(
      const std::string& peer_id,
      const autodev::remote::chassis::Chassis& telemetry_data) override {
    // std::cout << "TelemetryHandler: Processing telemetry from " << peer_id <<
    // std::endl;
    // TODO: Serialize telemetry_data (Protobuf) to a format suitable for
    // WebSocket (e.g., JSON) Example using nlohmann/json: json j;
    // // TODO: Populate JSON from protobuf fields
    // // j["speed_mps"] = telemetry_data.speed_mps();
    // // j["gear"] = telemetry_data.gear();
    // // ... map all relevant fields

    // std::string json_string = j.dump();

    // Send the JSON string to all connected WebSocket clients (the display)
    // Need a way to get active WebSocket connection IDs from the server.
    // Or, the transport server could have a broadcast method.
    // For simplicity, assuming a single display client connection with ID 1.

    // --- Skeleton implementation ---
    std::string dummy_json =
        "{ \"speed_mps\": " + std::to_string(telemetry_data.speed_mps()) +
        ", \"gear\": " + std::to_string(telemetry_data.gear()) + " }";
    std::cout << "TelemetryHandler: Sending telemetry (skeleton) to WS conn 1: "
              << dummy_json << std::endl;
    transportServer_->sendWebSocketMessage(
        1, dummy_json);  // Assuming conn_id 1 for the display
  }

 private:
  std::shared_ptr<ITransportServer> transportServer_;

  // Prevent copying
  TelemetryHandler(const TelemetryHandler&) = delete;
  TelemetryHandler& operator=(const TelemetryHandler&) = delete;
};
