#include "../current/blocks/xterm/progress.h"
#include "../current/blocks/xterm/vt100.h"
#include "../current/bricks/dflags/dflags.h"
#include "../current/bricks/net/tcp/tcp.h"
#include "../current/bricks/strings/printf.h"
#include "../current/bricks/time/chrono.h"
#include "third_party/wav.h"

DEFINE_string(host, "127.0.0.1", "The destination address to send data to.");
DEFINE_uint16(port, 9001, "The port to use.");
DEFINE_uint16(sampling_rate, COMMON_SAMPLE_RATE, "Sampling rate");
DEFINE_string(filename, "samples/jfk.wav", "Input audio file");

using namespace current::net;
using namespace current::vt100;

int main(int argc, char **argv) {
  ParseDFlags(&argc, &argv);

  std::vector<float> mono_in;
  std::vector<std::vector<float>> stereo_in;

  bool is_multichan = true;

  // Load audio file into memory in order to emulate PBX audio stream
  if (!read_wav(FLAGS_filename, mono_in, stereo_in, true)) {
    is_multichan = false;
    if (!read_wav(FLAGS_filename, mono_in, stereo_in, false)) {
      std::cout << "Can't read audio from " << FLAGS_filename << std::endl;
      return 1;
    }
  }

  if (is_multichan) {
    std::cout << "Channels: " << stereo_in.size() << std::endl;
    if (stereo_in.size()) {
      std::cout << "Length = "
                << float(stereo_in[0].size() / FLAGS_sampling_rate) << " sec"
                << std::endl;
    }
  } else {
    std::cout << "Channels: mono" << std::endl;
    std::cout << "Length = " << float(mono_in.size() / FLAGS_sampling_rate)
              << " sec" << std::endl;
  }

  // TODO support multi channel audio

  current::ProgressLine progress;
  progress << "Starting the audio streamer";
  size_t total_sent = 0;
  size_t pos = 0;
  bool is_done = false;

  while (true) {
    try {
      Connection connection(ClientSocket(FLAGS_host, FLAGS_port));
      while (true) {
        // Get next chunk of audio
        std::vector<float> chunk = std::vector<float>(
            mono_in.begin() + pos,
            mono_in.begin() +
                std::min(pos + FLAGS_sampling_rate, mono_in.size()));
        int len = sizeof(float) * chunk.size();
        pos += FLAGS_sampling_rate;
        // Stop if all recording was transmitted
        if (!chunk.size()) {
          is_done = true;
          break;
        }
        // Send current chunk
        connection.BlockingWrite(chunk.data(), len, true);
        total_sent += len;
        progress << reset << "Sent " << total_sent;

        // Emulate 1 second or real-time speech (pause for 1 second between
        // sending next buffer)
        std::this_thread::sleep_for(std::chrono::seconds(1));
      }
    } catch (const current::Exception &e) {
      progress << red << bold << "error"
               << ": " << e.DetailedDescription() << reset;
    }
    if (is_done) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  progress << reset;
  std::cout << "Transmitted file '" << FLAGS_filename << "' to the server"
            << std::endl;
}
