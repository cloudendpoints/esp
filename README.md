# The Extensible Service Proxy #

Extensible Service Proxy, a.k.a. ESP is a proxy which enables API management
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

To download the Extensible Service Proxy source code, clone the ESP repository:

    # Clone ESP repository
    git clone https://github.com/cloudendpoints/esp

    # Initialize Git submodules.
    git -C esp submodule update --init --recursive

## Released ESP docker images ##

ESP docker images are released regularly. The regular images are named as gcr.io/endpoints-release/endpoints-runtime:MAJOR_VERSION.MINOR_VERSION.PATCH_NUMBER. For example, gcr.io/endpoints-release/endpoints-runtime:1.30.0 has MAJOR_VERSION=1, MINOR_VERSION=30 and PATCH_NUMBER=0.

Symbolically linked images:
* **MAJOR_VERSION** is linked to the latest image with same **MAJOR_VERSION**.

For example, gcr.io/endpoints-release/endpoints-runtime:1 is always pointed to the latest image with "1" major version.

### Secure image: ###
Normally ESP container runs as root, it is deemed as not secure. To make ESP container secure, it should be run as non-root and its root file system should be read-only. Normal docker images can be made to run as non-root, but such change may break some existing users. Starting 1.31.0, a new **secure image** is built with suffix "-secure" in the image name, e.g. gcr.io/endpoints-release/endpoints-runtime-secure:1.31.0.  It will be run as non-root.

You can switch to use the **secure images** if the followings are satisfied:
* Nginx is not listening on ports requiring root privilege (ports < 1024).
* If a custom nginx config is used and it has the *server_config* path set to "/etc/nginx", the secure image will not work. The *server_config* is moved to the "/home/nginx" folder in the secure image. Please replace "/etc/nginx" with "/home/nginx" for *sever_config" in your custom nginx config before using the secure image.

If some folders can be mounted externally, the root system can be made read-only. Please see this GKE deployment [yaml](/test/bookstore/gke/deploy_secure_template.yaml) file as example on how to make root system read-only.


## Repository Structure ##

* [doc](/doc): Documentation
* [docker](/docker): Scripts for packaging ESP in a Docker image.
* [include](/include): Extensible Service Proxy header files.
* [src](/src): Extensible Service Proxy source.
* [google](/google) and [third_party](/third_party): Git submodules containing
  dependencies of ESP, including NGINX.
* [script](/script): Scripts used for build, test, and continuous integration.
* [test](/test): Applications and client code used for end-to-end testing.
* [tools](/tools): Assorted tooling.
* [start_esp](/start_esp): A Python start-up script for the ESP proxy. The script includes a generic nginx configuration template and fetching logic to retrieve service configuration from Google Service Management service.



## ESP Tutorial ##

To find out more about building, running, and testing ESP, please review

* [Build ESP on Ubuntu 16.04](/doc/build-esp-on-ubuntu-16-04.md)
* [ESP Tutorial](/doc/tutorial.md)
* [Testing ESP with Bazel](/doc/testing.md)
* [Run ESP on Kubernetes](/doc/k8s/README.md)
* [Debug CI failures](/doc/debug_ci.md)


## Contributing ##

Your contributions are welcome. Please follow the [contributor
guidlines](/CONTRIBUTING.md).

