#include "server.h"
#include "sessions_collector.h"
#include "stream_handler.h"

DEFINE_uint16(port, 8080, "The local port to use.");
DEFINE_uint32(n, 3, "Max number of audio streams");
DEFINE_string(api_token, "", "HTTP api token to use");

int main(int argc, char **argv) {
  ParseDFlags(&argc, &argv);

  try {
    // Create http server
    auto &http = HTTP(current::net::AcquireLocalPort(FLAGS_port));
    // Create state object for sync
    current::WaitableAtomic<SharedState> safe_state;
    // Start sessions collector
    auto joiner = std::thread(sessions_collector, &safe_state);

    auto scope = http.Register("/channel", [&safe_state](Request r) {
      // validate token
      if (!FLAGS_api_token.empty() &&
          (!r.headers.Has("api_token") ||
           (FLAGS_api_token != r.headers["api_token"].value))) {
        r(VOResponse::Error("invalid token"), HTTPResponseCode.Forbidden);
        return;
      }

      // handle channel deletion
      if (r.method == "DELETE") {
        VOChannelDelete to_delete;
        try {
          to_delete = ParseJSON<VOChannelDelete>(r.body);
        } catch (JSONSchemaException &e) {
          r(e.OriginalDescription(), HTTPResponseCode.BadRequest);
          return;
        }
        auto result = safe_state.MutableUse([&to_delete](SharedState &state) {
          if (!state.channel_exists(to_delete.id)) {
            return VOResponse::Error("error: unknown channel");
          }
          state.to_kill.insert(to_delete.id);
          return VOResponse::OK("channel killed");
        });
        return r(result, result.is_error ? HTTPResponseCode.BadRequest
                                         : HTTPResponseCode.OK);
      }
      if (r.method != "POST") {
        r(VOResponse::Error("error: not supported"),
          HTTPResponseCode.BadRequest);
        return;
      }

      // Handle channel creation
      VOChannelCreate new_channel;
      try {
        new_channel = ParseJSON<VOChannelCreate>(r.body);
      } catch (JSONSchemaException &e) {
        r(e.OriginalDescription(), HTTPResponseCode.BadRequest);
        return;
      }

      auto node_state =
          safe_state.MutableUse([new_channel, &safe_state](SharedState &state) {
            if (state.channel_exists(new_channel.id)) {
              return VOResponse::Error("error: channel already exists");
            }
            if (state.channel_control.size() == FLAGS_n) {
              return VOResponse::Error("error: too many channels");
            }
            // Add channel and create worker thread
            state.channel_control[new_channel.id] = "";
            state.threads[new_channel.id] =
                std::thread(stream_handler, &safe_state, new_channel);

            return VOResponse::OK("created");
          });
      r(node_state, (node_state.is_error ? HTTPResponseCode.BadRequest
                                         : HTTPResponseCode.OK));
    });

    // Handle graceful stop
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
