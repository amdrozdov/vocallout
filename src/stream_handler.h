#pragma once

#include "server.h"

inline void stream_handler(
    current::WaitableAtomicImpl<SharedState, false>::BasicImpl *safe_state,
    VOChannelCreate new_channel) {

  safe_state->MutableUse([&new_channel](SharedState &state) {
    std::cout << "Channel '" << new_channel.id << "' is online on port "
              << int(new_channel.in_port) << std::endl;
  });

  std::string channel_id = new_channel.id;
  while (true) {
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
        // TODO: reduce this time (only for debug)
        std::chrono::seconds(1));
    if (control.stop) {
      safe_state->MutableUse([&channel_id](SharedState &state) {
        std::cout << "Channel '" << channel_id << "' has been stopped"
                  << std::endl;
      });
      break;
    }

    // TODO: route the stream here
    // streaming_sockets example could perfectly fit there
    std::cout << "[" << new_channel.id << "] worker tick" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    // this is a mock for stream processing
  }
}
