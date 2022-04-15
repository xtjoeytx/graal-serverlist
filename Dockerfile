# ListServer Build Environment
FROM alpine:3.12 AS build-env
COPY ./ /listserver
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
	&& rm -rf /listserver/build \
	&& mkdir /listserver/build \
	&& cd /listserver/build \
	&& cmake .. -Wno-dev -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_RELEASE="-O3 -ffast-math" \
	&& make clean \
	&& cmake --build . --config Release -- -j $(getconf _NPROCESSORS_ONLN) \
	&& apk del --purge .listserver-build-dependencies

# ListServer Run Environment
FROM alpine:3.12
ARG CACHE_DATE=2016-01-01
COPY --from=build-env /listserver/bin /listserver
RUN apk add --update libstdc++ libatomic
WORKDIR /listserver
VOLUME [ "/listserver/settings.ini", "/listserver/ipbans.txt" ]
ENTRYPOINT ["./listserver"]
EXPOSE 14900 14922 14923 6667
CMD []
