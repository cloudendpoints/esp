Kubernetes deployment script

Run `main.go` inside `example` directory as follows:

    go run main.go --all -k API_KEY -s SERVICE_ACCOUNT_TOKEN_FILE

By default, the script requires a local installation of `minikube`.
If you have `kubectl` configured with GKE, then use:
```
    go run main.go --all -t LoadBalancer -k API_KEY
```
Notice that you do not need to provide the service account token, since ESP can
fetch it from the metadata server.

By default, the script fetches Docker images from gcr.io for bookstore and ESP.

Refer to `go run main.go -h` for other parameters.

