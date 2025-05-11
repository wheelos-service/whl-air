#include "include/webrtc/signaling_client.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

#include "include/webrtc/signaling_message.h"

// --- Include websocketpp ---
// Ensure you have websocketpp and its Asio transport dependency (Boost.Asio or
// standalone Asio)
#include <websocketpp/client.hpp>
// Select the config based on whether you need TLS (wss) or not (ws)
#include <websocketpp/config/asio_tls_client.hpp>  // Use asio_tls_client for TLS (recommended)
// #include <websocketpp/config/asio_client.hpp> // Use asio_client for non-TLS

// Define the client type based on the chosen config
using client =
    websocketpp::client<websocketpp::config::asio_tls_client>;  // Example with
                                                                // TLS
// using client = websocketpp::client<websocketpp::config::asio_client>; //
// Example without TLS

using message_ptr = client::message_ptr;
using connection_hdl = websocketpp::connection_hdl;
// Use websocketpp's lib::bind which works with both Boost.Bind and std::bind
using websocketpp::lib::bind;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;

// --- SignalingClientImpl (Concrete implementation) ---
class SignalingClientImpl : public SignalingClient {
 public:
  // Constructor
  SignalingClientImpl(const std::string& uri, const std::string& jwt)
      : SignalingClient(uri, jwt),  // Call base class constructor
        uri_(uri),
        jwt_(jwt),
        isRunning_(false)  // Initialize state flag
  {
    std::cout << "SignalingClientImpl created." << std::endl;
    // Set logging and error handlers for websocketpp (optional but recommended)
    // Configure based on desired verbosity
    wsClient_.set_access_channels(
        websocketpp::log::alevel::all ^
        websocketpp::log::alevel::frame_payload);  // Log all except payload
    wsClient_.set_error_channels(websocketpp::log::elevel::all);

    // Initialize the Asio transport
    // This prepares the underlying I/O context
    wsClient_.init_asio();

    // Register websocketpp handlers by binding member functions
    wsClient_.set_open_handler(bind(&SignalingClientImpl::onOpen, this, _1));
    wsClient_.set_message_handler(
        bind(&SignalingClientImpl::onMessage, this, _1, _2));
    wsClient_.set_close_handler(bind(&SignalingClientImpl::onClose, this, _1));
    wsClient_.set_fail_handler(bind(&SignalingClientImpl::onFail, this, _1));

    // For TLS, you MUST set a TLS init handler to configure the SSL context
    if constexpr (std::is_same_v<client::config::transport_type,
                                 websocketpp::transport::asio::tls_client>) {
      wsClient_.set_tls_init_handler(
          bind(&SignalingClientImpl::onTlsInit, this, _1));
    }

    // Note: The Asio io_context is owned and run by wsClient_. It will be
    // started in connect().
  }

  // Destructor
  ~SignalingClientImpl() override {
    std::cout << "SignalingClientImpl destroying." << std::endl;
    // Ensure disconnect is called to stop the Asio thread and close the
    // connection
    disconnect();
    std::cout << "SignalingClientImpl destroyed." << std::endl;
  }

  // --- Public Interface Implementation ---

