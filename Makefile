.PHONY: fmt run all memory_run docker docker_run
CLANG_FORMAT=$(shell echo "$${CLANG_FORMAT:-clang-format}")
all:
	pls version >/dev/null || pip install plsbuild
	pls b

fmt:
	${CLANG_FORMAT} -i src/*.cc src/*.h

run:
	./.debug/vocallout --config=./demo/config.json

memory_run:
	valgrind --leak-check=yes ./.debug/vocallout --config=./demo/config.json

docker:
	docker build -t vocallout_1_1_14 .

docker_run: docker
	docker run --rm --net=host vocallout
