# whl-air Remote Driving System

This project implements a real-time remote driving system for autonomous vehicles, featuring real-time video transmission from the vehicle to a control center (cockpit) and bidirectional data communication (control commands, telemetry) using the **Google WebRTC C++ library**.

It is built with a **modular architecture**, utilizes **WebRTC Data Channels for reliable and unreliable data transfer**, and is designed for **real-time performance**, **security**, and **extensibility**.

---

## ✨ Features

-   **Real-time Video Streaming:** Low-latency video transmission from the vehicle to the cockpit via WebRTC media channels.
-   **Structured Data Communication:**
    * **Control Commands:** Send vehicle control inputs (throttle, brake, steering, gear) and high-level/emergency commands (e.g., emergency stop, pull over) from the cockpit to the vehicle.
    * **Vehicle Telemetry:** Receive real-time chassis status (speed, gear, etc.) from the vehicle in the cockpit.
    * **WebRTC Data Channels:** Data communication is handled securely and efficiently over dedicated Data Channels established within the WebRTC connection, supporting both reliable/ordered (SCTP) and unreliable/unordered (DCCP over DTLS) modes as configured.
-   **Google Protobuf:** Utilizes Protobuf for defining and serializing/deserializing structured control and telemetry messages, ensuring type safety and efficiency.
-   **Secure Transmission:** Leverages WebRTC's built-in security (DTLS/SRTP for media, DTLS for Data Channels) and TLS for WebSocket signaling.
-   **NAT Traversal:** Supports navigating network address translation using STUN and TURN servers (configurable).
-   **Web-based Cockpit Interface:** Provides a local web server serving an HTML/JavaScript interface for displaying video/status and receiving user input.
-   **Modular and Extensible Architecture:** Designed with clear interfaces for easy replacement or extension of components (e.g., different sensor inputs, control logic, signaling protocols, Web UI frameworks).

---

## 🏗️ Architecture Overview

The system consists of three main components:

1.  **Signaling Server:** A central server (typically runs on a public IP) that helps the Vehicle Client and Cockpit Client discover each other and exchange initial WebRTC signaling messages (SDP offers/answers, ICE candidates). Once signaling is complete, video and data flow directly between the clients.
2.  **Vehicle Client (`vehicle_client_app`):** Runs on the autonomous vehicle. It captures video from cameras, reads chassis status, establishes a WebRTC connection with the Cockpit Client (via the Signaling Server), sends video via a media track, sends telemetry via a Data Channel, and receives/processes control commands via another Data Channel.
3.  **Cockpit Client (`cockpit_client_app`):** Runs on the remote control station. It establishes a WebRTC connection with the Vehicle Client (via the Signaling Server), receives video (displayed in a web browser), receives telemetry via a Data Channel (displayed in the web browser), takes user input (joystick, keyboard, UI), formats control commands, and sends them via a Data Channel to the vehicle. It includes a local HTTP/WebSocket server (`transport_server`) to serve the web-based user interface (`display`) and relay data/commands between the C++ backend and the browser frontend.

```mermaid
graph TD
    subgraph Vehicle
        V[Vehicle Client App]
        VC(Camera Source)
        VS(Chassis Source)
    end

    subgraph Cockpit
        C[Cockpit Client App]
        CD(Command Handler)
        CT(Telemetry Handler)
        CTrans(Transport Server)
        CDisplay(Web Browser Display)
    end

    S[Signaling Server]

    V -->|Signaling| S
    C -->|Signaling| S
    V -->|WebRTC (Video, Data Channels)| C

    VC -->|Video Frames| V
    VS -->|Telemetry Data| V -->|Telemetry DataChannel| C -->|Telemetry Data| CT -->|Telemetry (JSON)| CTrans
    CD -->|Control Data| C -->|Control DataChannel| V -->|Control Data| CD@Vehicle[Controller]

    C -->|Commands (JSON/WS)| CTrans
    CTrans -->|Static Files, WS| CDisplay
    CDisplay -->|User Input (WS)| CTrans
    CTrans -->|Telemetry (WS)| CDisplay
````

-----

## 🚀 Quick Start

### Requirements

  - Ubuntu 20.04+ (or compatible Linux distribution)
  - C++17 compatible compiler (e.g., GCC 9+)
  - CMake 3.16+
  - OpenSSL development libraries
  - Protobuf Compiler (`protoc`)
  - Protobuf C++ runtime libraries
  - A C++ JSON library (e.g., [RapidJSON](https://github.com/Tencent/rapidjson), [nlohmann/json](https://github.com/nlohmann/json))
  - A C++ WebSocket library (e.g., [websocketpp](https://www.google.com/search?q=https://github.com/zaphoyere/websocketpp), [Boost.Beast](https://www.boost.org/doc/libs/release/libs/beast/))
  - A pre-built Google [WebRTC C++ library](https://webrtc.googlesource.com/src/) (libwebrtc) built for your target platform. Integrating libwebrtc requires careful setup and configuration.

### Build

Ensure all requirements are met and dependencies (especially the pre-built WebRTC library) are correctly referenced in your environment or CMake configuration.

```bash
# Navigate to the project root directory
cd whl-air/ # Or wherever your project is located

# Create a build directory
mkdir build && cd build

# Configure the build (adjust -DWEBRTC_ROOT if needed)
cmake .. -DCMAKE_BUILD_TYPE=Release # Add -DWEBRTC_ROOT=/path/to/your/webrtc/build if needed

# Build the project
make -j$(nproc)
```

### Configuration

Create configuration files based on the examples (not provided in skeleton, but implied by `VehicleConfig` and `CockpitConfig`) for the vehicle and cockpit clients. These files will specify signaling server addresses, peer IDs, sensor settings, DataChannel labels, ICE server configurations, etc.

  * `configs/vehicle_client_conf.txt`
  * `configs/cockpit_client_conf.txt`

Ensure the `client_id` in each config is unique and matches the `target_vehicle_id` in the cockpit config for establishing a PeerConnection. DataChannel labels (`control_channel_label`, `telemetry_channel_label`) must also match between the vehicle and cockpit configurations.

### Run

1.  **Start the Signaling Server:**
    This server needs to be accessible from both the vehicle and cockpit clients.

    ```bash
    ./signaling_server # Replace with your actual signaling server executable name
    ```

2.  **Start the Vehicle Client:**
    Run the vehicle application on the vehicle hardware.

    ```bash
    ./vehicle_client/vehicle_client_app --config ../configs/vehicle_client_conf.txt
    ```

3.  **Start the Cockpit Client:**
    Run the cockpit application on the remote control station. This will start the embedded local web server.

    ```bash
    ./cockpit_client/cockpit_client_app --config ../configs/cockpit_client_conf.txt
    ```

4.  **Access the Cockpit Web Interface:**
    Open a web browser on the cockpit machine and navigate to the address and port configured for the local `transport_server` (e.g., `http://localhost:8080/` or `http://127.0.0.1:8080/`). The JavaScript running in the browser will connect via WebSocket to the local C++ backend, initiate the WebRTC signaling process via this WebSocket connection, and display the video/status streams once the WebRTC connection is established with the vehicle.
