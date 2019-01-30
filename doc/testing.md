# Testing ESP with Bazel #

ESP is built using [Bazel](http://bazel.io) build tool. Install
[Bazel](http://bazel.io) version 0.5.4, following the [Bazel
documentation](http://bazel.io/docs/install.html).

# Building ESP #

Clone the ESP [GitHub repository](https://github.com/cloudendpoints/esp),
initialize Git submodules, and build ESP using Bazel:

    # Clone the ESP repository
    git clone https://github.com/cloudendpoints/esp

    cd esp

    # Initialize Git submodules
    git submodule update --init --recursive

    # Build ESP binary
    bazel build //src/nginx/main:nginx-esp

The ESP binary location is:

    ./bazel-bin/src/nginx/main/nginx-esp

# Running unit and integration tests #

libio-socket-ssl-perl is needed to run ESP tests:

    sudo apt-get install libio-socket-ssl-perl

Run ESP unit and integration tests:

    bazel test //src/... //third_party:all

# Running tests to check that the ESP configuration files are as expected #

Before starting ESP, start_esp executable generates the ESP configuration files based on its input arguments. To test that the
generated ESP configuration files are as expected:

    bazel test //start_esp/test:start_esp_test

# Running ASAN and TSAN tests #

ASAN works on both Linux and Mac, but TSAN only works on Linux.

    # Run ASAN
    bazel test --config=asan --test_tag_filters=-no_asan \
      //src/... //third_party:all

    # Run TSAN
    bazel test --config=tsan --test_tag_filters=-no_tsan \
      //src/... //third_party:all

If you know a test is not going to work under TSAN or ASAN, please add the
`no_tsan` or `no_asan` flags to your test targets as well as a reference
to the existing bug.
