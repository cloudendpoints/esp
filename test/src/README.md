## Deployment driver script

### Kubernetes

First, start kubectl proxy:

    kubectl --proxy --port=9000 --context=YOUR_CLOUD_CONTEXT

Run `driver.go` as follows:

    go run driver.go --test gke -k API_KEY --cred SERVICE_ACCOUNT_TOKEN_FILE --ip IP

By default, the script requires a local installation of `minikube`. Set `IP` to
be the address of your minikube node (`minikube ip`).  If you have `kubectl`
configured with GKE:

```
    go run driver.go --test gke -k API_KEY
```

Notice that you do not need to provide the service account token, since ESP
fetches it from the metadata server.

By default, the script fetches Docker images from gcr.io for bookstore and ESP
for a fixed version of the test service.

You can control the phase of the testing with `up`, `test`, or `down`.

Refer to `go run driver.go -h` for other parameters.

