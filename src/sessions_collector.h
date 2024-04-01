#pragma once

#include "server.h"

inline void sessions_collector(
    current::WaitableAtomicImpl<SharedState, false>::BasicImpl *safe_state) {
  while (true) {
    auto j_state = safe_state->WaitFor(
        [](SharedState const &state) {
          bool thread_found = false;
          for (auto &[key, _] : state.threads) {
            if (!state.channel_exists(key)) {
              thread_found = true;
              break;
            }
          }
          return state.die || thread_found;
        },
        [](SharedState &state) {
          // collect threads to join
          // n.b: we need to collect them even if it's time to halt
          std::vector<std::thread> to_join;
          std::vector<std::string> keys_to_drop;
          for (auto &pair : state.threads) {
            if (!state.channel_exists(pair.first)) {
              to_join.push_back(std::move(pair.second));
              keys_to_drop.push_back(pair.first);
            }
          }
          // clean up threads map
          for (auto &to_drop : keys_to_drop) {
            state.threads.erase(to_drop);
          }
          auto j_state = joiner_state{};
          j_state.die = state.die;
          j_state.to_join = std::move(to_join);
          return j_state;
        },
        std::chrono::seconds(1));

    // join worker threads outside
    for (auto &thread : j_state.to_join) {
      thread.join();
    }
    // exit joiner if needed
    if (j_state.die) {
      break;
    }
  }
}
