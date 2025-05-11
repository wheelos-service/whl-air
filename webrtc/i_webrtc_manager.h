#ifndef I_WEBRTC_MANAGER_H
#define I_WEBRTC_MANAGER_H

#include <functional>  // For callbacks
#include <memory>      // For shared_ptr if needed by callbacks
#include <string>
#include <vector>

// Forward declare necessary types if not included fully
// struct VideoFrame; // If video handling is included in interface

namespace autodev {
namespace remote {
namespace webrtc {

// Define types for DataChannel messages
using DataChannelMessage = std::vector<char>;

// Define common WebRTC states (simplified example, use libwebrtc enums in impl)
enum class PeerConnectionState {
  New,
  Connecting,
  Connected,
  Disconnected,
  Failed,
  Closed
};
enum class IceConnectionState {
  New,
  Checking,
  Connected,
  Completed,
  Failed,
  Disconnected,
  Closed
};

// Interface for managing WebRTC connections (Signaling and PeerConnections).
// This interface is used by the application (VehicleClientApp or
// CockpitClientApp).
class IWebrtcManager {
 public:
  virtual ~IWebrtcManager() = default;

  // --- Application-level Callbacks (Reported up to the App) ---
  // These handlers are set by the application and called by the WebrtcManager.
  // Implementations MUST be thread-safe if the WebrtcManager calls them from
  // background threads.

  // Called when the signaling connection is established.
  using OnSignalingConnectedHandler = std::function<void()>;
  // Called when the signaling connection is lost.
  using OnSignalingDisconnectedHandler =
      std::function<void(const std::string& reason)>;
  // Called when a signaling error occurs.
  using OnSignalingErrorHandler =
      std::function<void(const std::string& error_msg)>;

  // Called when a peer connection is successfully established (ICE connected
  // and DataChannels ready). peer_id: The ID of the remote peer.
  using OnPeerConnectedHandler =
      std::function<void(const std::string& peer_id)>;
  // Called when a peer connection is lost (ICE disconnected/failed/closed or
  // explicit signal). peer_id: The ID of the remote peer. reason: Description
  // of the disconnection reason.
  using OnPeerDisconnectedHandler = std::function<void(
      const std::string& peer_id, const std::string& reason)>;
  // Called when a significant error occurs related to a specific peer
  // connection.
  using OnPeerErrorHandler = std::function<void(const std::string& peer_id,
                                                const std::string& error_msg)>;

  // Called when a message is received via DataChannel.
  // peer_id: The sender peer.
  // label: The label of the DataChannel the message was received on.
  // message: The raw received data (e.g., serialized protobuf).
  // Implementations of this handler MUST BE THREAD-SAFE.
  using OnDataChannelMessageReceivedHandler =
      std::function<void(const std::string& peer_id, const std::string& label,
                         const DataChannelMessage& message)>;

  // Optional: Called when a video track is received from a peer (Cockpit side).
  // peer_id: The sender peer.
  // track: The WebRTC video track object. Needs WebRTC headers to define the
  // type. using OnVideoTrackReceivedHandler = std::function<void(const
  // std::string& peer_id, rtc::scoped_refptr<webrtc::VideoTrackInterface>
  // track)>;

  // --- Manager Lifecycle ---

  // Initializes internal components (signaling client, libwebrtc factory etc.)
  // and starts the signaling connection process. This is asynchronous.
  // Connection status will be reported via callbacks.
  // config: Configuration specific to WebRTC and signaling.
  // Returns true if initialization was successful, false otherwise.
  // This replaces the constructor's configuration role.
  // TODO: Needs Event Loop Context/Task Queue here for callbacks/tasks
  // virtual bool init(const WebrtcConfig& config, SomeEventLoopContext*
  // event_loop) = 0; Using a placeholder config for now.
  virtual bool init(
      /* const WebRTCConfig& config, EventLoopContext* event_loop */) = 0;

  // Starts all network activity (e.g., connect signaling, enable PC
  // monitoring). Must be called after init().
  virtual bool start() = 0;

  // Stops all network activity, disconnects from signaling, closes all
  // PeerConnections. Should block until all related threads/tasks are shut
  // down. Safe to call even if not started or already stopped.
  virtual void stop() = 0;

  // --- Application Interaction (called by the App) ---

  // Connects to a specific peer (initiates the WebRTC connection flow).
  // Typically used by the Cockpit to connect to a specific Vehicle.
  // Vehicle side might auto-accept connections or connect back based on signal.
  // Returns true if the process was initiated successfully.
  virtual bool connectToPeer(const std::string& peer_id) = 0;

  // Disconnects from a specific peer.
  // Returns true if the peer connection was found and closure was initiated.
  virtual bool disconnectFromPeer(const std::string& peer_id,
                                  const std::string& reason) = 0;

  // Sends a message over a specific DataChannel to a specific peer.
  // peer_id: The target peer.
  // channel_label: The label of the DataChannel (e.g., "control", "telemetry").
  // data: The data to send (serialized protobuf bytes).
  // Returns true if message was queued/sent, false if peer or channel is not
  // ready. This method MUST BE THREAD-SAFE.
  virtual bool sendDataChannelMessage(const std::string& peer_id,
                                      const std::string& channel_label,
                                      const DataChannelMessage& data) = 0;

  // Optional: Sends binary data to ALL active PeerConnections on a specific
  // DataChannel. Useful for broadcasting telemetry or status updates from
  // Vehicle to multiple Cockpits. This method MUST BE THREAD-SAFE.
  virtual bool sendDataChannelMessageToAllPeers(
      const std::string& channel_label, const DataChannelMessage& data) = 0;

  // Optional: Add a video track for sending (Vehicle side).
  // track: The WebRTC video track object (created by the vehicle application,
  // e.g., from camera source). Returns true if the track was added successfully
  // to all future/existing PeerConnections. virtual bool
  // addLocalVideoTrack(rtc::scoped_refptr<webrtc::VideoTrackInterface> track) =
  // 0;

  // Optional: Remove a video track for sending (Vehicle side).
  // virtual void
  // removeLocalVideoTrack(rtc::scoped_refptr<webrtc::VideoTrackInterface>
  // track) = 0;

  // --- Registering Application Callbacks with the Manager ---
  // These methods should be called after init() and before start().
  // Passing a default-constructed std::function clears the handler.

  virtual void onSignalingConnected(OnSignalingConnectedHandler handler) = 0;
  virtual void onSignalingDisconnected(
      OnSignalingDisconnectedHandler handler) = 0;
  virtual void onSignalingError(OnSignalingErrorHandler handler) = 0;
  virtual void onPeerConnected(OnPeerConnectedHandler handler) = 0;
  virtual void onPeerDisconnected(OnPeerDisconnectedHandler handler) = 0;
  virtual void onPeerError(OnPeerErrorHandler handler) = 0;
  virtual void onDataChannelMessageReceived(
      OnDataChannelMessageReceivedHandler handler) = 0;
  // Optional: virtual void onVideoTrackReceived(OnVideoTrackReceivedHandler
  // handler) = 0;
};

}  // namespace webrtc
}  // namespace remote
}  // namespace autodev

#endif  // I_WEBRTC_MANAGER_H
