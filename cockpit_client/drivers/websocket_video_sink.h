#include <iostream>  // Logging
#include <memory>    // For shared_ptr
#include <string>
#include <vector>

#include "transport/transport_server.h"         // Dependency
#include "webrtc/api/media_stream_interface.h"  // For webrtc::VideoSinkInterface
#include "webrtc/api/video/i420_buffer.h"       // If handling I420 format
#include "webrtc/api/video/video_frame.h"       // For webrtc::VideoFrame
#include "webrtc/rtc_base/platform_thread.h"    // Potentially needed
#include "webrtc/rtc_base/thread.h"  // Potentially needed for thread checks/marshalling
// Include a library for image encoding, e.g., libjpeg-turbo
// #include <turbojpeg.h> // Example

namespace remote {
namespace drivers {

// A WebRTC VideoSink implementation that encodes frames (e.g., to JPEG)
// and sends them over WebSocket via the TransportServer.
class WebSocketVideoSink
    : public webrtc::VideoSinkInterface<webrtc::VideoFrame> {
 public:
  // Needs access to the TransportServer to send data, and potentially config
  // The TransportServer must outlive this sink. Using raw pointer here assumes
  // the App manages lifetime correctly, or use shared_ptr if appropriate.
  WebSocketVideoSink(
      std::shared_ptr<autodev::remote::transport::ITransportServer>
          transport_server,
      const std::string& stream_id  // Identifier for this video stream
      /* , potentially config for encoding */)
      : transport_server_(transport_server),
        stream_id_(stream_id)
  // , tj_handle_(nullptr) // If using libjpeg-turbo
  {
    // TODO: Initialize encoder here, e.g., tjInitCompress(&tj_handle_)
    std::cout << "WebSocketVideoSink created for stream: " << stream_id_
              << std::endl;
  }

  ~WebSocketVideoSink() override {
    // TODO: Destroy encoder here, e.g., tjDestroy(tj_handle_)
    std::cout << "WebSocketVideoSink destroyed for stream: " << stream_id_
              << std::endl;
  }

  // This method is called by WebRTC when a new frame is available.
  // It will be called on a WebRTC internal thread. MUST BE THREAD-SAFE.
  void OnFrame(const webrtc::VideoFrame& frame) override {
    // std::cout << "Sink received frame: " << frame.width() << "x" <<
    // frame.height() << std::endl;

    if (!transport_server_) {
      // Server might be stopped or destroyed
      return;
    }

    // TODO: Implement frame conversion and encoding
    // Example (conceptual) using JPEG encoding:
    // - Get pixel data from frame (e.g., frame.video_frame_buffer()->ToI420()
    // then access planes)
    // - Encode I420/other format to JPEG bytes using a library (libjpeg-turbo,
    // OpenCV, etc.)
    // - Store JPEG bytes in a std::vector<char>

    std::vector<char> jpeg_data;  // Placeholder for encoded JPEG data
    // Dummy data for skeleton:
    std::string dummy_jpeg_string = "DUMMY_JPEG_DATA_" + stream_id_ + "_" +
                                    std::to_string(frame.timestamp_us());
    jpeg_data.assign(dummy_jpeg_string.begin(), dummy_jpeg_string.end());

    // Optional: Add a header/wrapper to the message to identify the stream or
    // frame type This helps the UI differentiate video data from other
    // WebSocket messages. Example: A simple header could be
    // "VIDEO:<stream_id>:<frame_data>"
    std::vector<char> message_with_header;
    std::string header_str = "VIDEO:" + stream_id_ + ":";
    message_with_header.insert(message_with_header.end(), header_str.begin(),
                               header_str.end());
    message_with_header.insert(message_with_header.end(), jpeg_data.begin(),
                               jpeg_data.end());

    // Send the encoded frame data to all connected WebSocket clients via the
    // TransportServer. TransportServer::sendToAllWebSocketClients MUST BE
    // THREAD-SAFE.
    bool success = transport_server_->sendToAllWebSocketClients(
        message_with_header);  // Use sendToAll for broadcast

    if (!success) {
      // std::cerr << "Sink failed to send video frame for stream: " <<
      // stream_id_ << std::endl; Handle send failure (e.g., clients
      // disconnected)
    }
  }

 private:
  // Shared pointer to the TransportServer. Ensures server is valid while sink
  // is active.
  std::shared_ptr<autodev::remote::transport::ITransportServer>
      transport_server_;
  std::string stream_id_;  // Identifier for this video stream (e.g.,
                           // "camera_front", "camera_rear")

  // TODO: Encoder handle, e.g., tjhandle tj_handle_;
};

}  // namespace drivers
}  // namespace remote
