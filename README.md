# vocallout
High throughput real-time ASR streams router [WIP]

## build
The build will automatically get cmake files from Current and build the server
```
make
```

## How to run the demo
1. Prepare and the asr in `demo` directory
2. Build the project
3. Run streaming server


## How to use
Assuming that ASR and server are up and running:
1. You may configure channels with following commands
```python
import requests
# Create new channel
requests.post("http://127.0.0.1:8080/channel", json={
    "id": "ch1",
    "in_port": 9001,
    "out_port": 43007,
    "out_host": "127.0.0.1"
})
# Delete the channel
requests.delete("http://127.0.0.1:8080/channel", json={"id": "ch1"})
```
2. To transmit the audio run `./.current/streamer --filename="samples/jfk.wav"`
3. You will be able to see the output on running asr

Expected output on streamer:
```
% ./.current/streamer --filename="samples/jfk.wav"
Channels: mono
Length = 11 sec
Transmitted file 'samples/jfk.wav' to the server
```
Expected output on ASR server:
```
Loading Whisper /workspace/faster-whisper-large-v3 model for en... done. It took 2.04 seconds.
Listening on localhost:43007
Connected
 And[ so,  my  fellow  Americans,]
 And  so,  my  fellow  Americans,[]
 And  so,  my  fellow  Americans,  ask  not[ what  your]
 And  so,  my  fellow  Americans,  ask  not  what  your[ country  can  do  for  you,]
 And  so,  my  fellow  Americans,  ask  not  what  your  country  can  do  for  you,[]
 And  so,  my  fellow  Americans,  ask  not  what  your  country  can  do  for  you,  ask  what  you[ can  do  for  yourself.]
 And  so,  my  fellow  Americans,  ask  not  what  your  country  can  do  for  you,  ask  what  you  can  do  for[ your  country.]
 And  so,  my  fellow  Americans,  ask  not  what  your  country  can  do  for  you,  ask  what  you  can  do  for  your  country.[]
Disconnected
```
Note, that regular text is commited asr output, and text in `[]` is uncommited hypothesis buffer

Expected output on server:
```
listening up to 3 streams. Admin server is on port 8080
Channel 'ch1' is online on port 9001
Channel 'ch1' is connected
Channel 'ch1' is disconnected
```
