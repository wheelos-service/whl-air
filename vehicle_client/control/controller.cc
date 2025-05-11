// Copyright 2025 WheelOS. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//  Created Date: 2025-04-27
//  Author: daohu527

#include "include/control/controller.h"

#include <iostream>

// Include generated protobuf headers
// #include "control/proto/control_command.pb.h"
// #include "control/proto/emergency_command.pb.h"

// Need Protobuf library for deserialization
// #include <google/protobuf/message.h>

class ApolloController : public IController {
 public:
  ApolloController() {
    std::cout << "ApolloController created (skeleton)" << std::endl;
  }

  void processControlCommand(
      const autodev::remote::control::ControlCommand& command) override {
    std::cout << "ApolloController: Processing ControlCommand (skeleton):"
              << std::endl;
    // TODO: Implement logic to apply command to vehicle actuators
    // This would involve interfacing with low-level vehicle control systems
    // (e.g., CAN bus commands)
    std::cout << "  Acceleration: " << command.acceleration() << std::endl;
    std::cout << "  Braking: " << command.braking() << std::endl;
    std::cout << "  Steering: " << command.steering_angle() << std::endl;
    std::cout << "  Gear: " << command.gear() << std::endl;
  }

  void processEmergencyCommand(
      const autodev::remote::control::EmergencyCommand& command) override {
    std::cout << "ApolloController: Processing EmergencyCommand (skeleton):"
              << std::endl;
    // TODO: Implement logic for emergency actions
    std::cout << "  Type: " << command.type()
              << std::endl;  // Need enum mapping
    std::cout << "  Reason: " << command.reason() << std::endl;

    // Example:
    // if (command.type() == autodev::remote::control::EMERGENCY_STOP) {
    //     std::cerr << "!!! EMERGENCY STOP INITIATED !!!" << std::endl;
    //     // Activate emergency brakes, cut power, etc.
    // } else if (command.type() == autodev::remote::control::PULL_OVER) {
    //      std::cout << "Executing PULL OVER maneuver." << std::endl;
    //     // Execute safe pull-over sequence
    // }
  }
};
