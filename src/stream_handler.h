#pragma once

#include "server.h"

// TODO: move sampling rate to channel configuration
const int chunk_size = 16000 * sizeof(float);
const int max_waits = 10;

inline struct ControlSignal get_state(
    current::WaitableAtomicImpl<SharedState, false>::BasicImpl *safe_state,
    std::string channel_id) {
  auto control = safe_state->WaitFor(
      [channel_id](SharedState const &state) {
        return state.channel_exists(channel_id);
      },
      [channel_id](SharedState &state) {
        auto iter = state.to_kill.find(channel_id);
        if (iter != state.to_kill.end()) {
          state.channel_control.erase(channel_id);
          state.to_kill.erase(iter);
          return ControlSignal{true};
        }
        return ControlSignal{false};
      },
      std::chrono::milliseconds(50));
  return control;
}

inline void stream_handler(
    current::WaitableAtomicImpl<SharedState, false>::BasicImpl *safe_state,
    VOChannelCreate new_channel) {

  safe_state->MutableUse([&new_channel](SharedState &state) {
    std::cout << "Channel '" << new_channel.id << "' is online on port "
              << int(new_channel.in_port) << std::endl;
  });

  std::string channel_id = new_channel.id;
  int waits = 0;
  std::vector<uint8_t> buffer(chunk_size);
  while (true) {
    try {
      auto control = get_state(safe_state, channel_id);
      if (control.stop) {
        break;
      }
      current::net::Socket socket(
          (current::net::BarePort(new_channel.in_port)));
      current::net::Connection connection(socket.Accept());
      current::net::Connection out_conn(current::net::ClientSocket(
          new_channel.out_host, new_channel.out_port));
      safe_state->MutableUse([&new_channel](SharedState &state) {
        std::cout << "Channel '" << new_channel.id << "' is connected"
                  << std::endl;
      });

      // TODO: add connection timeout and do continue here for next iteration

      while (waits < max_waits) {
        size_t size = 0;
        // Get data from PBX
        try {
          size = connection.BlockingRead(&buffer[0], buffer.size());
        } catch (const current::Exception &e) {
        }
        // Send data to ASR
        if (size) {
          out_conn.BlockingWrite(buffer.data(), buffer.size(), true);
        } else {
          waits += 1;
          std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        control = get_state(safe_state, channel_id);
        if (control.stop) {
          break;
        }
      }
      safe_state->MutableUse([&new_channel](SharedState &state) {
        std::cout << "Channel '" << new_channel.id << "' is disconnected"
                  << std::endl;
      });
      if (control.stop) {
        break;
      }
      waits = 0;
    } catch (const current::Exception &e) {
      safe_state->MutableUse([&e](SharedState &state) {
        std::cout << "error"
                  << ": " << e.OriginalDescription() << std::endl;
      });
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  safe_state->MutableUse([&channel_id](SharedState &state) {
    std::cout << "Channel '" << channel_id << "' has been stopped" << std::endl;
  });
}
