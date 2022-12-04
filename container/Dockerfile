FROM alpine:3.13
ARG USER=1000

RUN apk add g++ clang byacc make git zlib-dev openssl-dev
RUN adduser -u $USER -H -D dgd

WORKDIR /container

ADD server update /container/

RUN git config --global pull.rebase false
RUN git clone -b release https://github.com/dworkin/dgd.git
RUN git clone -b release https://github.com/dworkin/lpc-ext.git
RUN mkdir cache
RUN chown dgd:dgd cache
RUN ln -s lpc-ext/src/jit/jitcomp

ENTRYPOINT [ "/container/server", "/container/config" ]
