
# To build a docker image for bookstore
# you need to be running this on Linux.
#
# Here are the steps:
#
# 1) bazel build //test/transcoding:bookstore-server
# 2) cp bazel-bin/test/transcoding/bookstore-server test/transcoding
# 3) IMAGE=gcr.io/endpointsv2/bookstore-grpc:latest
# 4) docker build --no-cache -t "${IMAGE}" test/transcoding
# 5) gcloud docker -- push "${IMAGE}"

FROM debian:jessie

RUN apt-get update && \
    apt-get install --no-install-recommends -y -q ca-certificates && \
    apt-get -y -q upgrade && \
    apt-get install -y -q --no-install-recommends \
    apt-utils adduser cron curl logrotate python wget \
    libc6 libgcc1 libstdc++6 libuuid1 && \
    apt-get clean && rm /var/lib/apt/lists/*_*

ADD bookstore-server bookstore-server

EXPOSE 8081

ENTRYPOINT ["./bookstore-server", "0.0.0.0:8081"]
