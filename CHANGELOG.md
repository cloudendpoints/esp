# Release 1.0.0 11-11-2016

- Fix for rename version => config_id
- Usability improvements to the fetching script
- Add support for HTTPS upstream to the start script
- Adds support for binding request fields using query parameters.
- Add health endpoint to application port
- Change the prefix for /credential_id label to lowercase.
- Fixed wrong protocol value in Report for grpc pass-though
- Send service_config_id to service control server.
- Upgrade nginx to 1.11.5
- Upgrade GPRC and protobuf
- Enable padding when base64 "X-Endpoints-API-UserInfo" HTTP header

# Release 0.3.10 19-10-2016

- Fail open for method with `allow_unregistered_calls`.
- Ingress controller
- Update googleapis and service-control-client-cxx submodules.
- Cleanups, bug fixes and infrastructure improvements
  - Increase disk size for GCE raw test.
  - Use correct build paramater for rapture repo.
  - Fix gce test for latest release.
  - Adding fetch and retries for bazel builds.
  - Tempory fix for performance test.

# Release 0.3.9 06-10-2016

- Fix debian package dependencies.
- Changed memory detection for stress tests.
- Update kubernetes client to 1.5.
  - Adds missing informer framework from kubernetes.
  - Adds support for GCP auth provider (used by gcloud).
- Fix transcoding error test failures under TSAN.
- Fix a shutdown crash found by TSAN tests.
- Enable memory leak detection in grpc stress test.
- Disable liveness probe for GKE tests.

# Release 0.3.8 01-10-2016

- Fix handling large responses in gRPC handler.
- Fix memory leak caused by aborted client calls and report more detailed memory usage.
- Test presubmits in Jenkins and test improvement.
- Use instance internal ip instead of hostname.
- Add a test to make sure the "nbf" claim is checked.
- Report start_time correctly.
- Use Google APIs to deploy service configs.
- Use github Dockerfile to build Flex docker image.
- Add grpc interop stress test to Jenkins test.
- Read the "azp" (authorized party) claim from an auth token.
- Improve error handling logic.
- Enabling hazelcast in Jenkins.
- Stop collecting access logs in e2e tests.
- Stop status print outs in custom nginx.conf.
- Use espcli for GKE e2e tests.
- Calculate memory usage for long-run test.
- Add Google APIs Go bindings.
- Added a t test for invalid api_key case.
- Accept Issuer with or without https prefix for OpenID discovery.
- Removed testing for go binary for new docker slave.
- Update version number to 0.3.8.
- Switch to using start_esp for GCE raw VM.

# Release 0.3.7 13-09-2016

- Do not send consumer metrics if api_key is not provided.
- Remove /producer/by_consumer metrics.
- Set api_version correctly in Report calls.
- Update to use googleapis proto from Github.
- Update googleapis submodule.
- Update platform from GAE to GAE Flex.
- Log fetching steps in the start script, disable status print outs.
- Initial CLI for GKE deployment management using k8s Go client.
- Cleanups, bug fixes and infrastructure improvements
  - Use perl bazel rules from github
  - Remove grpc in script/release-publish and script/release-stable.
  - Fix flaky tests by disabling service control cache.
  - Add configuration option for the subrequest certificate files

# Release 0.3.6 07-09-2016

- Fix a memory leak in grpc transcode
- Add grpc large transcoding to stress test.
- Add endpoints_resolver in nginx config for HTTP subrequests DNS
- Upgrade to use GRPC to 1.0.0 GA and Protobuf to 3.0.0 GA
- Propagate grpc metadata from downstream to upstream
- Expose http and grpc ports when deploying grpc.
- Add gprc interop tests
- Switch to start_esp.py in docker generic image
- Extract api key from header x-api-key by default.

# Release 0.3.5 24-08-2016

- Add bindings and body prefix test for transcoding
- Allow multiple HTTP rules for the same RPC method
- Add cloud trace request sampling.
- Add doc for ESP on k8s.
- Added a test to verify service control data.
- Cleanups, bug fixes & Infrastructure improvements
  - Set corret LANG environment variable for Jenkins
  - Fix ASAN failure in jenkins presubmit
  - Fix error handling of release-stable script
  - Fix Travis TSAN build
  - Implement books support in grpc bookstore backend
  - Add invalid JSON cases to transcoding errors test
  - Check HTTP status code in transcoding tests
  - Fix ASAN heap-use-after-free warning
  - Fixed metadata_timeout t asan failure.
  - Integrate GKE go script into Jenkins pipeline
  - Running e2e test on released artifacts and gcloud release candidate


# Release 0.3.4 17-08-2016

- Add Kubernetes support & deployment improvements.
- Add configuration settings for local development using ESP.
- Aggregate traces and batch to CloudTrace API.
- Consolidate docs into a tutorial.
- Do not report producer project in errors.
- Improve GRPC/transcoding error handling.
- Print ESP version when running nginx-esp -V.
- Start a new Go based Test Infrastructure.
- Cleanups, bug fixes & Infrastructure improvements:
  - Add more test coverage.
  - Add presubmits to Jenkins.
  - Apply buildifer to BUILD files.
  - Parallelize t-tests.
  - Remove libgrpc based downstream implementation.
  - Stabilize stress tests.
  - Update Bazel to 0.3.1.
  - Update NGINX to 1.11.3.
  - Use NGINX trailers when finalizing gRPC response.
  - etc


# Release 0.3.3 26-07-2016

- Add HTTP2 load test. Refactored common functionality in the load test
  client to support both wrk and h2load.
- Check logs on passing release quals only.
- Clean-up test transcoding proto.
- Add test to share port with HTTPS and GRPC.
- Increase keepalive and port range for GKE tests.
- Save GKE & GCE container logs.


# Release 0.3.2 22-07-2016

- Improve grpc_pass stability; fixed a crash with large payload.
- Support grpc long run stress test.
- Pass cloud trace span id to backend.
- Pass x-endpoints-user-info to backend automatically, Not require
  a variable in nginx config.
- Update NGINX to 1.11.2 with self-managed workspace dependencies.
- Upgrade protobuf to 3.0.0-beta-4
- Use WRK for benchmarking instead of AB

# Release 0.3.1 13-07-2016

## Features in this release

- Add debug nginx binary into deb image.
- Add HTTP request retry logic
- HTTP/JSON <=> gRPC Transcoding
- Print endpoints status in nginx error log.
- Setup Travis CI

# Release 0.3.0 07-07-2016 (First Beta Release of ESP)

Extensible Service Proxy, a.k.a. ESP is a proxy which enables API management
capabilities for API services. It supports HTTP/JSON and gRPC APIs.

## Features in this release

- Authentication (auth0, gitkit)
- API key validation
- API-level monitoring and logging
- Integration with Cloud Tracing
