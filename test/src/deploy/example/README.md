# Kubernetes deployment script

First, start kubectl proxy:

    kubectl --proxy --port=9000 --context=YOUR_CLOUD_CONTEXT

Run `main.go` inside `example` directory as follows:

    go run main.go --all -k API_KEY -s SERVICE_ACCOUNT_TOKEN_FILE -ip IP

By default, the script requires a local installation of `minikube`. Set `IP` to
be the address of your minikube node (`minikube ip`).  If you have `kubectl`
configured with GKE, then use:

```
    go run main.go --all -k API_KEY
```

Notice that you do not need to provide the service account token, since ESP
fetches it from the metadata server.

By default, the script fetches Docker images from gcr.io for bookstore and ESP
for a fixed version of the test service.

You can control the phase of the testing by replacing `all` with `up`, `test`,
or `down`.

Refer to `go run main.go -h` for other parameters.

