.PHONY: fmt run all
CLANG_FORMAT=$(shell echo "$${CLANG_FORMAT:-clang-format}")
all:
	#pip install plsbuild
	pls b

fmt:
	${CLANG_FORMAT} -i src/*.cc src/*.h


run:
	./.debug/vocallout --config=./demo/config.json

memory_run:
	valgrind --leak-check=yes ./.debug/vocallout --config=./demo/config.json

docker:
	docker build -t vocallout .

docker_run: docker
	docker run --rm --net=host vocallout
