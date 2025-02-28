#include "pls.h"

PLS_INCLUDE_HEADER_ONLY_CURRENT();
PLS_ADD_DEP("websockets", "https://github.com/current-deps/websockets");
PLS_ADD_DEP("redis", "https://github.com/current-deps/redis");

#include "router.h"
#include "vocallout.h"
const std::string VERSION = "Vocallout v.1.1.0 Beta";

DEFINE_uint16(http_port, 8081, "Http server port");
DEFINE_string(api_token, "", "HTTP api token to use");
DEFINE_uint16(port, 8080, "The local port to use.");
DEFINE_string(host, "0.0.0.0", "Host to bind the server");
DEFINE_string(config, "config.json", "path to the configuration file");
DEFINE_int32(n_threads, 32, "Max number of worker threads");
DEFINE_int32(timeout_ms, 1000, "Connection timeout in milliseconds");

std::string read_config(std::string path) {
  std::ifstream reader(path);
  if (!reader.good()) {
    std::cout << "Error: Config file does not exists." << std::endl;
    exit(1);
  }
  std::stringstream buffer;
  buffer << reader.rdbuf();
  return buffer.str();
}

std::map<std::string, std::vector<VONode>>
load_and_parse_config(std::string path) {
  auto conf = read_config(path);
  auto result = parse_config(conf);
  if (!result.first) {
    exit(1);
  }
  return result.second;
}

int main(int argc, char **argv) {
  ParseDFlags(&argc, &argv);

  std::cout << VERSION << std::endl;
  bool stop = false;
  auto mapping = load_and_parse_config(FLAGS_config);
  auto ws_config = WSConfig::FromFields(FLAGS_host, FLAGS_port, FLAGS_n_threads,
                                        FLAGS_timeout_ms);
  auto router = StreamRouter(mapping, ws_config);
  try {
    auto &http = HTTP(current::net::AcquireLocalPort(FLAGS_http_port));
    auto scope = http.Register("/metrics", [&router](Request r) {
      if (!FLAGS_api_token.empty() &&
          (!r.headers.Has("api_token") ||
           (FLAGS_api_token != r.headers["api_token"].value))) {
        r(VOResponse::Error("invalid token"), HTTPResponseCode.Forbidden);
        return;
      }
      if (r.method != "GET") {
        r(VOResponse::Error("error: not supported"),
          HTTPResponseCode.BadRequest);
        return;
      }
      r(VOStatus::Response("OK", router.StreamsCount()));
    });

    scope += http.Register("/stop", [&router, &stop](Request r) {
      if (!FLAGS_api_token.empty() &&
          (!r.headers.Has("api_token") ||
           (FLAGS_api_token != r.headers["api_token"].value))) {
        r(VOResponse::Error("invalid token"), HTTPResponseCode.Forbidden);
        return;
      }
      router.BreakConnections();
      stop = true;
      r(VOResponse::OK("server stop"));
    });
    std::cout << "Started http server on port " << FLAGS_http_port << std::endl;
    while (!stop) {
      sleep(1);
    }
    std::cout << "Safe shutdown" << std::endl;
  } catch (current::net::SocketBindException const &) {
    std::cout << "the local port " << FLAGS_http_port << " is already taken"
              << std::endl;
  }

  return 0;
}
