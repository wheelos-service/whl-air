#ifndef VEHICLE_CONFIG_H
#define VEHICLE_CONFIG_H

#include <cstdint>
#include <string>
#include <vector>

struct WebRtcServerConfig {
  std::string uri;  // Signaling server URI
  std::string jwt;  // Optional JWT
};

struct SensorConfig {
  std::string camera_device;  // e.g., "/dev/video0"
  uint32_t camera_width;
  uint32_t camera_height;
  uint32_t camera_fps;
  // Add config for chassis source if needed (e.g., CAN bus interface)
  std::string can_interface;  // e.g., "can0"
};

// Structure to hold all configuration parameters for the vehicle client
struct VehicleConfig {
  WebRtcServerConfig signaling;
  SensorConfig sensors;
  std::string client_id;  // Unique ID for this vehicle client

  // DataChannel labels (should match remote driver client)
  std::string control_channel_label = "control";
  std::string telemetry_channel_label = "telemetry";

  // WebRTC ICE server configuration (STUN/TURN)
  struct IceServer {
    std::string uri;
    std::string username;  // for TURN
    std::string password;  // for TURN
  };
  std::vector<IceServer> ice_servers;

  // Add other configurations as needed (e.g., logging levels, heartbeat
  // intervals)
  int heartbeat_interval_ms = 5000;  // milliseconds
};

#endif  // VEHICLE_CONFIG_H
