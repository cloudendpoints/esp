# Generating .pb.go files under `test/src/google` directory

## 1) Prepare the necessary tools & protos
Fetch the necessary dependencies - googleapis (for service-config protos),
protobuf (for dependencies such as api.proto & type.proto), the Go protobuf
library and the protoc-gen-go tool. E.g.
```sh
ESP="$(git rev-parse --show-toplevel)"

# Fetch the submodules
${ESP}/script/setup

# Build protobuf to make sure it's fetched
bazel build //external:protobuf

GOOGLEAPIS="${ESP}/third_party/googleapis"
CONFIG="${ESP}/third_party/config"
PROTOBUF="${ESP}/bazel-esp/external/protobuf_git"
```
Pull the `protoc-gen-go` tool.
```sh
WORKDIR="$(mktemp -d)"
GOPATH="${WORKDIR}" go get -u 'github.com/golang/protobuf/protoc-gen-go'
export PATH="${WORKDIR}/bin":$PATH
```

## 2) Prepare the protos
Copy service-config, service-control and protobuf .proto files into one
directory structure. E.g.,
```sh
mkdir "${WORKDIR}/proto"

# Copy service control proto
cp -rf "${CONFIG}"/* "${WORKDIR}/proto/"

# Copy service config proto
cp -rf ${GOOGLEAPIS}/google/api  "${WORKDIR}/proto/google/"

# Copy necessary dependencies from protobuf
mkdir "${WORKDIR}/proto/google/protobuf"
DEPS="api.proto descriptor.proto source_context.proto type.proto"
for d in ${DEPS}; do
  cp -rf ${PROTOBUF}/src/google/protobuf/$d "${WORKDIR}/proto/google/protobuf/"
done
```

## 3) Remove `go_package` declarations
Some protos have `option go_package ...` declaration, which forces the Go proto
compiler to use a specific Go import path. We need to remove these to be able to
control the import paths. Use `grep 'go_package' "${WORKDIR}/proto" -lr` to
find the proto files with `option go_package ...` declarations and remove the
declarations.

## 4) Generate
Finally, invoke `protoc` for each directory (package) to generate the code. E.g.
```sh
mkdir "${WORKDIR}/output"
pushd "${WORKDIR}/proto"

PACKAGES="google/protobuf google/api google/rpc google/logging/type google/type"
for p in ${PACKAGES}; do
  protoc \
    --go_out="import_path=${p}:${WORKDIR}/output" \
    --proto_path . \
    --proto_path ${PROTOBUF}/src \
    "${p}"/*.proto
done

# Compile service control protos separately as it's package name is different
# from it's path
protoc \
  --go_out="import_path=google/api/servicecontrol:${WORKDIR}/output" \
  --proto_path . \
  --proto_path ${PROTOBUF}/src \
  google/api/servicecontrol/v1/*.proto;

popd
```

## 5) Copy the generated source, test & cleanup
Copy the generated .pb.go files into the `test/src/google` directory & remove
the temp working dir.
```sh
rm -rf "${ESP}"/test/src/google/*/
cp -rf "${WORKDIR}"/output/google/* "${ESP}/test/src/google/"
```
Run the tests and make sure they pass.
```sh
bazel test //test/src:all
```
Remove the temp directory.
```sh
rm -rf "${WORKDIR}"
```

