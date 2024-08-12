#pragma once

#include "blocks/http/api.h"
#include "blocks/json/json.h"
#include "bricks/dflags/dflags.h"
#include "bricks/net/tcp/tcp.h"
#include "bricks/sync/waitable_atomic.h"

#include "websockets.h"

const std::string default_selector = "default";

CURRENT_STRUCT(VONode) {
  CURRENT_FIELD(host, std::string);
  CURRENT_FIELD(port, uint16_t);
  static VONode Create(std::string host, uint16_t port) {
    VONode v;
    v.host = host;
    v.port = port;
    return v;
  }
};

CURRENT_STRUCT(VOStatus) {
  CURRENT_FIELD(status, std::string);
  CURRENT_FIELD(live_streams, uint32_t);
  static VOStatus Response(std::string msg, uint32_t live_streams) {
    VOStatus v;
    v.status = msg;
    v.live_streams = live_streams;
    return v;
  }
};

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

CURRENT_STRUCT(WSConfig) {
protected:
  CURRENT_DEFAULT_CONSTRUCTOR(WSConfig){};

public:
  CURRENT_FIELD(host, std::string);
  CURRENT_FIELD(port, uint16_t);
  CURRENT_FIELD(n_threads, int);
  CURRENT_FIELD(timeout_ms, int);
  static WSConfig FromFields(std::string host = "0.0.0.0", uint16_t port = 8080,
                             int n_threads = 32, int timeout_ms = 1000) {
    WSConfig conf;
    conf.host = host;
    conf.port = port;
    conf.n_threads = n_threads;
    conf.timeout_ms = timeout_ms;
    return conf;
  };
};

CURRENT_STRUCT(VOHandshakeMessage) {
  CURRENT_FIELD(call_id, std::string);
  CURRENT_FIELD(node_selector, std::string);
  CURRENT_FIELD(speakers, std::vector<std::string>);
};

struct Channel final {
  std::unique_ptr<current::net::Connection> conn;
  std::string node_selector;
  uint32_t round_robin_id;
};

struct SharedState final {
  bool die = false;
  std::map<std::string, Channel> channels;
  std::map<std::string, uint32_t> channel_states;
  std::map<std::string, std::vector<VONode>> mapping;
  bool channel_exists(const std::string &channel_id) const {
    return channel_states.find(channel_id) != channel_states.end();
  }
  VONode next_node(const std::string &id, const std::string &selector) {
    std::string node_selector = default_selector;
    if (mapping.find(selector) != mapping.end()) {
      node_selector = selector;
    }
    channels[id].round_robin_id++;
    if (channels[id].round_robin_id >= mapping[node_selector].size()) {
      channels[id].round_robin_id = 0;
    }
    return mapping[node_selector][channels[id].round_robin_id];
  }
  static struct SharedState
  FromMapping(std::map<std::string, std::vector<VONode>> mapping) {
    SharedState state;
    state.mapping = mapping;
    return state;
  }
};
