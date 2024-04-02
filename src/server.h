#pragma once

#include "blocks/http/api.h"
#include "blocks/json/json.h"
#include "bricks/dflags/dflags.h"
#include "bricks/net/tcp/tcp.h"
#include "bricks/sync/waitable_atomic.h"

struct ControlSignal final {
  // TODO:
  // think about/implement input/output channel switch command (for call
  // redirects)
  bool stop = false;
};

struct joiner_state final {
  bool die = false;
  std::vector<std::thread> to_join;
};

struct SharedState final {
  bool die = false;
  std::unordered_set<std::string> to_kill;
  std::map<std::string, std::thread> threads;
  std::map<std::string, std::string> channel_control;
  bool channel_exists(const std::string &channel_id) const {
    return channel_control.find(channel_id) != channel_control.end();
  }
};

CURRENT_STRUCT(VOChannelCreate) {
  CURRENT_FIELD(id, std::string);
  CURRENT_FIELD(in_port, uint16_t);
  CURRENT_FIELD(out_host, std::string);
  CURRENT_FIELD(out_port, uint16_t);
};
CURRENT_STRUCT(VOChannelDelete) { CURRENT_FIELD(id, std::string); };
CURRENT_STRUCT(VOResponse) {
  CURRENT_FIELD(msg, std::string);
  CURRENT_FIELD(is_error, bool);
  static VOResponse OK(std::string msg) {
    VOResponse resp;
    resp.msg = std::move(msg);
    resp.is_error = false;
    return resp;
  };
  static VOResponse Error(std::string msg) {
    VOResponse resp;
    resp.msg = std::move(msg);
    resp.is_error = true;
    return resp;
  };
};