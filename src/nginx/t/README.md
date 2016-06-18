# API Manager integration tests

To run all tests:

    bazel test //src/nginx/t/...

To run a specific test (e.g. `metadata_fail`):

    bazel test //src/nginx/t:metadata_fail

To run a test in order to inspect its NGINX error log (e.g. `metadata_fail`):

    bazel build -c dbg //src/nginx/t:metadata_fail
    TMPDIR=$PWD TEST_NGINX_LEAVE=1 bazel-bin/src/nginx/t/metadata_fail

The test files, including NGINX error log will be in a directory $PWD/nginx-test-*

Note: The `*-client-secret*.json` files were generated from a test service
account and were immediately revoked. They are used in auth tests, please don't
modify them.