  void connect() override {
    if (isRunning_) {
      std::cout << "SignalingClientImpl: Already running/connecting. Ignoring "
                   "connect call."
                << std::endl;
      // TODO: If already disconnected, allow reconnect? Needs more state
      // management.
      return;
    }
    std::cout << "SignalingClientImpl: Initiating connection to " << uri_
              << std::endl;

    websocketpp::lib::error_code ec;

    // --- Prepare the WebSocket URI with JWT ---
    // The Node.js signaling server expects the JWT in a query parameter (e.g.,
    // ?token=...) You NEED to correctly URI-encode the JWT token.
    std::string encoded_jwt =
        jwt_;  // TODO: Implement proper URI encoding for jwt_ string!
               // Example (using a hypothetical encodeURIComponent function):
               // std::string encoded_jwt = encodeURIComponent(jwt_);
               // Without encoding, special characters in JWT will break the
               // URI.
    std::string connection_uri = uri_;
    if (!encoded_jwt.empty()) {
      if (connection_uri.find('?') == std::string::npos) {
        connection_uri += "?token=" + encoded_jwt;
      } else {
        connection_uri += "&token=" + encoded_jwt;
      }
    }
    std::cout << "SignalingClientImpl: Connecting to modified URI: "
              << connection_uri << std::endl;

    client::connection_ptr con = wsClient_.get_connection(connection_uri, ec);
    if (ec) {
      std::cerr << "SignalingClientImpl Error: Could not create connection: "
                << ec.message() << std::endl;
      if (onErrorHandler_) {
        onErrorHandler_("Could not create connection: " + ec.message());
      }
      return;  // Cannot proceed without a connection object
    }

    // Store connection handle. This handle is used to refer to the connection
    // later.
    connectionHdl_ = con->get_handle();

    // Connect the WebSocket client
    wsClient_.connect(con);

    // Start the Asio io_context in a separate thread.
    // This call blocks until io_context.stop() is called (usually in
    // disconnect). All websocketpp callbacks (onOpen, onMessage, etc.) will be
    // invoked in this thread.
    isRunning_ = true;  // Set flag BEFORE starting thread
    asioThread_ = std::thread([this]() {
      std::cout << "SignalingClientImpl: Asio thread started." << std::endl;
      wsClient_.run();  // This is the blocking call processing events
      std::cout << "SignalingClientImpl: Asio thread finished running."
                << std::endl;
      // After run() exits (due to stop()), signal disconnected if not already
      if (isRunning_ && onDisconnectedHandler_) {
        // This check might help prevent double calls if disconnect/close
        // happened earlier but relying purely on wsClient_.run() exiting is
        // cleaner. Consider adding a mutex or atomic flag for disconnected
        // status if needed.
        onDisconnectedHandler_();  // Call disconnected handler if Asio thread
                                   // finished unexpectedly
      }
      isRunning_ = false;  // Clear flag after thread exits run()
    });
    std::cout << "SignalingClientImpl: Connect initiated. Asio thread starting."
              << std::endl;
  }

  void disconnect() override {
    if (!isRunning_) {
      std::cout
          << "SignalingClientImpl: Not connected or already disconnecting."
          << std::endl;
      return;
    }
    std::cout << "SignalingClientImpl: Initiating disconnection." << std::endl;

    // Set flag first (helpful if handlers check this)
    isRunning_ = false;

    // Get the connection pointer from the handle
    websocketpp::lib::error_code ec;
    client::connection_ptr con = wsClient_.get_connection(connectionHdl_, ec);

    if (ec || !con) {
      std::cerr << "SignalingClientImpl Error: Could not get connection handle "
                   "for disconnect: "
                << ec.message() << std::endl;
      // If the connection object is already gone, just stop the Asio thread
      // directly
      wsClient_.stop();  // Signal the Asio loop to stop
    } else {
      // Check if connection is still open before trying to send close frame
      if (con->get_state() == websocketpp::session::state::open) {
        // Send a graceful close frame
        std::cout << "SignalingClientImpl: Sending close frame." << std::endl;
        wsClient_.close(connectionHdl_, websocketpp::close::status::going_away,
                        "Client disconnecting", ec);
        if (ec) {
          std::cerr << "SignalingClientImpl Error: Failed to send close frame: "
                    << ec.message() << std::endl;
        }
        // Note: The server or remote peer's response to the close frame will
        // eventually lead to the onClose handler being called.
      } else {
        std::cout
            << "SignalingClientImpl: Connection not open, skipping close frame."
            << std::endl;
      }

      // **Crucially, signal the Asio io_context to stop its run loop.**
      // This allows the asioThread_ to exit gracefully.
      wsClient_.stop();
      std::cout << "SignalingClientImpl: Signaled Asio loop to stop."
                << std::endl;
    }

    // **Wait for the Asio thread to finish.**
    // This is safe NOW because wsClient_.stop() has been called, guaranteeing
    // run() will exit.
    if (asioThread_.joinable()) {
      std::cout << "SignalingClientImpl: Joining Asio thread." << std::endl;
      asioThread_.join();
      std::cout << "SignalingClientImpl: Asio thread joined." << std::endl;
    } else {
      std::cout << "SignalingClientImpl: Asio thread not joinable."
                << std::endl;
    }

    std::cout << "SignalingClientImpl: Disconnect complete." << std::endl;
  }

