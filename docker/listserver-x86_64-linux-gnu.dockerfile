# ListServer Build Environment
FROM amigadev/crosstools:x86_64-linux AS build-env
ARG VER_EXTRA
ARG CACHE_DATE=2021-07-25
COPY --chown=1001:1001 ./ /tmp/listserver

RUN cd /tmp/listserver \
	&& cmake -Wno-dev -GNinja -S/tmp/listserver -B/tmp/listserver/build -DCMAKE_BUILD_TYPE=Release -DSTATIC=ON -DVER_EXTRA=${VER_EXTRA} -DWOLFSSL=OFF -DCMAKE_CXX_FLAGS_RELEASE="-O3 -ffast-math" \
	&& cmake --build /tmp/listserver/build --target clean \
	&& cmake --build /tmp/listserver/build --target package --parallel $(getconf _NPROCESSORS_ONLN) \
	&& chmod 777 -R /tmp/listserver/dist \
    && rm -rf /tmp/listserver/dist/_CPack_Packages

USER 1001
# ListServer Run Environment
FROM alpine:3.16
ARG CACHE_DATE=2021-07-25
COPY --from=build-env /tmp/listserver/dist /dist
COPY --from=build-env /tmp/listserver/build /tmp/listserver/build
RUN apk add --update libstdc++ libatomic cmake
USER 1001
WORKDIR /listserver

