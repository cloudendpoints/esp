# Run ESP on Kubernetes

This document describes how to run ESP with
[Google Cloud Endpoints](https://cloud.google.com/endpoints/) integration on a
Kubernetes cluster that can run anywhere as long as it has internet access.

## Prerequisites

* [Set up a Kubernetes Cluster](http://kubernetes.io/docs/getting-started-guides/)
* [Installing `kubectl`](http://kubernetes.io/docs/user-guide/prereqs/)

## Before you begin

1. Select or create a [Cloud Platform Console project](https://console.cloud.google.com/project).

2. [Enable billing](https://support.google.com/cloud/answer/6293499#enable-billing) for your project.

3. Note the project ID, because you'll need it later.

4. Install [cURL](https://curl.haxx.se/download.html) for testing purposes.

5. [Enable Cloud Endpoints API](https://console.cloud.google.com/apis/api/endpoints.googleapis.com/overview)
   for your project in the Google Cloud Endpoints page in the API Manager.
   Ignore any prompt to create credentials.

6. [Download the Google Cloud SDK](https://cloud.google.com/sdk/docs/quickstarts).

## Configuring Endpoints

To configure Endpoints, replace YOUR-PROJECT-ID with your own project ID in
the [swagger.yaml](swagger.yaml) configuration file:

    swagger: "2.0"
    info:
      description: "A simple Google Cloud Endpoints API example."
      title: "Endpoints Example"
      version: "1.0.0"
    host: "YOUR-PROJECT-ID.appspot.com"

## Deploying the sample API

To deploy the sample application:

1. Invoke the following command:

        gcloud endpoints services deploy swagger.yaml

   The command returns several lines of information, including a line similar to the following:

       Service Configuration with version "2016-04-27R2" uploaded for service "YOUR-PROJECT-ID.appspot.com"

   Note that the version number that displayed will change when you deploy a new
version of the API.

2. Make a note of the service name and the service version because you'll need
them later when you configure the container cluster for the API.

# Deploying the sample API to the cluster #

To deploy to the cluster:

1. Edit the Kubernetes configuration file [esp_echo_http.yaml](esp_echo_http.yaml),
replacing SERVICE_NAME and SERVICE_VERSION shown in the snippet below with the
values returned when you deployed the API:

        containers:
        - name: esp
          image: gcr.io/endpoints-release/endpoints-runtime:1
          command: [
            "/usr/sbin/start_esp.py",
            "-p", "8080",
            "-a", "127.0.0.1:8081",
            "-s", "SERVICE_NAME",
            "-v", "SERVICE_VERSION",
            "-k", "/etc/nginx/creds/service-account-creds.json",
        ]

   Note you also need to change the service type from LoadBalancer to NodePort
   if you use [MiniKube](http://kubernetes.io/docs/getting-started-guides/minikube/)

2. [Create your service account credentials](https://cloud.google.com/storage/docs/authentication#generating-a-private-key)
   from Google API Console.

    * Save your credential as `service-account-creds.json`

3. Deploy the service account credentials to the cluster:

       kubectl create secret generic service-account-creds --from-file=service-account-creds.json

4. Start the service using the kubectl create command:

       kubectl create -f esp_echo_http.yaml

## Get the service's external IP address (skip this step if you use Minikube)

It can take a few minutes after you start your service in the container before
the external IP address is ready.

To view the service's external IP address:

1. Invoke the command:

       kubectl get service

2. Note the value for EXTERNAL-IP; you'll need it to send requests to the API.

## Sending a request to the sample API

After the sample API is running in the container cluster, you can send requests
to the API.

To send a request to the API

1. [Create an API key](https://console.cloud.google.com/apis/credentials)
   in the API credentials page.

   * Click Create credentials, then select API key > Server key, then click
     Create.

   * Copy the key, then paste it into the following export statement:

         export ENDPOINTS_KEY=AIza...

2. Send an HTTP request using curl, as follows,

   * If you don't use Minikube:

         curl -d '{"message":"hello world"}' -H "content-type:application/json" http://[EXTERNAL-IP]/echo?key=${ENDPOINTS_KEY}

   * Otherwise:

         NODE_PORT=`kubectl get service esp-echo --output='jsonpath="{.spec.ports[0]}"'`

         MINIKUBE_IP=`minikube ip`

         curl -d '{"message":"hello world"}' -H "content-type:application/json" ${MINIKUBE_IP}:${NODE_PORT}/echo?key=${ENDPOINTS_KEY}

## References

  * [echo sample code](https://github.com/GoogleCloudPlatform/python-docs-samples/tree/master/appengine/flexible/endpoints)
    (swagger.yaml in this directory is a copy from this echo sample)

  * [echo docker image](https://github.com/GoogleCloudPlatform/python-docs-samples/blob/master/appengine/flexible/endpoints/Dockerfile.container-engine)