  void sendSignal(const SignalMessage& message) override {
    websocketpp::lib::error_code ec;
    client::connection_ptr con = wsClient_.get_connection(connectionHdl_, ec);

    // Check if the connection handle is valid and the connection is open
    if (ec || !con || con->get_state() != websocketpp::session::state::open) {
      std::cerr << "SignalingClientImpl Error: Cannot send signal (connection "
                   "not open): "
                << ec.message() << std::endl;
      if (onErrorHandler_) {
        onErrorHandler_(
            "Cannot send signal, connection not open or handle invalid.");
      }
      // TODO: Implement message queueing here if you want to send messages
      // before the connection is established. Store the message and try sending
      // queued messages in the onOpen handler.
      return;
    }

    // Serialize the SignalMessage to a JSON string
    std::string message_string = SerializeSignalMessage(message);
    // std::cout << "SignalingClientImpl: Sending: " << message_string <<
    // std::endl; // Avoid logging large SDPs always

    // Send the message via the WebSocket connection
    wsClient_.send(connectionHdl_, message_string,
                   websocketpp::frame::opcode::text, ec);
    if (ec) {
      std::cerr << "SignalingClientImpl Error: Failed to send message: "
                << ec.message() << std::endl;
      if (onErrorHandler_) {
        onErrorHandler_("Failed to send message: " + ec.message());
      }
    }
    // Note: This send method is generally thread-safe in websocketpp as it
    // marshals the operation to the Asio thread.
  }

  // --- Callback Setters (Store handlers provided by the application) ---
  void onConnected(OnConnectedHandler handler) override {
    onConnectedHandler_ = handler;
  }
  void onDisconnected(OnDisconnectedHandler handler) override {
    onDisconnectedHandler_ = handler;
  }
  void onError(OnErrorHandler handler) override { onErrorHandler_ = handler; }
  void onMessageReceived(OnMessageReceivedHandler handler) override {
    onMessageReceivedHandler_ = handler;
  }

 private:
  client wsClient_;               // The websocketpp client instance
  connection_hdl connectionHdl_;  // Handle for the current connection
  std::string uri_;               // WebSocket server URI (from constructor)
  std::string jwt_;               // JWT for authentication (from constructor)

  std::atomic<bool> isRunning_;  // Flag indicating if the Asio thread is
                                 // expected to be running
  std::thread asioThread_;       // Thread for running the Asio io_context

  // Handlers stored in the base class (SignalingClient) are used by the
  // internal callbacks. onConnectedHandler_ onDisconnectedHandler_
  // onErrorHandler_
  // onMessageReceivedHandler_

  // --- Internal websocketpp handlers (called by websocketpp in the Asio
  // thread) ---

  void onOpen(connection_hdl hdl) {
    std::cout << "SignalingClientImpl: Connection opened successfully."
              << std::endl;
    // Ensure the stored handle is the same as the one that opened (important
    // after potential reconnects)
    connectionHdl_ = hdl;
    // Check if the handler is set and call it
    if (onConnectedHandler_) {
      // TODO: If application callbacks need to run on a specific thread (e.g.,
      // main thread, WebRTC thread), you MUST marshal this callback execution
      // using a thread-safe mechanism (e.g., queueing a task). Calling it
      // directly like this is only safe if the handler itself is thread-safe or
      // runs on the Asio thread.
      onConnectedHandler_();
    }
    // TODO: If using message queueing, attempt to send queued messages here.
  }

  void onMessage(connection_hdl hdl, message_ptr msg) {
    // std::cout << "SignalingClientImpl: Message received (payload size: " <<
    // msg->get_payload().size() << ")." << std::endl; // Avoid logging large
    // SDPs always

    // Deserialize the received message string (JSON) into a SignalMessage
    // struct
    auto signalMessage = DeserializeSignalMessage(msg->get_payload());

    if (signalMessage) {
      // Deserialization was successful. Call the user-provided handler.
      if (onMessageReceivedHandler_) {
        // TODO: As with onOpen, marshal this callback if it needs to run on a
        // different thread.
        onMessageReceivedHandler_(*signalMessage);
      }
    } else {
      // Deserialization failed, report an error
      std::cerr << "SignalingClientImpl Error: Failed to deserialize received "
                   "message payload."
                << std::endl;
      if (onErrorHandler_) {
        // TODO: As with onOpen, marshal this callback if it needs to run on a
        // different thread.
        onErrorHandler_("Failed to parse received message.");
      }
      // Note: Depending on the error handling strategy, you might close the
      // connection here.
    }
  }

