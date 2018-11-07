# Files in this folder


## nginx.conf

This file is used for testing grpc and transcoding in local environment.

 - Download your service account secret file here as "service_account.json"

 - At top folder run:

   script/run_nginx -c $PWD/test/grpc/local/nginx.conf

 - Start backend grpc test as:

   bazel-bin/test/grpc/grpc-test-server 0.0.0.0:8081

You need to specify service name with `name:` field and producer project name
with `producer_project_id:` in the YAML service configs.

## Service Config: service.json

The `service.json` service config file is generated from
`grpc-test.proto` and `grpc-test.yaml` using gcloud.

 - At top folder run:

   protoc test/grpc/grpc-test.proto -Itest/grpc -I$(bazel info output_base)/external/googleapis_git/ --include_imports --descriptor_set_out=out.pb

   gcloud endpoints services deploy out.pb test/grpc/grpc-test.yaml

 - Download the service.json to this local folder. You might need [oauth2l](https://github.com/google/oauth2l) and a service account secret file.

   curl -H "`oauth2l -json service_account.json header cloud-platform`" 'https://servicemanagement.googleapis.com/v1/services/SERVICE_NAME/config?configId=SERVICE_CONFIG_ID' > test/grpc/local/service.json

## Service Config: interop_service.json

The `interop_service.json` service config file is generated from
grpc interop test proto and `grpc-interop.yaml` using

 - Downdload the grpc from https://github.com/grpc/grpc. It is better
   to use the same version as ESP. You can find its version in WORKSPACE file.

 - At grpc folder, run

   protoc src/proto/grpc/testing/test.proto -I. --include_imports --descriptor_set_out=out.descriptors

 - At api-compiler folder, run

   ./run.sh --configs $ESP/test/grpc/grpc-interop.yaml --configs endpoints.yaml --descriptor $GRPC/out.descriptors --json_out interop_service.json
