# Release 1.29.0 07-02-2019

===============================================

- Build ESP Docker image which accepts start_esp arguments from environment variables (#527)
- Add the agent label to all trace spans. (#526)
- Log http request and response headers if configured (#524)
- Add prefix [ESP] in error logging for clarification (#525)
- Add support for grpc tracing header. (#503)
- Add access token to upstream requests for backend routing (#518)
- Support backend routing, based on backed rules (#514)

# Release 1.28.0 08-01-2019

- Not log JWT error when trying multiple public keys (#511)

- Not to extract api-key and use it if not required (#500)

# Release 1.27.0 04-12-2018

- Fix grpc_web crash at sending trailers (#496)
- Clear SSL errors if JWT signature validation fails (#494)
- Not call service management rollouts if not traffic (#492)
- Support downloading large service config (#491)
- Move strict_tls_security header from server level to location level (#487)
- Upgraded nginx to 1.15.0 (#447)

# Release 1.26.0 29-10-2018

- Correctly convert gRPC custom Status to JSON in transcoding. (#479)
- Add start_esp flag: experiental_proxy_backend_host_header. (#476)
- Add an option to enable HSTS (HTTP Strict Transport Security) (#474)
- Add enable_debug flag (#471)
- Not to log access token (#470)
- Upgrade gRPC from 1.4.2 to 1.15.0 (#450)
- CORS always adds the Access-Control-Allow-Origin header (#449)

# Release 1.25.0 17-10-2018

- Change side-way http to use 1.1 (#453)
- Upgrade gRPC from 1.4.2 to 1.15.0 (#450)
- CORS always adds the Access-Control-Allow-Origin header (#449)

# Release 1.24.0 29-08-2018

- Add --experimental_enable_multiple_api_configs flag (#439)

# Release 1.23.0 27-08-2018

- Proxy multiple Endpoints services (#434)
- Move GceMetadata fetching to start_esp. (#435)
- Harden code, not to free nullptr pool (#436)
- Add the start_esp_test to the BUILD file (#433)

# Release 1.22.0 16-08-2018

- Add two options --generate_config_file_only and --server_config_generation_path
- Skip the host header in the grpc metadata (#424)

# Release 1.21.0 11-07-2018

- Add JWT claims to the X-Endpoint-API-UserInfo header (#409)
- Add cloud_trace_url_override option
- add service_control_url_override option
- Add the parameter ssl_protocols to specify the SSL protocols

# Release 1.20.0 14-06-2018

- Add cors_with_regex preset with --cors_allow_origin_regex flag
- Add --cors_allow_credentials option
- Add SNI support to ESP for https clients (#399)

# Release 1.19.0 17-05-2018

- Added --disable_trace_sampling flag

# Release 1.18.0 10-05-2018

- Separated location related changes
- Added condition to check non_gcp and service_account_key
- Fixed metadata configuration for GCP
- Fix typo in the default CORS headers.
- Returns Status::OK when metadata service is not available (#386)

# Release 1.15.0 0-03-2018

- Added client IP address extraction for GAE Flex (#359)

# Release 1.14.0 01-03-2018

- add --worker-processes argument to scale nginx workers with number of cores
- Fixed the args.rollout_id not defined bug when --service_json_path is specified
- Not to set default location (#353)

# Release 1.13.0 24-01-2018

- Add option to generate self-signed certificate at start
- Added enable_websocket argument to start_esp
- Remove X-Endpoint-API-UserInfo header from clients (#339)
- support "grpc-status-details-bin" (#334)

# Release 1.12.0 14-12-2017

- Fix transcoding issue with h2 (#320)
- Fix possible segmentation fault in rewrite rule debugging mode (#318)
- Change skip_service_control not to skip auth. (#311)

# Release 1.11.0 07-11-2017

- Added --rewrite argument supporting rewrite rule
- Added --client_max_body_size argument
- Added --transcoding_always_print_primitive_fields true/false (#227)
- Added always_print_primitive_fields experimental option (#295)
- Added rewrite rule support instead of basepath match (#292)

# Release 1.10.0 24-10-2017

- Fix a bug in path match for OPTIONS. (#285)
- Bare minimum support for gRPC-Web. (#283)

# Release 1.9.0 27-09-2017

- Add start_esp options to allow more characters in HTTP headers
- Add --check_metadata option to disable checking metadata service
- Add --pid_file flag to configure PID file location.
- Add the support of JWK keys without the alg field (#274)

# Release 1.8.0 12-09-2017

- Auto detect custom verb to support : in the path. (#257)
- Upgrade nginx to 1.13.4 (#242)
- Adding ES256 for auth jwt validator (#231)
- Support Nginx restart (#252)
- Add IAP header support (#251)
- Adding authz cache. (#225)


# Release 1.7.0 22-08-2017

- Fixed the server config file backward compatibility

# Release 1.6.0 18-08-2017

- Integrate IAP JWT auto-verification for GAE Flex (#240)
- Added by-consumer metrics to report request, X-Endpoint-API-Project-I… (#235)
- Support authorization url (#228)
- Added esp service config rollouts info to /endpoints_status (#222)
- Update gRPC to 1.4.2 (#220)


# Release 1.5.0 14-07-2017

- Update NGINX to 1.13.3.
- Add support for skip_service_control usage rule.

# Release 1.4.0 30-05-2017

- Fixed a Firebase rule bug when api-key is in query parameter.
- Use producer project for Quota if api-key is not provided.
- Added /producer/by_consumer metrics.
- Add backend_protocol in the Report call.

# Release 1.3.0 25-04-2017

- Support endpoint authorization via firebase rules
- Update Dockerfile to expose port 8080 by default

# Release 1.2.0 12-04-2017

- Support rate-limit.
- Set 443 for the default port of https backend in start_esp.
- Support escaped / in the URL path.
- Support X-HTTP-Method-Override.
- not to send api_key in Report if service is not activated.
- Set gRPC max send/receive message size to unlimited.
- Support apikey based traffic restriction.
- Rename log entry name request_size to request_size_in_bytes.

# Release 1.1.0 01-03-2017

- Start deprecation of OpenAPI x-security to security (#101)
- Stop using api_key if service is not activated. (#98)
- Fail request if api_key is not valid
- Basic GRPC request compression support (#94)
- Support HEAD request in transcoding (#74)
- Rename release GCR images to gcr.io/endpoints-release (#60)
- NGINX high connection usage optimizations (#57)
- Make TLS client certificate optional in start_esp
- Start using AuthProvider audiences
- Notable bug fixes:
  * Fix ProxyFlow leak (#93)
  * Do not report latency for streaming requests
  * Validate if contents of x-jwks_uri contains a public key
- General improvements to testing and build infrastructure:
  * Update GRPC to 1.1.1
  * Update grpc test service.json (#61)
  * Add t test for fail wrong api key. (#104)
  * Fix grpc interop stress test script. (#103)
  * Use grpc-go for interop tests (#88)
  * Fix debian jessie package issue
  * Upgrade bazel to 0.4.4 (#92)
  * t-test changes to check that x-endpoint-api-userinfo is received by grpc (#96)
    service.
  * Add transcoding metadata test
  * Change scripts for new version file location.
  * Move nginx_repositories close to its load.
  * Fix start_esp main entry problem.
  * Change script/release_tag_git to use upstream. (#44)
  * Change release_tag_git to use absolute path.
  * Not to save huge access.log for GCE. (#41)
  * Fix bugs in script/release-publish. (#42)
  * Use newer protobuf.bzl (#39)
  * Use bazel to pull NGINX (#38)
  * Fix GRPC interop test BUILD file (#37)
  * Fix GRPC test BUILD file (#35)

# Release 1.0.1 06-12-2016

- Use GOOGLE_APPLICATION_CREDENTIALS in start-up script
- service_control_client not to send large Report (<1MB)
- Add max_report_size to statistics
- Package start-up script with Python PEX
- ESP CLI use same version for ESP docker image
- A workaround for Proto2.MessageOptions.* options
- Not call Check if api_key not provided
- Respect allow_cors configuration
- Log a warning if service control replies with a different service config

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
