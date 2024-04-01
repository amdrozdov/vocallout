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
