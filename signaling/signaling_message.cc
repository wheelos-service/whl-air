// signaling_client/src/signaling_message.cc

#include "include/webrtc/signaling_message.h"  // Assuming include path

#include <iostream>
#include <map>  // For the maps
#include <stdexcept>
#include <string>

// --- Include a JSON library ---
// You MUST include and use a C++ JSON library here.
// Examples:
// #include <nlohmann/json.hpp>
// using json = nlohmann::json;
//
// #include <rapidjson/document.h>
// #include <rapidjson/writer.h>
// #include <rapidjson/stringbuffer.h>
// #include <rapidjson/error/en.h> // For parse error messages
// using namespace rapidjson;

namespace {
// Map enum to string
const std::map<SignalMessage::Type, std::string> typeToStringMap = {
    {SignalMessage::Type::UNKNOWN, "unknown"},
    {SignalMessage::Type::JOIN, "join"},
    {SignalMessage::Type::LEAVE, "leave"},
    {SignalMessage::Type::OFFER, "offer"},
    {SignalMessage::Type::ANSWER, "answer"},
    {SignalMessage::Type::CANDIDATE, "candidate"}};

// Map string to enum
const std::map<std::string, SignalMessage::Type> stringToTypeMap = {
    {"unknown", SignalMessage::Type::UNKNOWN},
    {"join", SignalMessage::Type::JOIN},
    {"leave", SignalMessage::Type::LEAVE},
    {"offer", SignalMessage::Type::OFFER},
    {"answer", SignalMessage::Type::ANSWER},
    {"candidate", SignalMessage::Type::CANDIDATE}};
}  // anonymous namespace

// Implementation of the static member function
SignalMessage::Type SignalMessage::StringToType(const std::string& typeStr) {
  auto it = stringToTypeMap.find(typeStr);
  if (it != stringToTypeMap.end()) {
    return it->second;
  }
  return SignalMessage::Type::UNKNOWN;
}

std::string SerializeSignalMessage(const SignalMessage& message) {
  // TODO: Implement using a JSON library (e.g., nlohmann/json or RapidJSON)
  // This is a skeleton and does NOT handle quotes, special characters, or
  // complex nested structures correctly.
  std::string json_str = "{";
  json_str += "\"type\":\"" + SignalMessage::TypeToString(message.type) + "\",";
  json_str += "\"from\":\"" + message.from + "\",";
  json_str += "\"to\":\"" + message.to + "\"";

  if (message.sdp) json_str += ",\"sdp\":\"" + *message.sdp + "\"";
  if (message.candidate) {
    json_str += ",\"candidate\":{";
    json_str += "\"candidate\":\"" + *message.candidate + "\"";
    if (message.sdpMid) json_str += ",\"sdpMid\":\"" + *message.sdpMid + "\"";
    if (message.sdpMlineIndex)
      json_str +=
          ",\"sdpMlineIndex\":" + std::to_string(*message.sdpMlineIndex);
    json_str += "}";
  }
  if (message.reason) json_str += ",\"reason\":\"" + *message.reason + "\"";

  json_str += "}";

  // std::cout << "Serialized (Skeleton): " << json_str << std::endl; // Debug
  // print
  return json_str;
}

std::optional<SignalMessage> DeserializeSignalMessage(const std::string& data) {
  // TODO: Implement using a JSON library (e.g., nlohmann/json or RapidJSON)
  // This is a fragile skeleton parser. It will fail for most real-world JSON
  // messages. You MUST replace this with a proper JSON library parser.

  // Example conceptual using nlohmann/json with error handling:
  /*
  try {
      auto j = json::parse(data);
      SignalMessage msg;
      // Use .value() to get fields with default values and avoid exceptions for
  missing fields msg.type = SignalMessage::StringToType(j.value("type",
  "unknown")); msg.from = j.value("from", ""); msg.to = j.value("to", "");

      // Access optional fields safely
      if (j.count("sdp") && j["sdp"].is_string()) msg.sdp =
  j["sdp"].get<std::string>();

      if (j.count("candidate") && j["candidate"].is_object()) {
          const auto& cand_obj = j["candidate"];
          if (cand_obj.count("candidate") && cand_obj["candidate"].is_string())
  msg.candidate = cand_obj["candidate"].get<std::string>(); if
  (cand_obj.count("sdpMid") && cand_obj["sdpMid"].is_string()) msg.sdpMid =
  cand_obj["sdpMid"].get<std::string>();
          // Handle sdpMlineIndex potentially being a number or string
          if (cand_obj.count("sdpMlineIndex")) {
               if (cand_obj["sdpMlineIndex"].is_number_integer()) {
                    msg.sdpMlineIndex = cand_obj["sdpMlineIndex"].get<int>();
               } else if (cand_obj["sdpMlineIndex"].is_string()) {
                   try { msg.sdpMlineIndex =
  std::stoi(cand_obj["sdpMlineIndex"].get<std::string>()); } catch(...) {} //
  Attempt conversion
               }
          }
      }
      if (j.count("reason") && j["reason"].is_string()) msg.reason =
  j["reason"].get<std::string>();

      // Note: If the server sends custom messages (like 'connected', 'error')
  that don't fit
      // the standard SignalMessage types/fields, you'll need to handle them
  here.
      // You could add new enum types or process them when msg.type is UNKNOWN.

      // std::cout << "Deserialized (Conceptual): Type=" <<
  SignalMessage::TypeToString(msg.type) << ", From=" << msg.from << std::endl;
  // Debug print return msg;

  } catch (const json::exception& e) {
      std::cerr << "DeserializeSignalMessage Error: JSON parse failed: " <<
  e.what() << ". Data: " << data << std::endl; return std::nullopt; // Indicate
  parsing failure } catch (...) { std::cerr << "DeserializeSignalMessage Error:
  Unknown error during parsing. Data: " << data << std::endl; return
  std::nullopt;
  }
  */

  // --- Simple Skeleton Parser (Highly Fragile) ---
  SignalMessage msg;
  try {
    // This is a very basic parser assuming field order and simple values. NOT
    // ROBUST.
    size_t type_pos = data.find("\"type\":\"");
    if (type_pos != std::string::npos) {
      size_t start = type_pos + 8;
      size_t end = data.find("\"", start);
      if (end != std::string::npos) {
        msg.type = SignalMessage::StringToType(data.substr(start, end - start));
      }
    }

    size_t from_pos = data.find("\"from\":\"");
    if (from_pos != std::string::npos) {
      size_t start = from_pos + 8;
      size_t end = data.find("\"", start);
      if (end != std::string::npos) {
        msg.from = data.substr(start, end - start);
      }
    }

    size_t to_pos = data.find("\"to\":\"");
    if (to_pos != std::string::npos) {
      size_t start = to_pos + 6;
      size_t end = data.find("\"", start);
      if (end != std::string::npos) {
        msg.to = data.substr(start, end - start);
      }
    }

    // Add similar simple logic for sdp, candidate, etc. - this gets complex
    // fast.

    // std::cout << "Deserialized (Skeleton): Type=" <<
    // SignalMessage::TypeToString(msg.type) << ", From=" << msg.from << ", To="
    // << msg.to << std::endl; // Debug print
    return msg;

  } catch (const std::exception& e) {
    std::cerr << "DeserializeSignalMessage Error: Skeleton parsing failed: "
              << e.what() << ". Data: " << data << std::endl;
    return std::nullopt;
  } catch (...) {
    std::cerr << "DeserializeSignalMessage Error: Unknown error during "
                 "skeleton parsing. Data: "
              << data << std::endl;
    return std::nullopt;
  }
}
