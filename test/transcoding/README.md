# HTTP/JSON <=> gRPC Transcoding Sample

## Build & Run

 - replace `<YOUR_PROJECT_ID>` in `service.json` with your project ID
   (TIP: if you want to avoid setting up service-control authentication, delete
   the `control` section in the `service.json`),
 - run ESP using the `nginx.conf` in this directory
   (see [Running ESP](/doc/running.md)),
 - run the gRPC backend: `bazel run //test/transcoding:bookstore-server`,
 - try it out: `curl http://localhost:8090/shelves`.

## Service Config
The `service.json` service config file used in this sample is generated from
`bookstore.proto` and `bookstore-api.yaml` using
[API Compiler](https://github.com/googleapis/api-compiler). To regenerate:

 - follow the [API Compiler](https://github.com/googleapis/api-compiler)
   instructions and use `bookstore.proto` and `bookstore-api.yaml` to generate
   the `service.json` service config file,
 - move the values containing `<YOUR_PROJECT_ID>` and the `control` section to
   the top of the file, s.t. it's easier to edit.
