#ifndef VOCALLOUT_SERVER_H
#define VOCALLOUT_SERVER_H

#include "blocks/http/api.h"
#include "blocks/json/json.h"
#include "bricks/dflags/dflags.h"
#include "bricks/sync/waitable_atomic.h"

DEFINE_uint16(port, 8080, "The local port to use.");
DEFINE_uint32(n, 3, "Max number of audio streams");
DEFINE_string(api_token, "", "HTTP api token to use");

struct ControlSignal final {
  // TODO:
  // think about/implement input/output channel switch command (for call
  // redirects)
  bool stop = false;
};

CURRENT_STRUCT(VOChannelCreate) {
  CURRENT_FIELD(id, std::string);
  CURRENT_FIELD(in_port, uint8_t);
  CURRENT_FIELD(out_host, std::string);
  CURRENT_FIELD(out_port, uint8_t);
};
CURRENT_STRUCT(VOChannelDelete) { CURRENT_FIELD(id, std::string); };
CURRENT_STRUCT(VOResponse) {
  CURRENT_FIELD(msg, std::string);
  CURRENT_FIELD(is_error, bool);
};

VOResponse vo_ok(std::string msg) {
  VOResponse resp;
  resp.msg = msg;
  resp.is_error = false;
  return resp;
}
VOResponse vo_error(std::string msg) {
  VOResponse resp;
  resp.msg = msg;
  resp.is_error = true;
  return resp;
}

#endif // VOCALLOUT_SERVER_H
