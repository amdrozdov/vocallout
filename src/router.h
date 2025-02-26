#pragma once

#include "vocallout.h"

class StreamRouter {
private:
  current::WaitableAtomic<SharedState> safe_state_;
  const WSConfig ws_config_;
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

          auto node = state.next_node(id, handshake.node_selector);
          state.channels[id] =
              Channel{std::make_unique<current::net::Connection>(
                          current::net::Connection(current::net::ClientSocket(
                              node.host, node.port))),
                      handshake.node_selector, 0};
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
            [this](WebsocketClient &client, std::string_view data,
                   int type) { on_data(client, data, type); },
            [this](WebsocketClient &client) { on_connect(client); },
            [this](WebsocketClient &client) { on_disconnect(client); },
            ws_config_.port, ws_config_.host, ws_config_.n_threads,
            ws_config_.timeout_ms)) {
    server_.start();
    std::cout << "Started stream router on " << ws_config_.host << ":"
              << ws_config_.port << std::endl;
  };
  void BreakConnections() {
    safe_state_.MutableUse([](SharedState &state) { state.die = true; });
  }
  uint32_t StreamsCount() const {
    return safe_state_.ImmutableScopedAccessor()->channels.size();
  }
  StreamRouter(const StreamRouter &) = delete;
  ~StreamRouter() = default;
};
