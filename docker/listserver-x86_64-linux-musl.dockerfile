# ListServer Build Environment
FROM alpine:3.16 AS build-env
ARG VER_EXTRA
ARG CACHE_DATE=2021-07-25
COPY --chown=1001:1001 ./ /tmp/listserver

RUN apk add --update --virtual .listserver-build-dependencies \
		cmake \
		gcc \
		g++ \
		bash \
		make \
		git \
		automake \
		autoconf \
		curl-dev \
		openssl-dev \
		bzip2-dev \
		ninja \
	&& cd /tmp/listserver \
	&& cmake -Wno-dev -GNinja -S/tmp/listserver -B/tmp/listserver/build -DCMAKE_BUILD_TYPE=Release -DVER_EXTRA=${VER_EXTRA} -DWOLFSSL=OFF -DCMAKE_CXX_FLAGS_RELEASE="-O3 -ffast-math" \
	&& cmake --build /tmp/listserver/build --config Release --target clean \
	&& cmake --build /tmp/listserver/build --config Release --target package --parallel $(getconf _NPROCESSORS_ONLN) \
	&& chmod 777 -R /tmp/listserver/dist \
	&& rm -rf /tmp/listserver/dist/_CPack_Packages \
    && chown 1001:1001 -R /tmp/listserver \
    && apk del --purge .listserver-build-dependencies

USER 1001
# ListServer Run Environment
FROM alpine:3.16
ARG CACHE_DATE=2021-07-25
COPY --from=build-env /tmp/listserver/bin /listserver
RUN apk add --update libstdc++ libatomic libbz2
WORKDIR /listserver
#VOLUME [ "/listserver/settings.ini", "/listserver/ipbans.txt" ]
ENTRYPOINT ["./listserver"]
EXPOSE 14900 14922 14923 6667
CMD ["-d"]
