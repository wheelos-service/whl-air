#ifndef CAMERA_SOURCE_H
#define CAMERA_SOURCE_H

#include <chrono>  // For timestamp
#include <cstdint>
#include <functional>
#include <memory>  // For std::shared_ptr
#include <string>
#include <vector>

// Use namespace for better organization
namespace autodev {
namespace remote {
namespace sensors {

// Forward declaration (if VideoFrame is defined elsewhere)
// struct VideoFrame;

// Simple structure to hold video frame data and metadata
// In a real system, this might be more complex (e.g., include stride, pixel
// format enum, etc.)
struct VideoFrame {
  std::vector<uint8_t> data;
  uint32_t width = 0;
  uint32_t height = 0;
  // Use a steady_clock timestamp for relative timing
  std::chrono::steady_clock::time_point timestamp;
  // Add format info, e.g.: VideoFormat format;

  // Optional: Constructor for convenience
  VideoFrame(std::vector<uint8_t> d, uint32_t w, uint32_t h,
             std::chrono::steady_clock::time_point ts)
      : data(std::move(d)), width(w), height(h), timestamp(ts) {}

  // Default constructor needed for shared_ptr
  VideoFrame() = default;
};

// Interface for a video source (e.g., camera)
class ICameraSource {
 public:
  virtual ~ICameraSource() = default;

  // Callback type for delivering captured video frames.
  // Uses std::shared_ptr for VideoFrame to manage memory efficiently
  // if frames are processed asynchronously.
  // IMPORTANT: Implementations must guarantee this handler is only called
  // while capture is active (between startCapture and stopCapture),
  // and stopCapture must block until all pending callbacks have finished
  // or been discarded. The handler may be called from a different thread
  // than the one that called startCapture; handle synchronization accordingly.
  using OnFrameCapturedHandler =
      std::function<void(std::shared_ptr<VideoFrame>)>;

  // Initializes the camera source (e.g., opens the device, sets format).
  // Returns true on success, false on failure.
  virtual bool init(const std::string& device_path, uint32_t width,
                    uint32_t height, uint32_t fps) = 0;

  // Starts capturing video frames. Frames are delivered asynchronously
  // via the previously set handler. Returns true on success.
  // The handler MUST be set via setOnFrameCapturedHandler BEFORE calling
  // startCapture.
  virtual bool startCapture() = 0;

  // Stops capturing. This method should block until the capture thread (if any)
  // has stopped and no more frame handlers will be invoked.
  virtual void stopCapture() = 0;

  // Sets the handler function to be called when a frame is captured.
  // This must be called AFTER init() and BEFORE startCapture().
  virtual void setOnFrameCapturedHandler(OnFrameCapturedHandler handler) = 0;

  // Gets camera parameters (Optional, might be available after init)
  virtual uint32_t getWidth() const = 0;
  virtual uint32_t getHeight() const = 0;
  virtual uint32_t getFps() const = 0;

  // Optional: Add an error handler callback
  // using OnErrorHandler = std::function<void(const std::string& error_msg)>;
  // virtual void setOnErrorHandler(OnErrorHandler handler) = 0;

  // Optional: Get the underlying media source object if needed for direct
  // WebRTC integration (e.g., rtc::VideoSourceInterface).
  // This ties the interface to WebRTC specifics, consider if this is desired.
  // virtual rtc::VideoSourceInterface* getRtcVideoSource() = 0;
};

}  // namespace sensors
}  // namespace remote
}  // namespace autodev

#endif  // CAMERA_SOURCE_H
