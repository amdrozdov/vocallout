FROM --platform=linux/amd64 alpine:3.18

RUN apk update && apk add --no-cache build-base gcc git cmake python3 py3-pip bash
RUN pip3 install plsbuild

COPY src /src
COPY CMakeLists.txt /
RUN pls build
RUN cp ./.debug/vocallout /vocallout

EXPOSE 8080
EXPOSE 8081

COPY demo/config.json /config.json
ENTRYPOINT ["/vocallout", "--config=/config.json"]
