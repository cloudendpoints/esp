
# To build a docker image for grpc-test-server
#
# Here are the steps:
#
# 1) bazel build //test/grpc:all
# 2) cp bazel-bin/test/grpc/grpc-test-server test/grpc
# 3) IMAGE=gcr.io/endpointsv2/grpc-test-server:latest
# 4) docker build --no-cache -t "${IMAGE}" test/grpc
# 5) gcloud docker -- push "${IMAGE}"

FROM debian:stretch-backports

# Install all of the needed dependencies

RUN apt-get update && \
    apt-get install --no-install-recommends -y -q ca-certificates && \
    apt-get -y -q upgrade && \
    apt-get install -y -q --no-install-recommends \
    apt-utils adduser cron curl logrotate python wget \
    libc6 libgcc1 libstdc++6 libuuid1 && \
    apt-get clean && rm /var/lib/apt/lists/*_*

# Copy TEST_SERVER_BIN binary to the same director as this Dockerfile
ADD TEST_SERVER_BIN TEST_SERVER_BIN

EXPOSE 8081

ENTRYPOINT ["./TEST_SERVER_BIN", "TEST_SERVER_ARGS"]
