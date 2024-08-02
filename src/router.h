#pragma once

#include "vocallhout.h"

class StreamRouter {
private:
  current::WaitableAtomic<SharedState> safe_state_;
  std::string host_;
  uint16_t port_;
  int n_threads_;
  int timeout_ms_;

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
  void on_data(WebsocketClient &client, std::vector<uint8_t> data, int type) {
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
          VOHandshakeMessage handshake =
              ParseJSON<VOHandshakeMessage>((const char *)(data.data()));
          auto node = state.next_node(id, handshake.node_selector);
          state.channels[id] =
              Channel{std::make_unique<current::net::Connection>(
                          current::net::Connection(current::net::ClientSocket(
                              node.host, node.port))),
                      handshake.node_selector, 0};
          // Do the handshake: send init message and read sync byte
          state.channels[id].conn->BlockingWrite(data.data(), data.size(),
                                                 true);
          state.channels[id].conn->BlockingRead(&data[0], 1);
          // Update channel state - ready to stream
          state.channel_states[id] = 1;
        } else {
          state.channels[id].conn->BlockingWrite(data.data(), data.size(),
                                                 true);
        }
      } catch (const current::Exception &e) {
        std::cout << "error"
                  << ": " << e.OriginalDescription() << std::endl;
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
                        std::string host = "0.0.0.0", uint16_t port = 8080,
                        int n_threads = 0, int timeout_ms = 1000) {
    safe_state_.MutableUse(
        [config](SharedState &state) { state.mapping = config; });
    host_ = host;
    port_ = port;
    n_threads_ = n_threads;
    timeout_ms_ = timeout_ms;
  };
  void start() {
    auto server = WebsocketServer(
        [this](WebsocketClient &client, std::vector<uint8_t> data, int type) {
          on_data(client, data, type);
        },
        [this](WebsocketClient &client) { on_connect(client); },
        [this](WebsocketClient &client) { on_disconnect(client); }, port_,
        host_, n_threads_, timeout_ms_);

    std::cout << "Started stream router on " << host_ << ":" << port_
              << std::endl;
    server.start();
  }
  void stop() {
    safe_state_.MutableUse([](SharedState &state) { state.die = true; });
  }
  uint32_t streams_count() {
    auto result = safe_state_.MutableUse(
        [](SharedState &state) { return state.channels.size(); });
    return result;
  }
  StreamRouter(const StreamRouter &) = delete;
  ~StreamRouter() = default;
};
