.PHONY: fmt run all
CLANG_FORMAT=$(shell echo "$${CLANG_FORMAT:-clang-format}")
all:
	#pip install plsbuild
	pls b

fmt:
	${CLANG_FORMAT} -i src/*.cc src/*.h


run:
	./.debug/vocallout --config=./demo/config.json
