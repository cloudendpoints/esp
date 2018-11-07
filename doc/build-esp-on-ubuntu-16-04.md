# Build ESP on Ubuntu 16.04 #

This tutorial includes detailed instructions for building
ESP and running ESP tests on Ubuntu 16.04.

# Prerequisites #

Install Ubuntu 16.04 LTS OS.

Install the following software packages:

    # Software packages needed for building ESP
    sudo apt-get install -y \
         g++ git openjdk-8-jdk openjdk-8-source \
         pkg-config unzip uuid-dev zip zlib1g-dev

    # Software packages needed for building ESP
    sudo apt-get install libtool m4 autotools-dev automake
         
## Install bazel ##

ESP is built using the [bazel](http://bazel.io) build tool. 
Follow the bazel [install instructions](https://docs.bazel.build/versions/master/install-ubuntu.html#install-using-binary-installer) to install bazel. 
ESP currently requires bazel 0.5.4. For bazel 0.5.4 on Ubuntu, 
the binary installer is bazel-0.5.4-installer-linux-x86_64.sh.

*Note:* Bazel is under active development and from time to time, ESP continuous
integration systems are upgraded to a new version of Bazel. 
The version of Bazel used by ESP continuous integration systems can be found in
the [linux-install-software](/script/tools/linux-install-bazel)
script variable `BAZEL_VERSION=<SHA>`.

# Build ESP #

To build ESP, run the following commands in the terminal .

    # Clone the ESP repository
    git clone https://github.com/cloudendpoints/esp

    cd esp

    # Initialize Git submodules
    git submodule update --init --recursive

    # Build ESP binary
    bazel build //src/nginx/main:nginx-esp

If the build completes successfully, the ESP binary built is at:

    ./bazel-bin/src/nginx/main/nginx-esp

# Runn ESP tests #

libio-socket-ssl-perl is needed to run ESP tests: 

    sudo apt-get install libio-socket-ssl-perl

To run ESP tests, run the following command in the terminal.

    bazel test //src/... //third_party:all
