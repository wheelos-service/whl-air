#ifndef SIGNALING_MESSAGE_H
#define SIGNALING_MESSAGE_H

#include <optional>  // Requires C++17 or later
#include <string>

// Represents a structured signaling message payload
struct SignalMessage {
  enum class Type {
    UNKNOWN,
    JOIN,       // A peer joining the room/session
    LEAVE,      // A peer leaving
    OFFER,      // WebRTC SDP Offer
    ANSWER,     // WebRTC SDP Answer
    CANDIDATE,  // WebRTC ICE Candidate
    // Add other application-specific message types as needed (e.g., CHAT,
    // PRESENCE)
  };

  Type type = Type::UNKNOWN;
  std::string from;  // Sender's peer ID. Should always be present.
  std::string to;  // Receiver's peer ID (or empty for broadcast/room messages).

  // Payload data - use optional as not all messages have these fields
  std::optional<std::string> sdp;        // For OFFER, ANSWER
  std::optional<std::string> candidate;  // For CANDIDATE
  std::optional<std::string> sdpMid;     // For CANDIDATE
  std::optional<int> sdpMlineIndex;      // For CANDIDATE
  std::optional<std::string> reason;     // For LEAVE messages, errors, etc.
  // Add more optional fields as needed for other message types

  // Constructor for convenience (example for sending simple messages)
  SignalMessage(Type msg_type = Type::UNKNOWN, const std::string& sender = "",
                const std::string& receiver = "")
      : type(msg_type), from(sender), to(receiver) {}

  // Helper functions (implementation would be in .cc file)
  static std::string TypeToString(Type type);
  static Type StringToType(const std::string& typeStr);
};

// Helper function declarations for converting SignalMessage to/from string
// (e.g., JSON) Implementation would be in signaling_message.cc
std::string SerializeSignalMessage(const SignalMessage& message);
// Returns std::nullopt if deserialization fails
std::optional<SignalMessage> DeserializeSignalMessage(const std::string& data);

#endif  // SIGNALING_MESSAGE_H
