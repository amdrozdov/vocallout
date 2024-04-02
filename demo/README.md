## Realtime asr demo

1. First you have to download the asr model: https://huggingface.co/Systran/faster-whisper-large-v3/tree/main
2. Export path to your local model `export CU_MODEL_PATH="/path/to/your/model"`
3. Build the container `make -B build`
4. Run ASR server `make run`
