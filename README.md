# whl-air for Autonomous Driving
This project implements real-time video transmission from autonomous vehicles to a control center using the **Google WebRTC C++ library**.
It features **modular architecture**, **separate video and control channels**, and is optimized for **real-time performance**, **security**, and **scalability**.

---

## ✨ Features

- Real-time video streaming via WebRTC
- Separate TCP/UDP control command channel
- Secure transmission (DTLS/SRTP, TLS)
- NAT traversal support (STUN/TURN)
- Configurable and extensible architecture

---

## 🚀 Quick Start

### Requirements
- Ubuntu 20.04+ / MacOS
- C++17, CMake 3.16+, OpenSSL
- Pre-built [WebRTC library](https://webrtc.googlesource.com/src/)

### Build & Run

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

Start the server:

```bash
./server_main --config ./configs/server_config.json
```

Start the client:

```bash
./client_main --config ./configs/client_config.json
```

---

## 📦 Project Structure

- `common/`: Config & logging utilities
- `signaling/`: WebRTC signaling (SDP, ICE)
- `transport/`: PeerConnection management
- `video/`: Video capture and rendering
- `control/`: TCP/UDP control channels
- `server/`: Client entry points
- `client/`: Server entry points

---
