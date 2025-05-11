#ifndef CHASSIS_SOURCE_H
#define CHASSIS_SOURCE_H

#include <functional>
#include <memory>
#include <string>

// Include the generated Protobuf header
// Ensure this path is correct for your build system
#include "proto/chassis.pb.h"

// Use namespace for better organization
namespace autodev {
namespace remote {
namespace sensors {

// Interface for a vehicle chassis data source
class IChassisSource {
 public:
  virtual ~IChassisSource() = default;

  // Callback type for delivering updated chassis state.
  // Using a const reference to the Protobuf message is efficient.
  // If asynchronous processing *after* the handler returns requires shared
  // ownership, consider using std::shared_ptr<const
  // autodev::remote::chassis::Chassis>.
  //
  // IMPORTANT: Implementations must guarantee this handler is only called
  // while updates are active (after a successful startUpdates and before
  // stopUpdates returns), and stopUpdates must block until all pending
  // callbacks have finished or been discarded. The handler may be called from a
  // different thread than the one that called startUpdates; handle
  // synchronization accordingly.
  using OnChassisStateUpdatedHandler =
      std::function<void(const autodev::remote::chassis::Chassis& state)>;

  // Initializes the chassis source (e.g., connects to CAN bus, sets
  // parameters). Returns true on success, false on failure. config_params could
  // be a struct or individual parameters.
  virtual bool init(const std::string& can_interface /*, other config */) = 0;

  // Starts reporting chassis state updates. The provided handler will be called
  // periodically (e.g., at a configured frequency or upon state change).
  // Returns true on success.
  // If updates are already active, calling this might return false or overwrite
  // the handler depending on the implementation policy (returning false is
  // simpler). The provided handler must remain valid and callable as long as
  // updates are active.
  virtual bool startUpdates(OnChassisStateUpdatedHandler handler) = 0;

  // Stops reporting updates. This method should block until the update thread
  // (if any) has stopped and no more state handlers will be invoked. It's safe
  // to call stopUpdates() even if updates are not active.
  virtual void stopUpdates() = 0;

  // Optional: Get the current state immediately (pull model).
  // This should be thread-safe if called from a different thread than the
  // update thread. Returns a default/empty Chassis message if not initialized
  // or updates are stopped.
  virtual autodev::remote::chassis::Chassis getCurrentState() const = 0;

  // Optional: Add an error handler callback
  // using OnErrorHandler = std::function<void(const std::string& error_msg)>;
  // virtual void setOnErrorHandler(OnErrorHandler handler) = 0;
};

}  // namespace sensors
}  // namespace remote
}  // namespace autodev

#endif  // CHASSIS_SOURCE_H
