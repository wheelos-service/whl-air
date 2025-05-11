#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

#include "include/sensors/chassis_source.h"

// Include CAN bus headers (e.g., SocketCAN)
// #include <linux/can.h>
// #include <linux/can/raw.h>
// #include <net/if.h>
// #include <sys/ioctl.h>
// #include <sys/socket.h>
// #include <unistd.h>

// Include generated protobuf header
// #include "proto/chassis.pb.h"

class CanBusChassisSource : public IChassisSource {
 public:
  CanBusChassisSource() {
    std::cout << "CanBusChassisSource created (skeleton)" << std::endl;
  }

  ~CanBusChassisSource() override {
    stopUpdates();
    std::cout << "CanBusChassisSource destroyed (skeleton)" << std::endl;
  }

  bool init(/* config parameters, e.g., interface name */) override {
    std::cout << "CanBusChassisSource: Initializing (skeleton)." << std::endl;
    // TODO: Implement SocketCAN initialization
    // device_ = "can0"; // From config
    // s_ = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    // if (s_ < 0) { perror("SocketCAN socket error"); return false; }
    // // Bind socket to interface, etc.

    std::cout << "CanBusChassisSource: Initialization successful (simulated)."
              << std::endl;
    return true;  // Simulate success
  }

  bool startUpdates(OnChassisStateUpdatedHandler handler) override {
    if (!handler) {
      std::cerr << "CanBusChassisSource: Cannot start updates, no handler set."
                << std::endl;
      return false;
    }
    if (isUpdating_) {
      std::cout << "CanBusChassisSource: Updates already started." << std::endl;
      return true;
    }
    std::cout << "CanBusChassisSource: Starting updates (skeleton)."
              << std::endl;
    onChassisStateUpdatedHandler_ = handler;
    isUpdating_ = true;

    // TODO: Start CAN frame receiving thread/process
    updateThread_ = std::thread(&CanBusChassisSource::updateLoop, this);

    return true;
  }

  void stopUpdates() override {
    if (!isUpdating_) return;
    std::cout << "CanBusChassisSource: Stopping updates (skeleton)."
              << std::endl;
    isUpdating_ = false;
    if (updateThread_.joinable()) {
      updateThread_.join();
    }
    // TODO: Clean up SocketCAN resources (close socket)
  }

 private:
  std::atomic<bool> isUpdating_ = false;
  std::thread updateThread_;
  OnChassisStateUpdatedHandler onChassisStateUpdatedHandler_;
  // TODO: SocketCAN file descriptor, interface name
  // int s_ = -1;
  // std::string device_;

  void updateLoop() {
    std::cout << "CanBusChassisSource: Update loop started (skeleton)."
              << std::endl;
    // TODO: Implement CAN frame reading loop
    // Read frames from s_
    // Parse CAN frames to extract speed, gear, etc.
    // Populate autodev::remote::chassis::Chassis protobuf message

    // For skeleton, simulate updates with dummy data
    autodev::remote::chassis::Chassis dummy_state;
    dummy_state.set_speed_mps(0.0);
    dummy_state.set_gear(0);  // NEUTRAL
    dummy_state.set_throttle_percentage(0.0);
    dummy_state.set_brake_percentage(0.0);
    dummy_state.set_steering_percentage(0.0);

    std::chrono::milliseconds update_interval(100);  // Simulate 10Hz update

    double speed = 0.0;
    int gear = 0;

    while (isUpdating_) {
      // Simulate data change
      speed += 0.1;
      if (speed > 10.0) speed = 0.0;
      gear = (int)(speed / 2.0) % 3;  // 0, 1, 2

      dummy_state.set_speed_mps(speed);
      dummy_state.set_gear(gear);

      // Call the handler with the updated state
      if (onChassisStateUpdatedHandler_) {
        onChassisStateUpdatedHandler_(dummy_state);
      }

      // Simulate update interval
      std::this_thread::sleep_for(update_interval);
    }
    std::cout << "CanBusChassisSource: Update loop stopped (skeleton)."
              << std::endl;
  }

  // Prevent copying
  CanBusChassisSource(const CanBusChassisSource&) = delete;
  CanBusChassisSource& operator=(CanBusChassisSource&) = delete;
};
