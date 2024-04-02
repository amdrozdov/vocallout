build:
	docker build -t rta .

run:
	docker run --rm --runtime=nvidia --gpus all --net host -v "${CU_MODEL_PATH}":/workspace rta

