# The Endpoints Server Proxy #

Endpoints Server Proxy, a.k.a. ESP is a proxy which enables API management
capabilities for JSON/REST or gRPC API services. The current implementation is
based on an [NGINX](http://nginx.org) HTTP reverse proxy server.

ESP provides:

* **Features**: authentication (auth0, gitkit), API key validation, JSON to gRPC
  transcoding, as well as  API-level monitoring, tracing and logging. More
  features coming in the near future: quota, billing, ACL, etc.

* **Easy Adoption**: the API service can be implemented in any coding language
  using any IDLs.

* **Platform flexibility**: support the deployment on any cloud or on-premise
  environment.

* **Superb performance and scalability**: low latency and high throughput

## ESP can Run Anywhere ##

However, the initial development was done on Google App Engine Flexible
Environment, GCE and GKE for API services using [Open API
Specification](https://openapis.org/specification) and so our instructions
and samples are focusing on these platforms. If you make it work on other
infrastructure and IDLs please let us know and contribute instructions/code.

## Prerequisites ##

Common prerequisites used irrespective of operating system and build tool
chain are:

* [Git](http://www.git-scm.com/)
* [Node.js](http://node.js.org) is required for running included example
  Endpoints [bookstore](/test/bookstore/) application.

## Getting ESP ##

To download the Endpoints Server Proxy source code, clone the ESP repository:

    # Clone ESP repository
    git clone https://github.com/cloudendpoints/esp.git

    # Initialize Git submodules.
    git -C esp submodule update --init --recursive

## Repository Structure ##

* [doc](/doc): Documentation
* [docker](/docker): Scripts for packaging ESP in a Docker image.
* [include](/include): Endpoints Server Proxy header files.
* [src](/src): Endpoints Server Proxy source.
* [google](/google) and [third_party](/third_party): Git submodules containing
  dependencies of ESP, including NGINX.
* [script](/script): Scripts used for build, test, and continuous integration.
* [test](/test): Applications and client code used for end-to-end testing.
* [tools](/tools): Assorted tooling.


## ESP Tutorial ##

To find out more about building, running, and testing ESP, please review

* [ESP Tutorial](/doc/tutorial.md)
* [Testing ESP with Bazel](/doc/testing.md)


## Contributing ##

Your contributions are welcome. Please follow the [contributor
guidlines](/CONTRIBUTING.md).

