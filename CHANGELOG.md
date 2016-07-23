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

Endpoints Server Proxy, a.k.a. ESP is a proxy which enables API management
capabilities for API services. It supports HTTP/JSON and gRPC APIs.

## Features in this release

- Authentication (auth0, gitkit)
- API key validation
- API-level monitoring and logging
- Integration with Cloud Tracing
