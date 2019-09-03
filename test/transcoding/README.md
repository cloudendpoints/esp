# HTTP/JSON <=> gRPC Transcoding Sample

## Build & Run

 - replace `<YOUR_PROJECT_ID>` in `service.pb.txt` with your project ID
   (TIP: if you want to avoid setting up service-control authentication, delete
   the `control` section in the `service.pb.txt`),
 - run ESP using the `nginx.conf` in this directory
   (see [Running ESP](/doc/running.md)),
 - run the gRPC backend: `bazel run //test/transcoding:bookstore-server`,
 - try it out: `curl http://localhost:8090/shelves`.

## Service Config
The `service.pb.txt` service config file used in this sample is generated from
`bookstore.proto` and `bookstore-api.yaml`. The steps to regenerate:

 - Use protoc to generate proto api_descriptor.pb as
```
bazel build @protobuf_git//:protoc
cd test/transcoding
../../bazel-bin/external/protobuf_git/protoc --include_imports --include_source_info --proto_path=. --proto_path=../../bazel-esp/external/protobuf_git/src --descriptor_set_out=api_descriptor.pb bookstore.proto
```

 - Modify bookstore-api.yaml to your project and use gcloud CLI to deploy it:
```
sed -i "s|<YOUR_PROJECT_ID>.appspot.com|YOUR-SERVICE-NAME|g" bookstore-api.yaml
gcloud endpoints services deploy api_descriptor.pb bookstore-api.yaml
```

 - Download the config from Inception
```
gcloud endpoints configs list --servicve=YOUR_SERVICE_NAME
LAST_CONFIG_ID=$(THE TOP ONE FROM ABOVE OUTPUT)
gcloud endpoints configs describe LAST_CONFIG_ID --servicve=YOUR_SERVICE_NAME --format=json > /tmp/out.json
```

 - Use src/tools/esp_config_gen to convert to text format. The text format
 from gcloud is not the same as the proto text format desired
```
 bazel build //src/tools:all
 bazel-bin/src/tools/esp_config_gen --src /tmp/out.json --text > /tmp/out.txt
```

 - Modify the /tmp/out.txt:
   * remove all fields already in src/nginx/t/testdata/logs_metrics.pb.txt
   * replace following fields
```
producer_project_id: "<YOUR_PROJECT_ID>"
name: "<YOUR_PROJECT_ID>.appspot.com"
id: "2016-08-25r1"
```
   * add following rules to http rule:
 ```
  rules {
    selector: "endpoints.examples.bookstore.Bookstore.CreateBook"
    post: "/shelves/{shelf}/books/{book.id}/{book.author}"
    body: "book.title"
  }
 ```
 - Replace /tmp/out.txt with test/transcoding/service.pb.txt

