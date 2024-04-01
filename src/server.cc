#include "server.h"

DEFINE_uint16(port, 8080, "The local port to use.");
DEFINE_uint32(n, 3, "Max number of audio streams");
DEFINE_string(api_token, "", "HTTP api token to use");

struct SharedState final {
  bool die = false;
  std::unordered_set<std::string> to_kill;
  std::map<std::string, std::thread> threads;
  std::map<std::string, std::string> channel_control;
  bool channel_exists(const std::string &channel_id) const {
    return channel_control.find(channel_id) != channel_control.end();
  }
};

void sessions_collector(current::WaitableAtomic<SharedState>& safe_state){}

int main(int argc, char **argv) {
  ParseDFlags(&argc, &argv);

  try {
    auto &http = HTTP(current::net::AcquireLocalPort(FLAGS_port));

    current::WaitableAtomic<SharedState> safe_state;

    // Need to join and remove finished sessions
    auto joiner = std::thread([&safe_state] {
      while (true) {
        auto j_state = safe_state.WaitFor(
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
    });

    auto scope = http.Register("/channel", [&safe_state](Request r) {
      if (!FLAGS_api_token.empty() &&
          (!r.headers.Has("api_token") ||
           (FLAGS_api_token != r.headers["api_token"].value))) {
        r(VOResponse::Error("invalid token"), HTTPResponseCode.Forbidden);
        return;
      }

      if (r.method == "DELETE") {
        VOChannelDelete to_delete;
        try {
          to_delete = ParseJSON<VOChannelDelete>(r.body);
        } catch (JSONSchemaException &e) {
          r(e.OriginalDescription(), HTTPResponseCode.BadRequest);
          return;
        }

        std::string channel_id = to_delete.id;
        auto has_channel =
            safe_state.ImmutableUse([channel_id](const SharedState &state) {
              return state.channel_exists(channel_id);
            });
        if (!has_channel) {
          r(VOResponse::Error("error: unknown channel"));
          return;
        }
        // Send kill signal to the channel
        safe_state.MutableUse([channel_id](SharedState &state) {
          state.to_kill.insert(channel_id);
        });
        r(VOResponse::Error("error: channel killed"));
        return;
      }
      if (r.method != "POST") {
        r(VOResponse::Error("error: not supported"),
          HTTPResponseCode.BadRequest);
        return;
      }

      VOChannelCreate new_channel;
      try {
        new_channel = ParseJSON<VOChannelCreate>(r.body);
      } catch (JSONSchemaException &e) {
        r(e.OriginalDescription(), HTTPResponseCode.BadRequest);
        return;
      }

      auto node_state = safe_state.MutableUse([new_channel, &safe_state](
                                                  SharedState &state) {
        if (state.channel_exists(new_channel.id)) {
          return VOResponse::Error("error: channel already exists");
        }
        if (state.channel_control.size() == FLAGS_n) {
          return VOResponse::Error("error: too many channels");
        }
        // Add channel
        state.channel_control[new_channel.id] = "";

        state.threads[new_channel.id] = std::thread([&safe_state,
                                                     new_channel]() {
          std::cout << "Channel '" << new_channel.id << "' is online on port "
                    << int(new_channel.in_port) << std::endl;
          std::string channel_id = new_channel.id;
          while (true) {
            auto control = safe_state.WaitFor(
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
              std::cout << "Channel '" << channel_id << "' has been stopped"
                        << std::endl;
              break;
            }

            // TODO: route the stream here
            // streaming_sockets example could perfectly fit there
            std::cout << "[" << new_channel.id << "] worker tick" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
          }
        });
        return VOResponse::OK("created");
      });
      r(node_state, (node_state.is_error ? HTTPResponseCode.BadRequest
                                         : HTTPResponseCode.OK));
    });

    scope += http.Register("/stop", [&safe_state](Request r) {
      safe_state.MutableUse([](SharedState &state) { state.die = true; });
      r(VOResponse::OK("server stop"));
    });

    std::cout << "listening up to " << FLAGS_n
              << " streams. Admin server is on port " << FLAGS_port
              << std::endl;
    joiner.join();
    std::cout << "Safe shutdown" << std::endl;
  } catch (current::net::SocketBindException const &) {
    std::cout << "the local port " << FLAGS_port << " is already taken"
              << std::endl;
  }
}
