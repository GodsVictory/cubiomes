FROM alpine:3
COPY . /usr/src/app
WORKDIR /usr/src/app
RUN apk add build-base
RUN gcc Gods_seedfinder.c layers.c generator.c finders.c -o Gods_seedfinder
CMD ./Gods_seedfinder ${GSF_START} ${GSF_END} ${GSF_THREADS} ${GSF_FILTER_RANGE} ${GSF_FULL_RANGE}