  void onClose(connection_hdl hdl) {
    std::cout << "SignalingClientImpl: Connection closed." << std::endl;
    // Note: You can get close code and reason via
    // wsClient_.get_connection(hdl)->get_remote_close_code() and
    // get_remote_close_reason(). This handler is called when the connection is
    // closed by the server or the remote peer, or after the client initiated a
    // close frame. The disconnect() method should ensure wsClient_.stop() is
    // called, which will cause the Asio thread's run() to exit. Call the
    // user-provided disconnected handler. Use isRunning_ flag or similar state
    // to avoid potential double-calls if onFail also triggers a close. A mutex
    // might be needed to safely update/check state related to connection
    // status.

    // Call disconnected handler. It's crucial this happens when the connection
    // is truly gone. The logic in disconnect() handles stopping the thread.
    // This callback should simply inform the app.
    if (onDisconnectedHandler_) {
      // TODO: As with onOpen, marshal this callback if it needs to run on a
      // different thread.
      onDisconnectedHandler_();
    }
    // TODO: Implement auto-reconnect logic here if desired.
  }

  void onFail(connection_hdl hdl) {
    // This handler is called when the initial connection attempt fails.
    std::cerr
        << "SignalingClientImpl Error: Connection failed during handshake."
        << std::endl;
    // Get error details from the connection object
    websocketpp::lib::error_code ec;
    client::connection_ptr con = wsClient_.get_connection(hdl, ec);
    std::string error_msg = "Connection handshake failed";
    if (con) {
      error_msg += ": " + con->get_ec().message();
    } else if (ec) {
      error_msg +=
          ": (handle error) " + ec.message();  // Fallback if handle is also bad
    }

    std::cerr << "SignalingClientImpl Error: " << error_msg << std::endl;

    // Report the error
    if (onErrorHandler_) {
      // TODO: As with onOpen, marshal this callback if it needs to run on a
      // different thread.
      onErrorHandler_(error_msg);
    }

    // A connection failure means the connection never opened, so onClose might
    // not be called. Ensure the disconnected handler is called to inform the
    // app the connection is NOT active. Use state flags carefully if onClose
    // might also be called.
    if (onDisconnectedHandler_) {
      // TODO: As with onOpen, marshal this callback if it needs to run on a
      // different thread. onDisconnectedHandler_(); // Call disconnected
      // handler
    }

    // After a failure, ensure the Asio thread's run() exits.
    // wsClient_.stop() should be called here as well if it wasn't in
    // disconnect(). The disconnect() logic attempts to handle this by calling
    // stop() and join(). If connect() failed before starting the thread, this
    // is moot. If connect() started the thread but handshake failed, run() will
    // exit.
  }

