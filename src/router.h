#pragma once

#include "redis.h"
#include "vocallout.h"

class RedisSync {
  // Redis client wrapper for parsing vocallout config
protected:
  const std::string redis_key = "vocallout_config";
  RedisClient db_;

public:
  explicit RedisSync() : db_("0.0.0.0", 6379, "default", "pass") {}
  explicit RedisSync(std::string host, int port, std::string user,
                     std::string pass)
      : db_(RedisClient(host, port, user, pass)) {}

  std::pair<bool, std::map<std::string, std::vector<VONode>>> sync() {
    auto reply = db_.SendCommand("get " + redis_key);
    if (!reply.IsOK() || !reply.IsString()) {
      std::cout << "Failed to sync with redis" << std::endl;
      return std::make_pair(false,
                            std::map<std::string, std::vector<VONode>>{});
    }
    auto result = parse_config(reply.string);
    if (!result.first) {
      std::cout << "Failed to parse config" << std::endl;
      return std::make_pair(false,
                            std::map<std::string, std::vector<VONode>>{});
    }
    return result;
  }
};

class StreamRouter {
private:
  const int redis_delay_ms_ = 1000;
  current::WaitableAtomic<SharedState> safe_state_;
  WSConfig ws_config_;
  WebsocketServer server_;

  void on_connect(WebsocketClient &client) {
    auto id = client.Address() + ":" + client.Port();
    safe_state_.MutableUse([&id](SharedState &state) {
      state.channel_states[id] = 0;
      std::cout << "Client " << id << " connected" << std::endl;
    });
  }
  void on_disconnect(WebsocketClient &client) {
    auto id = client.Address() + ":" + client.Port();
    safe_state_.MutableUse([&id](SharedState &state) {
      // close the connection and clean buffers
      state.channel_states.erase(id);
      state.channels.erase(id);
      std::cout << "Client " << id << " disconnected" << std::endl;
    });
  }
  void on_data(WebsocketClient &client, std::string_view data, int type) {
    auto id = client.Address() + ":" + client.Port();
    safe_state_.MutableUse([&id, &data, &client](SharedState &state) {
      // do the handshake with ASR or stream audio
      if (!state.channel_exists(id)) {
        std::cout << "Channel not found" << std::endl;
        client.Close();
      }
      try {
        if (state.channel_states[id] == 0) {
          // Parse init message and configure the channel
          auto to_parse = std::string(data.begin(), data.end());
          VOHandshakeMessage handshake =
              ParseJSON<VOHandshakeMessage>(to_parse);

          auto selector =
              handshake.meta.account_id + ":" + handshake.meta.configuration_id;
          // Compatibility with 1.0
          if (Exists(handshake.node_selector))
            selector = Value(handshake.node_selector);

          auto node = state.next_node(id, selector);
          state.channels[id] =
              Channel{std::make_unique<current::net::Connection>(
                          current::net::Connection(current::net::ClientSocket(
                              node.host, node.port))),
                      selector, 0};
          // Do the handshake: send init message and read sync byte
          state.channels[id].conn->BlockingWrite(data.data(), data.size(),
                                                 true);
          uint8_t read_to = 0;
          state.channels[id].conn->BlockingRead(&read_to, 1);
          // Update channel state - ready to stream
          state.channel_states[id] = 1;
        } else {
          state.channels[id].conn->BlockingWrite(data.data(), data.size(),
                                                 true);
        }
      } catch (const current::Exception &e) {
        std::cout << "error"
                  << ": " << e.OriginalDescription() << " "
                  << e.DetailedDescription() << std::endl;
        // PBX is responsible for reconnects, in case of error we have to drop
        // the connection and let PBX decide/reconnect if needed
        client.Close();
      }
      // Finish current buffer transmission and shutdown the connection
      if (state.die) {
        client.Close();
      }
    });
  }

public:
  explicit StreamRouter(std::map<std::string, std::vector<VONode>> config,
                        WSConfig ws_config)
      : safe_state_(current::WaitableAtomic<SharedState>(
            SharedState::FromMapping(config))),
        ws_config_(ws_config),
        server_(WebsocketServer(
            [this](WebsocketClient &client, std::string_view data, int type) {
              on_data(client, data, type);
            },
            [this](WebsocketClient &client) { on_connect(client); },
            [this](WebsocketClient &client) { on_disconnect(client); },
            ws_config_.port, ws_config_.host, ws_config_.n_threads,
            ws_config_.timeout_ms)) {
    server_.start();
    std::cout << "Started stream router on " << ws_config_.host << ":"
              << ws_config_.port << std::endl;
  };

  void Join() {
    std::string host = safe_env("REDIS_HOST");
    int port = std::atoi(safe_env("REDIS_PORT").c_str());
    std::string user = safe_env("REDIS_USER");
    std::string pass = safe_env("REDIS_PWD");
    if (host.length() > 0 && port > 0 && user.length() > 0 &&
        pass.length() > 0) {
      // Start redis sync thread if redis credentials and url are available
      auto sync_thread = std::thread([host, port, user, pass, this]() {
        RedisProc(host, port, user, pass);
      });
      sync_thread.join();
    }
  }

  void RedisProc(std::string host, int port, std::string user,
                 std::string pass) {
    auto sync_ = RedisSync(host, port, user, pass);
    std::cout << "Started redis sync" << std::endl;
    while (true) {
      // check for gracefull stop
      if (safe_state_.ImmutableScopedAccessor()->die) {
        std::cout << "Stop redis sync" << std::endl;
        break;
      }

      // sync with redis
      auto result = sync_.sync();
      if (result.first) {
        // update config after successfull sync
        safe_state_.MutableUse([&result](SharedState &state) {
          for (auto &[key, value] : result.second) {
            state.mapping[key] = value;
          }
        });
      }

      // wait for next round
      std::this_thread::sleep_for(std::chrono::milliseconds(redis_delay_ms_));
    }
  }

  void BreakConnections() {
    safe_state_.MutableUse([](SharedState &state) { state.die = true; });
  }
  uint32_t StreamsCount() const {
    return safe_state_.ImmutableScopedAccessor()->channels.size();
  }
  StreamRouter(const StreamRouter &) = delete;
  ~StreamRouter() = default;
};
