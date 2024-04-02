from third_party.whisper_online import *
import io
import soundfile
import socket
import sys
import argparse
import numpy as np


class StreamProcessor:
    """
    Current server input processing class
    How it works:
    1. Load the model and do asr init
    2. Create a socket
    3. Connect to Current server and start receiving audio stream
    4. Show the output every time we have changes on c/u buffers
    """

    MAX_SIZE = 64000
    SAMPLING_RATE = 16000
    BYTE_FORMAT = "PCM_32"
    ENDIAN = "LITTLE"

    def __init__(self, online_asr_proc, min_chunk, host, port):
        self.online_asr_proc = online_asr_proc
        self.min_chunk = min_chunk
        self.host = host
        self.port = port
        self.connection = None

    def next_chunk(self):
        """
        Returnms next audio chunk ready to insert to the transcription buffer
        """
        out = []
        while sum(len(x) for x in out) < self.min_chunk * self.SAMPLING_RATE:
            raw_bytes = self.connection.recv(self.MAX_SIZE)
            if not raw_bytes:
                break
            sf = soundfile.SoundFile(
                io.BytesIO(raw_bytes),
                channels=1,
                endian=self.ENDIAN,
                samplerate=self.SAMPLING_RATE,
                subtype=self.BYTE_FORMAT,
                format="RAW",
            )
            audio, _ = librosa.load(sf, sr=self.SAMPLING_RATE, dtype=np.float32)
            out.append(audio)
        if not out:
            return None
        return np.concatenate(out)

    def worker(self):
        """
        Entrypoint - creates the socket and runs the audio processing
        """
        print("Listening on %s:%d" % (self.host, self.port), file=sys.stderr)
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.bind((self.host, self.port))
            s.listen(1)
            while True:
                conn, addr = s.accept()
                self.connection = conn
                print("Connected", file=sys.stderr)
                self.process()
                conn.close()
                print("Disconnected", file=sys.stderr)

    def show(self):
        """
        Demo output for commited and uncommited buffers
        """
        commited = " ".join(o[2] for o in self.online_asr_proc.commited)
        uncommited = " ".join(
            o[2] for o in self.online_asr_proc.transcript_buffer.buffer
        )
        print("%s[%s]" % (commited, uncommited), file=sys.stderr)

    def process(self):
        """
        ASR processing loop
        """
        self.online_asr_proc.init()
        while True:
            chunk = self.next_chunk()
            if chunk is None:
                self.online_asr_proc.commited.extend(
                    self.online_asr_proc.transcript_buffer.buffer
                )
                self.online_asr_proc.transcript_buffer.buffer = []
                self.show()
                break
            self.online_asr_proc.insert_audio_chunk(chunk)
            output = self.online_asr_proc.process_iter()
            if output and output[0] is not None:
                self.show()
        self.online_asr_proc.finish()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", type=str, default="localhost")
    parser.add_argument("--port", type=int, default=43007)
    add_shared_args(parser)
    args = parser.parse_args()

    asr = asr_factory(args)
    online = OnlineASRProcessor(
        asr, None, buffer_trimming=(args.buffer_trimming, args.buffer_trimming_sec)
    )
    s = StreamProcessor(online, args.min_chunk_size, args.host, args.port)
    s.worker()


if __name__ == "__main__":
    main()