  // --- TLS Init Handler (required if using asio_tls_client) ---
  // This function is called by websocketpp when a TLS connection is being
  // established. It's where you configure the SSL context (e.g., set CA
  // certificates for verification).
  websocketpp::lib::shared_ptr<websocketpp::lib::asio::ssl::context> onTlsInit(
      connection_hdl hdl) {
    std::cout << "SignalingClientImpl: Performing TLS initialization."
              << std::endl;
    // Create a new SSL context for TLSv1.2 or higher (recommended)
    websocketpp::lib::shared_ptr<websocketpp::lib::asio::ssl::context>
        ssl_context =
            websocketpp::lib::make_shared<websocketpp::lib::asio::ssl::context>(
                websocketpp::lib::asio::ssl::context::tlsv12);

    try {
      // Set standard recommended SSL options
      ssl_context->set_options(
          websocketpp::lib::asio::ssl::context::default_workarounds |
          websocketpp::lib::asio::ssl::context::no_sslv2 |    // Disable SSLv2
          websocketpp::lib::asio::ssl::context::no_sslv3 |    // Disable SSLv3
          websocketpp::lib::asio::ssl::context::no_tlsv1 |    // Disable TLSv1.0
          websocketpp::lib::asio::ssl::context::no_tlsv1_1 |  // Disable TLSv1.1
          websocketpp::lib::asio::ssl::context::
              single_dh_use  // Improve security against certain attacks
      );

      // **IMPORTANT SECURITY CONSIDERATION:**
      // For production, you MUST verify the server's certificate.
      // 1. Load trusted CA certificates:
      // ssl_context->load_verify_file("path/to/ca-certificates.crt"); // e.g.,
      // system CA bundle or your specific CA
      // 2. Enable peer verification:
      // ssl_context->set_verify_mode(websocketpp::lib::asio::ssl::verify_peer);
      // 3. Implement and set a verify callback to check the hostname against
      // the certificate. client::connection_ptr con =
      // wsClient_.get_connection(hdl);
      // ssl_context->set_verify_callback(bind(&SignalingClientImpl::verifyCertificate,
      // this, con->get_host(), _1, _2));

      // --- FOR TESTING AGAINST SELF-SIGNED OR UNTRUSTED CERTS ONLY ---
      // This disables proper verification and is INSECURE FOR PRODUCTION!
      ssl_context->set_verify_mode(
          websocketpp::lib::asio::ssl::verify_none);  // WARNING: Disables ALL
                                                      // verification!
      // Or, if verify_peer is on, use verify_accept_all (still insecure):
      // ssl_context->set_verify_mode(websocketpp::lib::asio::ssl::verify_peer);
      // ssl_context->set_verify_callback(websocketpp::lib::asio::ssl::verify_accept_all);
      // // DANGER: Accepts ANY cert!
      std::cerr << "SignalingClientImpl WARNING: TLS certificate verification "
                   "is DISABLED. This is INSECURE for production."
                << std::endl;

    } catch (websocketpp::lib::asio::system_error& e) {
      std::cerr << "SignalingClientImpl Error: TLS context setup failed: "
                << e.what() << std::endl;
      // Depending on policy, you might terminate the connection setup here.
      // throw; // Rethrow to signal fatal error in handshake
    }
    return ssl_context;
  }

  // Optional: Certificate verification callback implementation
  // bool verifyCertificate(const std::string& hostname, bool preverified,
  //                        websocketpp::lib::asio::ssl::verify_context& ctx) {
  //      // Implement logic to check hostname against cert subject/alt names
  //      // using X509_get_subject_name, X509_check_host etc. from OpenSSL
  //      // return preverified && /* hostname verification passes */;
  //      return true; // Skeleton: always trust
  // }

  // Prevent copying and assignment
  SignalingClientImpl(const SignalingClientImpl&) = delete;
  SignalingClientImpl& operator=(const SignalingClientImpl&) = delete;
};

// --- If using the abstract SignalingClient class ---
// The public methods of the abstract class would delegate to an internal
// concrete instance (e.g., unique_ptr<SignalingClientImpl>). This requires the
// abstract class definition from signaling_client.h

/*
// In your .h file (assuming PImpl):
// class SignalingClientImpl; // Forward declaration
// class SignalingClient { ... private: std::unique_ptr<SignalingClientImpl>
impl_; ... };

// In your .cc file:
// #include "signaling_client_impl.cc" // Or link it
// #include "include/webrtc/signaling_client.h" // Include public header

// SignalingClient::SignalingClient(const std::string& uri, const std::string&
jwt) {
//     // Create the concrete implementation instance
//     impl_ = std::make_unique<SignalingClientImpl>(uri, jwt);
// }

// SignalingClient::~SignalingClient() {
//     // Unique_ptr automatically handles deletion
// }

// void SignalingClient::connect() {
//     if (impl_) impl_->connect();
// }

// void SignalingClient::disconnect() {
//     if (impl_) impl_->disconnect();
// }
// // ... and so on for all public methods ...
// void SignalingClient::onConnected(OnConnectedHandler handler) { if (impl_)
impl_->onConnected(handler); }
// ...
*/

// If NOT using the abstract class and directly using SignalingClientImpl,
// remove the SignalingClient base class definition and rename
// SignalingClientImpl to SignalingClient.
