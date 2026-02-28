#pragma once

#include "blocks/http/api.h"
#include "blocks/json/json.h"
#include "bricks/dflags/dflags.h"
#include "bricks/net/tcp/tcp.h"
#include "bricks/sync/waitable_atomic.h"

#include "src/websockets.h"

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
  CURRENT_FIELD(timeout_read_sec, int);
  CURRENT_FIELD(timeout_write_sec, int);
  static WSConfig FromFields(std::string host = "0.0.0.0", uint16_t port = 8080,
                             int n_threads = 32, int timeout_ms = 1000,
                             int timeout_read_sec = 1,
                             int timeout_write_sec = 1) {
    WSConfig conf;
    conf.host = host;
    conf.port = port;
    conf.n_threads = n_threads;
    conf.timeout_ms = timeout_ms;
    conf.timeout_read_sec = timeout_read_sec;
    conf.timeout_write_sec = timeout_write_sec;
    return conf;
  };
};
CURRENT_STRUCT(VOMeta) {
  CURRENT_FIELD(account_id, std::string);
  CURRENT_FIELD(configuration_id, std::string);
};

CURRENT_STRUCT(VOHandshakeMessage) {
  CURRENT_FIELD(call_id, std::string);
  CURRENT_FIELD(node_selector, Optional<std::string>);
  CURRENT_FIELD(speakers, std::vector<std::string>);
  CURRENT_FIELD(meta, VOMeta);
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

std::pair<bool, std::map<std::string, std::vector<VONode>>>
parse_config(std::string conf) {
  std::map<std::string, std::vector<VONode>> mapping;
  bool is_ok = true;
  try {
    const current::json::JSONValue parsed =
        current::json::ParseJSONUniversally(conf);
    for (const current::json::JSONObject::const_element element :
         Value<current::json::JSONObject>(parsed)) {
      mapping[element.key] = ParseJSON<std::vector<VONode>>(
          current::json::AsJSON(element.value).c_str());
    }
  } catch (JSONSchemaException &e) {
    std::cout << "Error: Invalid schema in the configuration file."
              << std::endl;
    is_ok = false;
  } catch (TypeSystemParseJSONException &e) {
    std::cout << "Error: Invalid json in the configuration file." << std::endl;
    is_ok = false;
  }
  return std::make_pair(is_ok, mapping);
}

std::string safe_env(std::string key) {
  char *env = std::getenv(key.c_str());
  if (env == NULL) {
    return "";
  }
  return std::string(env);
}
