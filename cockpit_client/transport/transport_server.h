#ifndef TRANSPORT_SERVER_H
#define TRANSPORT_SERVER_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// Define a type for a WebSocket connection identifier.
// Could be an integer, a pointer, or something specific to the server library.
// It must uniquely identify a connection within the server's lifetime.
using WebSocketConnectionId = uint32_t;  // Example: using uint32_t

// Use namespace for better organization
namespace autodev {
namespace remote {
namespace transport {

// Interface for a local server serving web content and providing WebSocket
// communication. This server is typically used by the CockpitClientApp to host
// the remote driving UI.
class ITransportServer {
 public:
  virtual ~ITransportServer() = default;

  // --- Callback Type Definitions for WebSocket events ---

  // Handler for new WebSocket connections.
  // conn_id: The unique identifier for the new connection.
  // Implementations MUST be thread-safe if the server runs on a different
  // thread.
  using OnWebSocketConnectedHandler =
      std::function<void(WebSocketConnectionId conn_id)>;

  // Handler for WebSocket disconnections.
  // conn_id: The identifier of the disconnected connection.
  // Implementations MUST be thread-safe.
  using OnWebSocketDisconnectedHandler =
      std::function<void(WebSocketConnectionId conn_id)>;

  // Handler for messages received over WebSocket.
  // Use vector<char> for binary support, which is common for Protobuf or other
  // formats. conn_id: The identifier of the connection the message arrived on.
  // message: The received raw data.
  // Implementations MUST be thread-safe. The message vector's memory
  // is typically owned by the server implementation; copy data if needed after
  // handler returns.
  using OnWebSocketMessageReceivedHandler = std::function<void(
      WebSocketConnectionId conn_id, const std::vector<char>& message)>;

  // Handler for critical server errors (e.g., bind failure, listen error).
  // error_msg: Description of the error.
  // Implementations MUST be thread-safe.
  using OnServerErrorHandler =
      std::function<void(const std::string& error_msg)>;

  // --- Server Lifecycle Methods ---

  // Initializes the server with network configuration and static file path.
  // Should validate configuration and set up internal structures, but not start
  // listening. Returns true on success, false on failure.
  virtual bool init(const std::string& address, uint16_t port,
                    const std::string& static_files_path) = 0;

  // Starts the server's listening and handling loop.
  // This method might be blocking (e.g., runs in the caller's thread) or
  // start background threads/tasks internally depending on the implementation.
  // Returns true if the server successfully starts listening.
  // Must be called after init().
  virtual bool start() = 0;

  // Stops the server gracefully.
  // This should close all active connections and stop any internal threads or
  // tasks. This method should block until the server is fully stopped and
  // resources are released. Safe to call even if the server is not started or
  // already stopped.
  virtual void stop() = 0;

  // --- Data Sending Methods ---

  // Sends binary data over a specific WebSocket connection.
  // conn_id: The identifier of the target connection.
  // data: The binary data to send.
  // Returns true if the send operation was successfully queued or completed
  // (depends on async nature). This method should ideally be non-blocking,
  // queuing the send operation.
  virtual bool sendWebSocketMessage(WebSocketConnectionId conn_id,
                                    const std::vector<char>& data) = 0;

  // Overload to send string data over a specific WebSocket connection.
  virtual bool sendWebSocketMessage(WebSocketConnectionId conn_id,
                                    const std::string& data) = 0;

  // Sends binary data to ALL currently connected WebSocket clients.
  // Useful for broadcasting telemetry updates or status messages.
  // data: The binary data to send.
  // Returns true if the broadcast operation was successfully queued or
  // completed. This method should ideally be non-blocking.
  virtual bool sendToAllWebSocketClients(const std::vector<char>& data) = 0;

  // Overload to send string data to ALL currently connected WebSocket clients.
  virtual bool sendToAllWebSocketClients(const std::string& data) = 0;

  // --- Handler Registration Methods ---
  // These methods allow the application to register callbacks for server
  // events. They should be called after init() and before start(). Passing a
  // default-constructed std::function clears the handler.

  // Register handler for new WebSocket connections.
  virtual void onWebSocketConnected(OnWebSocketConnectedHandler handler) = 0;

  // Register handler for WebSocket disconnections.
  virtual void onWebSocketDisconnected(
      OnWebSocketDisconnectedHandler handler) = 0;

  // Register handler for messages received over WebSocket.
  virtual void onWebSocketMessageReceived(
      OnWebSocketMessageReceivedHandler handler) = 0;

  // Register handler for critical server errors.
  virtual void onServerError(OnServerErrorHandler handler) = 0;

  // Optional: Add a method to get the list of currently connected client IDs?
  // virtual std::vector<WebSocketConnectionId> getConnectedClients() const = 0;
};

}  // namespace transport
}  // namespace remote
}  // namespace autodev

#endif  // TRANSPORT_SERVER_H
