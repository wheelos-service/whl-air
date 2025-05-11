#include "webrtc_manager_impl.h"  // Include the implementation header

// Include concrete implementations (linked, not included as .cc)
// #include "signaling/websocket_signaling_client.h" // Concrete signaling
// client #include "webrtc/libwebrtc_peer_connection.h" // Concrete
// PeerConnection implementation

// Include other necessary headers
#include "signaling/signaling_message.h"  // SignalMessage definition
// #include "config/webrtc_config.h" // Include actual config struct if used
// #include "event_loop/event_loop_context.h" // Include event loop context
// header #include "webrtc/rtc_base/thread.h" // For libwebrtc thread management
// #include "webrtc/api/peer_connection_interface.h" // For
// PeerConnectionFactoryInterface

#include <algorithm>  // For std::find_if
#include <chrono>     // For std::chrono
#include <iostream>
#include <string>
#include <thread>   // For std::this_thread::sleep_for (if simulating timers)
#include <utility>  // For std::move
#include <vector>   // For std::vector

// Dummy config placeholder if actual config struct is not included
namespace autodev {
namespace remote {
namespace config {
struct VehicleConfig {};
}  // namespace config
}  // namespace remote
}  // namespace autodev

namespace autodev {
namespace remote {
namespace webrtc {

// --- WebrtcManagerImpl Implementation ---

WebrtcManagerImpl::WebrtcManagerImpl() : state_(AppState::Uninitialized) {
  std::cout << "WebrtcManagerImpl created." << std::endl;
  // libwebrtc initialization and thread creation should ideally happen BEFORE
  // this constructor, managed by the application or a dedicated wrapper,
  // and the factory/context passed into init().
  // TODO: If WebrtcManagerImpl OWNS the libwebrtc factory/threads, initialize
  // them here or in init().
}

WebrtcManagerImpl::~WebrtcManagerImpl() {
  std::cout << "WebrtcManagerImpl destroying..." << std::endl;
  // Ensure stop is called to clean up resources managed by this class
  stop();
  // TODO: If WebrtcManagerImpl OWNS the libwebrtc factory/threads, shut them
  // down here.
  std::cout << "WebrtcManagerImpl destroyed." << std::endl;
}

// Implementation of IWebrtcManager::init
// TODO: Add parameters: const WebrtcConfig& webrtc_config, EventLoopContext*
// event_loop, PeerConnectionFactory* factory
bool WebrtcManagerImpl::init(/* const WebrtcConfig& webrtc_config, EventLoopContext* event_loop, PeerConnectionFactory* factory */) {
  AppState expected = AppState::Uninitialized;
  if (!state_.compare_exchange_strong(expected, AppState::Initializing)) {
    std::cerr
        << "WebrtcManagerImpl: Already initialized or in a different state."
        << std::endl;
    return false;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "WebrtcManagerImpl: Initializing..." << std::endl;

  // TODO: Store config_ = webrtc_config;
  // TODO: Store event_loop_ = event_loop;
  // TODO: Store factory_ = factory; // Store libwebrtc factory

  // TODO: Create the concrete signaling client implementation
  // It needs config and callbacks, potentially the event loop context
  // signalingClient_ = std::make_unique<WebsocketSignalingClient>(
  //    config_.signaling_uri, config_.signaling_jwt, event_loop_, this); //
  //    Pass config, context, and 'this' as handler sink

  // Dummy creation for skeleton
  signalingClient_ = std::make_unique<SignalingClientImpl>(
      "ws://dummy", "", this);  // Dummy config for skeleton

  // Set handlers for the signaling client (Callbacks are implemented below)
  // These handlers will be called by the signaling client's thread; they must
  // acquire mutex_.
  signalingClient_->onConnected(
      std::bind(&WebrtcManagerImpl::handleSignalingConnected, this));
  signalingClient_->onDisconnected(
      std::bind(&WebrtcManagerImpl::handleSignalingDisconnected, this));
  signalingClient_->onError(std::bind(&WebrtcManagerImpl::handleSignalingError,
                                      this, std::placeholders::_1));
  signalingClient_->onMessageReceived(std::bind(
      &WebrtcManagerImpl::handleSignalingMessage, this, std::placeholders::_1));

  // TODO: Initialize heartbeat timer if enabled in config
  // if (config_.heartbeat_interval_ms > 0) {
  //     startHeartbeatTimer(); // Start the timer (implementation needs
  //     event_loop_)
  // }

  state_ = AppState::Initialized;
  std::cout << "WebrtcManagerImpl: Initialization successful." << std::endl;
  return true;
}

// Implementation of IWebrtcManager::start
bool WebrtcManagerImpl::start() {
  AppState expected = AppState::Initialized;
  if (!state_.compare_exchange_strong(expected, AppState::Running)) {
    std::cerr << "WebrtcManagerImpl: Cannot start, not in Initialized state."
              << std::endl;
    return false;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "WebrtcManagerImpl: Starting..." << std::endl;

  if (!signalingClient_) {
    std::cerr << "WebrtcManagerImpl: Signaling client not initialized."
              << std::endl;
    state_ = AppState::Stopped;  // Cannot run without signaling
    return false;
  }

  // Connect the signaling client - this is typically asynchronous
  signalingClient_->connect();
  std::cout << "WebrtcManagerImpl: start completed." << std::endl;
  return true;
}

// Implementation of IWebrtcManager::stop
void WebrtcManagerImpl::stop() {
  AppState expected = AppState::Running;
  if (!state_.compare_exchange_strong(expected, AppState::Stopping)) {
    expected = AppState::Initialized;  // Allow stopping from Initialized state
    if (!state_.compare_exchange_strong(expected, AppState::Stopping)) {
      std::cout << "WebrtcManagerImpl: Already stopping, stopped, or "
                   "uninitialized. Skipping stop."
                << std::endl;
      return;
    }
  }

  // Acquire lock while stopping resources managed by this class
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "WebrtcManagerImpl: Stopping..." << std::endl;

  // TODO: Stop heartbeat timer
  // stopHeartbeatTimer(); // Implementation needs event_loop_

  // Close all peer connections managed by this manager
  std::cout << "WebrtcManagerImpl: Closing " << peerConnections_.size()
            << " peer connections." << std::endl;
  for (auto const& [peer_id, pc] : peerConnections_) {
    if (pc) {
      pc->Close();  // This should trigger PeerConnectionState::kClosed/Failed
                    // callbacks
    }
  }
  // Clear the map to release unique_ptrs. This must happen AFTER Close()
  // is called and potential cleanup callbacks finish, or
  // PeerConnection::Close() is blocking until cleanup. Be careful with thread
  // synchronization here. A common pattern is to clear the map only after the
  // WebRTC signaling thread is stopped or PeerConnection callbacks have a safe
  // way to check manager validity. For simplicity in skeleton, clear after
  // signaling disconnect.
  peerConnections_.clear();  // Release unique_ptrs

  // Disconnect signaling client - this is typically asynchronous
  if (signalingClient_) {
    signalingClient_->disconnect();
    // After signaling disconnect, the signaling client should stop calling
    // handlers.
  }

  state_ = AppState::Stopped;  // Final state
  std::cout << "WebrtcManagerImpl: stop completed." << std::endl;
}

// Implementation of IWebrtcManager::connectToPeer
bool WebrtcManagerImpl::connectToPeer(const std::string& peer_id) {
  // This method is called by the application thread. ACQUIRE mutex_.
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "WebrtcManagerImpl: Attempting to connect to peer: " << peer_id
            << std::endl;

  // Check if state is Running
  if (state_ != AppState::Running) {
    std::cerr
        << "WebrtcManagerImpl: Cannot connect to peer, manager is not running."
        << std::endl;
    return false;
  }

  // Check if peer connection already exists
  if (peerConnections_.count(peer_id)) {
    std::cout << "WebrtcManagerImpl: Peer connection to " << peer_id
              << " already exists." << std::endl;
    return true;  // Or false, depending on policy
  }

  // Create a new PeerConnection instance
  // This call to getOrCreatePeerConnection also acquires the mutex internally
  // or uses the same mutex if called from within the lock.
  // Let's call createPeerConnection directly and then add to map.
  std::cout << "WebrtcManagerImpl: Creating new PeerConnection for outbound "
               "connection to "
            << peer_id << std::endl;

  // Prepare callbacks for the new PC
  PeerConnectionCallbacks pc_callbacks;
  // Use lambdas capturing peer_id and 'this'. Ensure 'this' is valid when
  // lambda is called. Using a shared_ptr to the manager might be safer if
  // callbacks can outlive the manager temporarily. For now, rely on the
  // manager's stop() ensuring callbacks are not called after destruction.
  pc_callbacks.onLocalSdpGenerated = [this, peer_id](
                                         const std::string& sdp_type,
                                         const std::string& sdp_string) {
    handlePeerLocalSdpGenerated(peer_id, sdp_type,
                                sdp_string);  // This handler ACQUIRES mutex_
  };
  pc_callbacks.onLocalCandidateGenerated =
      [this, peer_id](const std::string& candidate, const std::string& sdp_mid,
                      int sdp_mline_index) {
        handlePeerLocalCandidateGenerated(
            peer_id, candidate, sdp_mid,
            sdp_mline_index);  // This handler ACQUIRES mutex_
      };
  pc_callbacks.onConnectionStateChange = [this,
                                          peer_id](PeerConnectionState state) {
    handlePeerConnectionStateChange(peer_id,
                                    state);  // This handler ACQUIRES mutex_
  };
  pc_callbacks.onIceConnectionStateChange =
      [this, peer_id](IceConnectionState state) {
        handlePeerIceConnectionStateChange(
            peer_id, state);  // This handler ACQUIREs mutex_
      };
  pc_callbacks.onSignalingStateChange = [this, peer_id](int state) {
    handlePeerSignalingStateChange(peer_id,
                                   state);  // This handler ACQUIREs mutex_
  };
  pc_callbacks.onDataChannelOpened = [this, peer_id](const std::string& label) {
    handlePeerDataChannelOpened(peer_id,
                                label);  // This handler ACQUIREs mutex_
  };
  pc_callbacks.onDataChannelClosed = [this, peer_id](const std::string& label) {
    handlePeerDataChannelClosed(peer_id,
                                label);  // This handler ACQUIRES mutex_
  };
  pc_callbacks.onDataChannelMessage = [this, peer_id](
                                          const std::string& label,
                                          const DataChannelMessage& message) {
    handlePeerDataChannelMessage(peer_id, label,
                                 message);  // This handler ACQUIRES mutex_
  };
  pc_callbacks.onError = [this, peer_id](const std::string& error_msg) {
    handlePeerError(peer_id, error_msg);  // This handler ACQUIRES mutex_
  };
  // TODO: Add VideoTrackReceived callback for Cockpit side

  // Create the PC instance using the factory method
  // This method is called from within the mutex_ lock.
  auto pc =
      createPeerConnection(peer_id, pc_callbacks /*, event_loop_, factory_ */);

  if (!pc) {
    std::cerr << "WebrtcManagerImpl: Failed to create PeerConnection for "
              << peer_id << std::endl;
    return false;
  }

  // Store the new PC in the map
  peerConnections_[peer_id] = std::move(pc);

  // Initiate the offer/answer process for this peer connection
  // Vehicle side typically creates offer, Cockpit side creates answer upon
  // receiving offer. Assuming this WebrtcManager is on the Vehicle side (from
  // VehicleConfig in original code), it might create an offer immediately upon
  // connecting to a peer (if client initiated) or wait for an offer from the
  // peer. Let's assume for this `connectToPeer` method that the LOCAL side
  // initiates the connection. If this is a Vehicle connecting to a Cockpit,
  // Vehicle should create offer. If this is a Cockpit connecting to a Vehicle,
  // Cockpit should create offer. This method should ideally only be called on
  // the initiating side. Let's assume this method is on the OFFERING side.
  peerConnections_[peer_id]
      ->CreateOffer();  // This triggers onLocalSdpGenerated callback
                        // asynchronously

  std::cout << "WebrtcManagerImpl: Initiated connection process for peer "
            << peer_id << std::endl;
  return true;
}

// Implementation of IWebrtcManager::disconnectFromPeer
bool WebrtcManagerImpl::disconnectFromPeer(const std::string& peer_id,
                                           const std::string& reason) {
  // This method is called by the application thread. ACQUIRE mutex_.
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "WebrtcManagerImpl: Attempting to disconnect from peer: "
            << peer_id << " Reason: " << reason << std::endl;

  auto it = peerConnections_.find(peer_id);
  if (it != peerConnections_.end()) {
    // Call Close on the PeerConnection. This triggers cleanup callbacks.
    it->second->Close();
    // The PC will be removed from the map later in handlePeerDisconnected after
    // callbacks finish.
    std::cout << "WebrtcManagerImpl: Called Close() on peer connection for "
              << peer_id << std::endl;
    return true;
  } else {
    std::cerr << "WebrtcManagerImpl: Peer " << peer_id
              << " not found for disconnection." << std::endl;
    return false;
  }
}

// Implementation of IWebrtcManager::sendDataChannelMessage
// This method is called by application threads (e.g., CommandHandler,
// TelemetryHandler). MUST BE THREAD-SAFE.
bool WebrtcManagerImpl::sendDataChannelMessage(const std::string& peer_id,
                                               const std::string& channel_label,
                                               const DataChannelMessage& data) {
  // Acquire lock to safely access peerConnections_
  std::lock_guard<std::mutex> lock(mutex_);

  // Check if state is Running
  if (state_ != AppState::Running) {
    // std::cerr << "WebrtcManagerImpl: Cannot send data, manager is not
    // running." << std::endl; Report error via application callback? Or just
    // return false.
    return false;
  }

  auto it = peerConnections_.find(peer_id);
  if (it != peerConnections_.end() &&
      it->second) {  // Check if iterator is valid and unique_ptr is not null
    // Call SendData on the PeerConnection interface
    // This call itself should be thread-safe within the PeerConnection
    // implementation, but accessing the PeerConnection object pointer requires
    // the mutex.
    bool success = it->second->SendData(channel_label, data);
    // std::cout << "WebrtcManagerImpl: Sent data to " << peer_id << " on label
    // " << channel_label << ", size=" << data.size() << (success ? "" : "
    // (failed)") << std::endl;
    return success;
  } else {
    // std::cerr << "WebrtcManagerImpl: Cannot send data, peer " << peer_id << "
    // not found or PC is null." << std::endl; Report error via application
    // callback? invokePeerErrorCallback(peer_id, "Attempted to send data to
    // unknown or invalid peer connection."); // Needs to acquire lock
    // internally
    return false;
  }
}

// Implementation of IWebrtcManager::sendDataChannelMessageToAllPeers
// This method is called by application threads (e.g., TelemetryHandler). MUST
// BE THREAD-SAFE.
bool WebrtcManagerImpl::sendDataChannelMessageToAllPeers(
    const std::string& channel_label, const DataChannelMessage& data) {
  // Acquire lock to safely iterate through peerConnections_
  std::lock_guard<std::mutex> lock(mutex_);

  // Check if state is Running
  if (state_ != AppState::Running) {
    // std::cerr << "WebrtcManagerImpl: Cannot broadcast data, manager is not
    // running." << std::endl;
    return false;
  }

  bool any_sent = false;
  // Iterate through all active peer connections
  for (auto const& [peer_id, pc] : peerConnections_) {
    if (pc) {  // Check if unique_ptr is not null
      // Call SendData on each PeerConnection
      // PeerConnection::SendData should be thread-safe.
      bool sent = pc->SendData(channel_label, data);
      if (sent) any_sent = true;
      // Log or handle individual send failures if needed
      // std::cout << "WebrtcManagerImpl: Broadcasted data to " << peer_id << "
      // on label " << channel_label << (sent ? "" : " (failed)") << std::endl;
    }
  }
  return any_sent;  // Return true if at least one message was sent successfully
}

// Implementation of IWebrtcManager::onSignalingConnected etc. (Callback
// registration) These methods are called by the application thread. They must
// be thread-safe as the handlers might be read from different threads.
void WebrtcManagerImpl::onSignalingConnected(
    OnSignalingConnectedHandler handler) {
  std::lock_guard<std::mutex> lock(mutex_);  // Protect setting the handler
  onSignalingConnectedHandler_ = handler;
}
void WebrtcManagerImpl::onSignalingDisconnected(
    OnSignalingDisconnectedHandler handler) {
  std::lock_guard<std::mutex> lock(mutex_);
  onSignalingDisconnectedHandler_ = handler;
}
void WebrtcManagerImpl::onSignalingError(OnSignalingErrorHandler handler) {
  std::lock_guard<std::mutex> lock(mutex_);
  onSignalingErrorHandler_ = handler;
}
void WebrtcManagerImpl::onPeerConnected(OnPeerConnectedHandler handler) {
  std::lock_guard<std::mutex> lock(mutex_);
  onPeerConnectedHandler_ = handler;
}
void WebrtcManagerImpl::onPeerDisconnected(OnPeerDisconnectedHandler handler) {
  std::lock_guard<std::mutex> lock(mutex_);
  onPeerDisconnectedHandler_ = handler;
}
void WebrtcManagerImpl::onPeerError(OnPeerErrorHandler handler) {
  std::lock_guard<std::mutex> lock(mutex_);
  onPeerErrorHandler_ = handler;
}
void WebrtcManagerImpl::onDataChannelMessageReceived(
    OnDataChannelMessageReceivedHandler handler) {
  std::lock_guard<std::mutex> lock(mutex_);
  onDataChannelMessageReceivedHandler_ = handler;
}
// Optional: void WebrtcManagerImpl::onVideoTrackReceived(...) { ... }

// --- Internal Handlers for SignalingClient Events ---
// These methods are called by the SignalingClient's thread. They must acquire
// mutex_.

void WebrtcManagerImpl::handleSignalingConnected() {
  // Called by SignalingClient thread. ACQUIRE mutex_.
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "WebrtcManagerImpl: Signaling connected." << std::endl;

  // TODO: Send JOIN message to signaling server (needs local client ID from
  // config) SignalMessage join_msg(SignalMessage::Type::JOIN,
  // config_.client_id, ""); // To signaling server if (signalingClient_)
  // signalingClient_->sendSignal(join_msg);

  // Invoke application callback (safely)
  invokeSignalingConnectedCallback();  // Calls invoke helper which acquires
                                       // mutex again (or passes func)
}

void WebrtcManagerImpl::handleSignalingDisconnected() {
  // Called by SignalingClient thread. ACQUIRE mutex_.
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "WebrtcManagerImpl: Signaling disconnected." << std::endl;

  // TODO: Handle peers. Depending on policy, disconnect/clean up PCs if
  // signaling is essential. Iterating and calling Close() might be okay, but
  // need to be careful if PC state changes also trigger cleanup logic.
  // destroyPeerConnection(...) might be called.

  // Invoke application callback (safely)
  invokeSignalingDisconnectedCallback(
      "Signaling connection lost");  // Calls invoke helper
}

void WebrtcManagerImpl::handleSignalingError(const std::string& msg) {
  // Called by SignalingClient thread. ACQUIRE mutex_.
  std::lock_guard<std::mutex> lock(mutex_);
  std::cerr << "WebrtcManagerImpl: Signaling error: " << msg << std::endl;
  invokeSignalingErrorCallback(msg);  // Calls invoke helper
}

void WebrtcManagerImpl::handleSignalingMessage(const SignalMessage& message) {
  // Called by SignalingClient thread. ACQUIRE mutex_.
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "WebrtcManagerImpl: Received signal message from "
            << message.from
            << " type=" << SignalMessage::TypeToString(message.type)
            << std::endl;

  std::string peer_id = message.from;  // Assuming 'from' is the peer ID

  // Handle specific message types
  switch (message.type) {
    case SignalMessage::Type::JOIN:
      std::cout << "WebrtcManagerImpl: Peer " << peer_id << " joined."
                << std::endl;
      // If this peer initiated the join (we are connecting to them), we might
      // create offer later. If they joined OUR session, we might need to create
      // a PeerConnection for them.
      // For simplicity, let's create or get the PC now.
      // getOrCreatePeerConnection(peer_id); // This method ACQUIRES mutex_ or
      // relies on current lock

      // Alternative: Only create PC on Offer/Answer/Candidate to avoid
      // unnecessary PC creation if (!peerConnections_.count(peer_id)) {
      //     std::cout << "WebrtcManagerImpl: Ignoring JOIN from unknown peer "
      //     << peer_id << std::endl;
      // }
      break;
    case SignalMessage::Type::LEAVE:
      std::cout << "WebrtcManagerImpl: Peer " << peer_id << " left."
                << std::endl;
      // Find and close the PeerConnection for this peer
      if (peerConnections_.count(peer_id)) {
        // Calling Close() here, cleanup happens in state change handler
        // destroyPeerConnection(peer_id, "Peer left signaling"); // This method
        // ACQUIRES mutex_ or relies on current lock
        if (peerConnections_[peer_id])
          peerConnections_[peer_id]
              ->Close();  // Calling Close under lock is generally safe
      } else {
        std::cout << "WebrtcManagerImpl: Received LEAVE for unknown peer "
                  << peer_id << std::endl;
      }
      break;
    case SignalMessage::Type::OFFER: {
      PeerConnection* pc = getOrCreatePeerConnection(
          peer_id);  // ACQUIRES mutex_ internally or relies on current lock
      if (pc && message.sdp) {
        pc->SetRemoteDescription("offer", *message.sdp);
        // Assuming this is the ANSWERING side (Vehicle)
        pc->CreateAnswer();  // Triggers onLocalSdpGenerated callback
                             // asynchronously
      } else {
        std::cerr << "WebrtcManagerImpl: Received OFFER without SDP or PC not "
                     "created for "
                  << peer_id << std::endl;
        invokePeerErrorCallback(
            peer_id,
            "Received OFFER with missing SDP or PC");  // Calls invoke helper
      }
      break;
    }
    case SignalMessage::Type::ANSWER: {
      PeerConnection* pc = getOrCreatePeerConnection(
          peer_id);  // ACQUIRES mutex_ internally or relies on current lock
      if (pc && message.sdp) {
        pc->SetRemoteDescription("answer", *message.sdp);
        // Assuming this is the OFFERING side (Cockpit)
        // Connection process continues...
      } else {
        std::cerr << "WebrtcManagerImpl: Received ANSWER without SDP or PC not "
                     "created for "
                  << peer_id << std::endl;
        invokePeerErrorCallback(
            peer_id,
            "Received ANSWER with missing SDP or PC");  // Calls invoke helper
      }
      break;
    }
    case SignalMessage::Type::CANDIDATE: {
      PeerConnection* pc = getOrCreatePeerConnection(
          peer_id);  // ACQUIRES mutex_ internally or relies on current lock
      if (pc && message.candidate && message.sdpMid && message.sdpMlineIndex) {
        pc->AddRemoteCandidate(*message.candidate, *message.sdpMid,
                               *message.sdpMlineIndex);
      } else {
        std::cerr << "WebrtcManagerImpl: Received CANDIDATE with missing "
                     "fields or PC not created for "
                  << peer_id << std::endl;
        invokePeerErrorCallback(
            peer_id,
            "Received CANDIDATE with missing fields or PC");  // Calls invoke
                                                              // helper
      }
      break;
    }
    case SignalMessage::Type::HEARTBEAT: {
      // std::cout << "WebrtcManagerImpl: Received HEARTBEAT from " << peer_id
      // << std::endl;
      handleReceivedHeartbeat(
          peer_id);  // ACQUIRES mutex_ or relies on current lock
      break;
    }
    case SignalMessage::Type::UNKNOWN:
    default:
      std::cerr
          << "WebrtcManagerImpl: Received unknown signal message type from "
          << peer_id << std::endl;
      invokeSignalingErrorCallback(
          "Received unknown signal message type from " +
          peer_id);  // Calls invoke helper
      break;
  }
}

// --- Internal Helper to get or create PeerConnection ---
// This method is called from within the mutex_ lock.
PeerConnection* WebrtcManagerImpl::getOrCreatePeerConnection(
    const std::string& peer_id) {
  // Lock is assumed to be held by the caller (e.g., handleSignalingMessage)
  // std::lock_guard<std::mutex> lock(mutex_); // If not called under lock,
  // acquire here

  auto it = peerConnections_.find(peer_id);
  if (it != peerConnections_.end()) {
    return it->second.get();  // Return raw pointer
  }

  std::cout << "WebrtcManagerImpl: Creating new PeerConnection for peer "
            << peer_id << std::endl;

  // Prepare callbacks for the new PC.
  // Lambdas capture peer_id and 'this'. Ensure 'this' is valid when lambda is
  // called. Using a shared_ptr to the manager might be safer if callbacks can
  // outlive the manager temporarily. For now, rely on the manager's stop()
  // ensuring callbacks are not called after destruction.
  PeerConnectionCallbacks pc_callbacks;
  pc_callbacks.onLocalSdpGenerated = [this, peer_id](
                                         const std::string& sdp_type,
                                         const std::string& sdp_string) {
    handlePeerLocalSdpGenerated(peer_id, sdp_type,
                                sdp_string);  // ACQUIRES mutex_
  };
  pc_callbacks.onLocalCandidateGenerated =
      [this, peer_id](const std::string& candidate, const std::string& sdp_mid,
                      int sdp_mline_index) {
        handlePeerLocalCandidateGenerated(peer_id, candidate, sdp_mid,
                                          sdp_mline_index);  // ACQUIRES mutex_
      };
  pc_callbacks.onConnectionStateChange = [this,
                                          peer_id](PeerConnectionState state) {
    handlePeerConnectionStateChange(peer_id, state);  // ACQUIRES mutex_
  };
  pc_callbacks.onIceConnectionStateChange =
      [this, peer_id](IceConnectionState state) {
        handlePeerIceConnectionStateChange(peer_id, state);  // ACQUIRES mutex_
      };
  pc_callbacks.onSignalingStateChange = [this, peer_id](int state) {
    handlePeerSignalingStateChange(peer_id, state);  // ACQUIRES mutex_
  };
  pc_callbacks.onDataChannelOpened = [this, peer_id](const std::string& label) {
    handlePeerDataChannelOpened(peer_id, label);  // ACQUIRES mutex_
  };
  pc_callbacks.onDataChannelClosed = [this, peer_id](const std::string& label) {
    handlePeerDataChannelClosed(peer_id, label);  // ACQUIRES mutex_
  };
  pc_callbacks.onDataChannelMessage = [this, peer_id](
                                          const std::string& label,
                                          const DataChannelMessage& message) {
    handlePeerDataChannelMessage(peer_id, label, message);  // ACQUIRES mutex_
  };
  pc_callbacks.onError = [this, peer_id](const std::string& error_msg) {
    handlePeerError(peer_id, error_msg);  // ACQUIRES mutex_
  };
  // TODO: Add OnAddStream/OnRemoveStream for media tracks (Cockpit side)
  // TODO: Add OnDataChannel for receiving incoming DataChannels (Cockpit side)

  // Create the concrete PC instance using the factory method (Implemented
  // below) This method needs access to libwebrtc factory and event loop context
  auto pc =
      createPeerConnection(peer_id, pc_callbacks /*, event_loop_, factory_ */);

  if (!pc) {
    std::cerr << "WebrtcManagerImpl: Failed to create PeerConnection for "
              << peer_id << std::endl;
    return nullptr;
  }

  // Store and return the new PC
  // The map access is protected by the caller's lock
  peerConnections_[peer_id] = std::move(pc);
  return peerConnections_[peer_id].get();
}

// Helper to destroy a PeerConnection and clean up state.
// This method is called from within the mutex_ lock.
void WebrtcManagerImpl::destroyPeerConnection(const std::string& peer_id,
                                              const std::string& reason) {
  // Lock is assumed to be held by the caller (e.g.,
  // handlePeerConnectionStateChange, handleSignalingMessage)
  // std::lock_guard<std::mutex> lock(mutex_); // If not called under lock,
  // acquire here

  auto it = peerConnections_.find(peer_id);
  if (it != peerConnections_.end()) {
    std::cout << "WebrtcManagerImpl: Destroying PeerConnection for peer "
              << peer_id << ". Reason: " << reason << std::endl;

    // Call Close() explicitly before erasing if not already done by the state
    // change
    if (it->second) {
      it->second->Close();
    }

    // Remove from heartbeat tracking
    lastHeartbeatRxTime_.erase(peer_id);
    reconnectionAttemptCount_.erase(peer_id);

    // Remove the unique_ptr from the map (this destroys the PeerConnection
    // object)
    peerConnections_.erase(it);

    // Invoke application callback (safely)
    invokePeerDisconnectedCallback(peer_id, reason);  // Calls invoke helper
  } else {
    // std::cout << "WebrtcManagerImpl: Attempted to destroy non-existent PC for
    // " << peer_id << std::endl;
  }
}

// --- Factory method for creating PeerConnection instances ---
// This method is called from within the mutex_ lock.
// TODO: Needs EventLoopContext* event_loop and PeerConnectionFactory* factory
// parameters
std::
    unique_ptr<PeerConnection>
    WebrtcManagerImpl::createPeerConnection(const std::string& peer_id,
                                            PeerConnectionCallbacks callbacks /*, EventLoopContext* event_loop, PeerConnectionFactory* factory */) {
  // Lock is assumed to be held by the caller (getOrCreatePeerConnection or
  // connectToPeer) std::lock_guard<std::mutex> lock(mutex_); // If not called
  // under lock, acquire here

  std::cout << "WebrtcManagerImpl: Using factory to create PeerConnection for "
            << peer_id << std::endl;

  // TODO: Use the actual libwebrtc factory and event loop context
  // Example (conceptual):
  // webrtc::PeerConnectionInterface::RTCConfiguration rtc_config;
  // // Populate rtc_config from config_ (ICE servers etc.)
  // rtc_config.ice_servers.push_back(webrtc::PeerConnectionInterface::IceServer());
  // // Populate ICE servers

  // auto rtc_pc = factory->CreatePeerConnection(rtc_config, nullptr, nullptr,
  // callbacks_adapter); // Use a callbacks adapter to bridge between libwebrtc
  // observer and our callbacks struct

  // if (!rtc_pc) {
  //     std::cerr << "Failed to create libwebrtc PeerConnection." << std::endl;
  //     return nullptr;
  // }

  // TODO: Create the concrete PeerConnection wrapper implementation
  // auto pc_impl = std::make_unique<LibwebrtcPeerConnection>(rtc_pc,
  // callbacks); // Pass libwebrtc PC and our callbacks struct

  // Dummy creation for skeleton
  auto pc_impl = std::make_unique<LibwebrtcPeerConnection>();  // Dummy instance
  pc_impl->SetCallbacks(callbacks);  // Set the provided callbacks

  return std::move(pc_impl);  // Return the unique_ptr
}

// --- Internal Handlers for PeerConnection Events ---
// These methods are called by the WebRTC Signaling thread (via PeerConnection
// Callbacks). They must acquire mutex_.

void WebrtcManagerImpl::handlePeerLocalSdpGenerated(
    const std::string& peer_id, const std::string& sdp_type,
    const std::string& sdp_string) {
  // Called by WebRTC signaling thread. ACQUIRE mutex_.
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "WebrtcManagerImpl: Local SDP generated for " << peer_id
            << ", type=" << sdp_type << std::endl;

  // Check if the peer connection still exists
  if (!peerConnections_.count(peer_id)) {
    std::cout << "WebrtcManagerImpl: Ignoring SDP for non-existent peer "
              << peer_id << std::endl;
    return;
  }

  // Create signal message and send via SignalingClient
  SignalMessage msg;
  msg.type = (sdp_type == "offer") ? SignalMessage::Type::OFFER
                                   : SignalMessage::Type::ANSWER;
  // msg.from = config_.client_id; // Needs client ID from config
  msg.from = "client_dummy_id";  // Dummy ID for skeleton
  msg.to = peer_id;
  msg.sdp = sdp_string;

  if (signalingClient_) {
    signalingClient_->sendSignal(msg);  // Send via signaling client (should be
                                        // thread-safe or use event loop)
  } else {
    std::cerr
        << "WebrtcManagerImpl: Signaling client not available to send SDP."
        << std::endl;
    invokeSignalingErrorCallback("Signaling client not available to send SDP");
  }
}

void WebrtcManagerImpl::handlePeerLocalCandidateGenerated(
    const std::string& peer_id, const std::string& candidate,
    const std::string& sdp_mid, int sdp_mline_index) {
  // Called by WebRTC signaling thread. ACQUIRE mutex_.
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "WebrtcManagerImpl: Local Candidate generated for " << peer_id
            << std::endl;

  // Check if the peer connection still exists
  if (!peerConnections_.count(peer_id)) {
    std::cout << "WebrtcManagerImpl: Ignoring candidate for non-existent peer "
              << peer_id << std::endl;
    return;
  }

  // Create signal message and send via SignalingClient
  SignalMessage msg;
  msg.type = SignalMessage::Type::CANDIDATE;
  // msg.from = config_.client_id; // Needs client ID from config
  msg.from = "client_dummy_id";  // Dummy ID for skeleton
  msg.to = peer_id;
  msg.candidate = candidate;
  msg.sdpMid = sdp_mid;
  msg.sdpMlineIndex = sdp_mline_index;

  if (signalingClient_) {
    signalingClient_->sendSignal(msg);  // Send via signaling client (should be
                                        // thread-safe or use event loop)
  } else {
    std::cerr << "WebrtcManagerImpl: Signaling client not available to send "
                 "candidate."
              << std::endl;
    invokeSignalingErrorCallback(
        "Signaling client not available to send candidate");
  }
}

void WebrtcManagerImpl::handlePeerConnectionStateChange(
    const std::string& peer_id, PeerConnectionState state) {
  // Called by WebRTC signaling thread. ACQUIRE mutex_.
  std::lock_guard<std::mutex> lock(mutex_);
  // TODO: Map int state to meaningful enum/string from libwebrtc
  std::cout << "WebrtcManagerImpl: PeerConnection state change for " << peer_id
            << ", state=" << static_cast<int>(state) << std::endl;

  // Check if the peer connection still exists
  if (!peerConnections_.count(peer_id)) {
    std::cout << "WebrtcManagerImpl: State change for non-existent peer "
              << peer_id << std::endl;
    return;
  }

  if (state == PeerConnectionState::Connected) {  // Use enum
    std::cout << "WebrtcManagerImpl: Peer " << peer_id << " connected!"
              << std::endl;
    // Invoke application callback (safely)
    invokePeerConnectedCallback(peer_id);  // Calls invoke helper

    // TODO: Start heartbeat for this peer if enabled
    // if (config_.heartbeat_interval_ms > 0) {
    //     lastHeartbeatRxTime_[peer_id] = std::chrono::steady_clock::now();
    //     // Ensure timer is running and checks include this peer
    // }
  } else if (state == PeerConnectionState::Disconnected ||
             state == PeerConnectionState::Failed ||
             state == PeerConnectionState::Closed) {  // Use enums
    std::string reason =
        "PC State: " + std::to_string(static_cast<int>(
                           state));  // Map state int to string for clarity
    if (state == PeerConnectionState::Disconnected)
      reason = "PC State: Disconnected";
    else if (state == PeerConnectionState::Failed)
      reason = "PC State: Failed";
    else if (state == PeerConnectionState::Closed)
      reason = "PC State: Closed";

    std::cout << "WebrtcManagerImpl: Peer " << peer_id
              << " disconnected/failed/closed. Reason: " << reason << std::endl;
    // Clean up the peer connection instance and notify app.
    // destroyPeerConnection(peer_id, reason); // This method ACQUIRES mutex_ or
    // relies on current lock. Calling it here might cause re-entrancy issues if
    // erase() triggers destructor and more callbacks. Safer: Queue cleanup or
    // perform cleanup after the state change handler returns. For simplicity in
    // skeleton, call destroy here and be aware of potential issues.
    destroyPeerConnection(peer_id,
                          reason);  // Calls destroy helper which acquires mutex
                                    // and removes from map.
  }
}

void WebrtcManagerImpl::handlePeerIceConnectionStateChange(
    const std::string& peer_id, IceConnectionState state) {
  // Called by WebRTC signaling thread. ACQUIRE mutex_.
  std::lock_guard<std::mutex> lock(mutex_);
  // TODO: Map int state to meaningful enum/string from libwebrtc
  std::cout << "WebrtcManagerImpl: Peer ICE Connection state change for "
            << peer_id << ", state=" << static_cast<int>(state) << std::endl;

  // Check if the peer connection still exists
  if (!peerConnections_.count(peer_id)) {
    std::cout << "WebrtcManagerImpl: ICE state change for non-existent peer "
              << peer_id << std::endl;
    return;
  }

  if (state == IceConnectionState::Connected ||
      state == IceConnectionState::Completed) {  // Use enums
    std::cout << "WebrtcManagerImpl: ICE Connected/Completed for " << peer_id
              << std::endl;
    // This often indicates actual connectivity status better than the overall
    // PC state. Maybe trigger onPeerConnectedHandler_ here if not already done
    // by PC state or update ConnectionMonitor status.
  } else if (state == IceConnectionState::Failed ||
             state == IceConnectionState::Disconnected ||
             state == IceConnectionState::Closed) {  // Use enums
    std::cout << "WebrtcManagerImpl: ICE Failed/Disconnected/Closed for "
              << peer_id << std::endl;
    // This might also indicate disconnection, let the PC StateChange handler
    // handle the main cleanup. But might trigger specific ConnectionMonitor
    // handlers.
  }
}

void WebrtcManagerImpl::handlePeerSignalingStateChange(
    const std::string& peer_id, int state) {
  // Called by WebRTC signaling thread. ACQUIRE mutex_.
  std::lock_guard<std::mutex> lock(mutex_);
  // TODO: Map int state to meaningful enum/string from libwebrtc
  std::cout << "WebrtcManagerImpl: Peer Signaling state change for " << peer_id
            << ", state=" << state << std::endl;

  // Check if the peer connection still exists
  if (!peerConnections_.count(peer_id)) {
    std::cout
        << "WebrtcManagerImpl: Signaling state change for non-existent peer "
        << peer_id << std::endl;
    return;
  }
}

void WebrtcManagerImpl::handlePeerDataChannelOpened(const std::string& peer_id,
                                                    const std::string& label) {
  // Called by WebRTC signaling thread. ACQUIRE mutex_.
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "WebrtcManagerImpl: DataChannel opened for " << peer_id
            << ", label=" << label << std::endl;

  // Check if the peer connection still exists
  if (!peerConnections_.count(peer_id)) {
    std::cout << "WebrtcManagerImpl: DataChannel opened for non-existent peer "
              << peer_id << std::endl;
    return;
  }

  // TODO: Check label against config (control_channel_label,
  // telemetry_channel_label) Could keep track of opened channels per peer here
  // if needed for sendDataChannelMessage logic. Notify application if
  // DataChannel readiness is important for sending/receiving specific data.
  // Example: TelemetryHandler might need to know telemetry channel is open
  // before sending updates.
}

void WebrtcManagerImpl::handlePeerDataChannelClosed(const std::string& peer_id,
                                                    const std::string& label) {
  // Called by WebRTC signaling thread. ACQUIRE mutex_.
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "WebrtcManagerImpl: DataChannel closed for " << peer_id
            << ", label=" << label << std::endl;

  // Check if the peer connection still exists
  if (!peerConnections_.count(peer_id)) {
    std::cout << "WebrtcManagerImpl: DataChannel closed for non-existent peer "
              << peer_id << std::endl;
    return;
  }
  // TODO: Update internal state if tracking DataChannel status.
}

void WebrtcManagerImpl::handlePeerDataChannelMessage(
    const std::string& peer_id, const std::string& label,
    const DataChannelMessage& message) {
  // Called by WebRTC signaling thread. ACQUIRE mutex_.
  std::lock_guard<std::mutex> lock(mutex_);
  // std::cout << "WebrtcManagerImpl: DataChannel message received for " <<
  // peer_id << ", label=" << label << ", size=" << message.size() << std::endl;

  // Check if the peer connection still exists
  if (!peerConnections_.count(peer_id)) {
    std::cout << "WebrtcManagerImpl: DataChannel message for non-existent peer "
              << peer_id << std::endl;
    return;
  }

  // Check label and route message
  // TODO: Use config values for labels
  // if (label == config_.control_channel_label) {
  if (label == "control") {  // Dummy check for skeleton
    invokeDataChannelMessageReceivedCallback(peer_id, label,
                                             message);  // Calls invoke helper
  }
  // else if (label == config_.telemetry_channel_label) { // Needs config
  else if (label == "telemetry") {  // Dummy check for skeleton
    // This might be loopback telemetry if the remote echoes it
    // Assuming telemetry received here should also be routed to the app
    invokeDataChannelMessageReceivedCallback(peer_id, label,
                                             message);  // Calls invoke helper
  }
  // TODO: Handle Heartbeat messages here based on label/content
  // else if (label == "heartbeat" && message == "ping") {
  // handleReceivedHeartbeat(peer_id); }
  else {
    std::cout
        << "WebrtcManagerImpl: Received message on unknown DataChannel label: "
        << label << " from " << peer_id << std::endl;
    // Log unexpected message
  }
}

void WebrtcManagerImpl::handlePeerError(const std::string& peer_id,
                                        const std::string& error_msg) {
  // Called by WebRTC signaling thread. ACQUIRE mutex_.
  std::lock_guard<std::mutex> lock(mutex_);
  std::cerr << "WebrtcManagerImpl: PeerConnection error for " << peer_id << ": "
            << error_msg << std::endl;

  // Check if the peer connection still exists
  if (!peerConnections_.count(peer_id)) {
    std::cout << "WebrtcManagerImpl: Error for non-existent peer " << peer_id
              << std::endl;
    return;
  }

  invokePeerErrorCallback(peer_id, error_msg);  // Calls invoke helper
  // Error might mean the connection is going down, StateChange handler should
  // catch closure and perform cleanup.
}

// --- Heartbeat and Reconnection Logic ---
// These methods need synchronization. onHeartbeatTimer is called by the timer
// thread.

// TODO: Implement timer integration with event_loop_
void WebrtcManagerImpl::startHeartbeatTimer() {
  // Acquire mutex briefly if accessing shared timer object or config
  // Timer mechanism itself might need to be created/configured outside the
  // mutex.
  std::lock_guard<std::mutex> lock(
      mutex_);  // Protect timer object if it's a member

  // if (config_.heartbeat_interval_ms > 0 && !heartbeatTimer_) {
  //     heartbeatTimer_ = event_loop_->createTimer(...); // Example using event
  //     loop timer heartbeatTimer_->start(config_.heartbeat_interval_ms, true,
  //     [this](){ onHeartbeatTimer(); });
  //      std::cout << "WebrtcManagerImpl: Heartbeat timer started." <<
  //      std::endl;
  // }
}

// TODO: Implement timer integration with event_loop_
void WebrtcManagerImpl::stopHeartbeatTimer() {
  std::lock_guard<std::mutex> lock(mutex_);  // Protect timer object

  // if (heartbeatTimer_) {
  //     heartbeatTimer_->stop();
  //     heartbeatTimer_.reset();
  //      std::cout << "WebrtcManagerImpl: Heartbeat timer stopped." <<
  //      std::endl;
  // }
}

// Called by the timer thread when it fires. ACQUIRE mutex_.
void WebrtcManagerImpl::onHeartbeatTimer() {
  std::lock_guard<std::mutex> lock(mutex_);
  // std::cout << "WebrtcManagerImpl: Heartbeat timer fired." << std::endl;

  checkForHeartbeatLoss();  // Check for lost heartbeats

  // Send heartbeats to connected peers
  SignalMessage heartbeat_msg;
  heartbeat_msg.type = SignalMessage::Type::HEARTBEAT;
  // heartbeat_msg.from = config_.client_id; // Needs client ID from config
  heartbeat_msg.from = "client_dummy_id";  // Dummy ID for skeleton
  // Heartbeat content might include timestamp or sequence number for tracking.
  heartbeat_msg.message = "ping";  // Dummy content

  for (auto const& [peer_id, pc] : peerConnections_) {
    if (pc &&
        pc->GetConnectionState() ==
            PeerConnectionState::Connected) {  // Only send to connected peers
      // Option 1: Send heartbeat via signaling (Simpler if signaling supports
      // it) heartbeat_msg.to = peer_id; if (signalingClient_)
      // signalingClient_->sendSignal(heartbeat_msg);

      // Option 2: Send heartbeat via DataChannel (More common for peer-to-peer
      // checks) Needs a dedicated heartbeat DataChannel
      const std::string heartbeat_channel_label =
          "heartbeat";  // Define a heartbeat channel label
      DataChannelMessage ping_data = {'p', 'i', 'n', 'g'};  // Example raw data
      // Ensure heartbeat channel is open before sending
      // pc->IsDataChannelOpen(heartbeat_channel_label) // Check channel state
      // if available in PC interface
      pc->SendData(heartbeat_channel_label,
                   ping_data);  // Send via PC DataChannel
    }
  }
}

// Checks for lost heartbeats. Called by onHeartbeatTimer(). ACQUIRE mutex_.
void WebrtcManagerImpl::checkForHeartbeatLoss() {
  // Lock is assumed to be held by onHeartbeatTimer.
  // std::lock_guard<std::mutex> lock(mutex_); // If called elsewhere, acquire
  // here

  if (config_.heartbeat_interval_ms <= 0) return;  // Heartbeat disabled

  auto now = std::chrono::steady_clock::now();
  // Define a threshold, e.g., 2 or 3 times the interval
  auto timeout = std::chrono::milliseconds(config_.heartbeat_interval_ms * 3);

  std::vector<std::string> peers_to_disconnect;

  for (auto const& [peer_id, last_rx_time] : lastHeartbeatRxTime_) {
    if (peerConnections_.count(peer_id) && peerConnections_[peer_id] &&
        peerConnections_[peer_id]->GetConnectionState() ==
            PeerConnectionState::Connected) {
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
          now - last_rx_time);
      if (elapsed > timeout) {
        std::cerr << "WebrtcManagerImpl: Heartbeat lost from peer " << peer_id
                  << std::endl;
        peers_to_disconnect.push_back(peer_id);
        // TODO: Maybe increment a heartbeat loss counter before disconnecting
        // attemptReconnection(peer_id); // Or trigger reconnection attempt here
      }
    }
  }

  // Disconnect peers outside the iteration loop to avoid invalidating iterators
  for (const auto& peer_id : peers_to_disconnect) {
    // Calling destroyPeerConnection here will acquire the mutex again or rely
    // on current lock.
    destroyPeerConnection(peer_id,
                          "Heartbeat lost");  // This method ACQUIRES mutex_ or
                                              // relies on current lock.
  }
}

// Updates lastHeartbeatRxTime_. Called by handleSignalingMessage or
// handlePeerDataChannelMessage. ACQUIRE mutex_.
void WebrtcManagerImpl::handleReceivedHeartbeat(const std::string& peer_id) {
  // Lock is assumed to be held by the caller handler.
  // std::lock_guard<std::mutex> lock(mutex_); // If called elsewhere, acquire
  // here

  if (config_.heartbeat_interval_ms > 0) {
    lastHeartbeatRxTime_[peer_id] = std::chrono::steady_clock::now();
    // std::cout << "WebrtcManagerImpl: Updated heartbeat for " << peer_id <<
    // std::endl;
  }
}

// Tries to reconnect to a peer. ACQUIRE mutex_.
void WebrtcManagerImpl::attemptReconnection(const std::string& peer_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "WebrtcManagerImpl: Attempting reconnection for peer " << peer_id
            << std::endl;

  // TODO: Implement reconnection logic
  // Increment attempt counter
  // If attempts < max_attempts:
  //    Try reconnecting signaling?
  //    Try creating a new PeerConnection and sending offer/answer?
  // else:
  //    Report permanent failure
}

// --- Helper to safely invoke application callbacks ---
// These helpers acquire the mutex briefly to read the handler function,
// then release the mutex before invoking the handler.
// This prevents application callbacks from blocking the manager's threads while
// holding the mutex. If callbacks need to be marshalled to a specific
// application thread, that logic goes here. For now, callbacks are called
// directly from the manager/webrtc/signaling threads.

void WebrtcManagerImpl::invokeSignalingConnectedCallback() {
  OnSignalingConnectedHandler handler;
  {
    std::lock_guard<std::mutex> lock(mutex_);  // Safely get the handler
    handler = onSignalingConnectedHandler_;
  }
  if (handler) {
    // TODO: Marshal to application thread if needed
    handler();  // Invoke the application's handler
  }
}

void WebrtcManagerImpl::invokeSignalingDisconnectedCallback(
    const std::string& reason) {
  OnSignalingDisconnectedHandler handler;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    handler = onSignalingDisconnectedHandler_;
  }
  if (handler) {
    // TODO: Marshal to application thread if needed
    handler(reason);
  }
}

void WebrtcManagerImpl::invokeSignalingErrorCallback(
    const std::string& error_msg) {
  OnSignalingErrorHandler handler;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    handler = onSignalingErrorHandler_;
  }
  if (handler) {
    // TODO: Marshal to application thread if needed
    handler(error_msg);
  }
}

void WebrtcManagerImpl::invokePeerConnectedCallback(
    const std::string& peer_id) {
  OnPeerConnectedHandler handler;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    handler = onPeerConnectedHandler_;
  }
  if (handler) {
    // TODO: Marshal to application thread if needed
    handler(peer_id);
  }
}

void WebrtcManagerImpl::invokePeerDisconnectedCallback(
    const std::string& peer_id, const std::string& reason) {
  OnPeerDisconnectedHandler handler;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    handler = onPeerDisconnectedHandler_;
  }
  if (handler) {
    // TODO: Marshal to application thread if needed
    handler(peer_id, reason);
  }
}

void WebrtcManagerImpl::invokePeerErrorCallback(const std::string& peer_id,
                                                const std::string& error_msg) {
  OnPeerErrorHandler handler;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    handler = onPeerErrorHandler_;
  }
  if (handler) {
    // TODO: Marshal to application thread if needed
    handler(peer_id, error_msg);
  }
}

void WebrtcManagerImpl::invokeDataChannelMessageReceivedCallback(
    const std::string& peer_id, const std::string& label,
    const DataChannelMessage& message) {
  OnDataChannelMessageReceivedHandler handler;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    handler = onDataChannelMessageReceivedHandler_;
  }
  if (handler) {
    // TODO: Marshal to application thread if needed
    // If the application's handler does significant work or interacts with UI
    // (Cockpit), it MUST be marshalled to the application's event loop thread.
    handler(peer_id, label, message);
  }
}

// Optional: void WebrtcManagerImpl::invokeVideoTrackReceivedCallback(...) { ...
// }

// --- Other Internal Logic ---
// ... Heartbeat and Reconnection implementation details ...

}  // namespace webrtc
}  // namespace remote
}  // namespace autodev
