# ESP Tutorial #

In this brief tutorial you will:

- install ESP prerequisites
- build ESP
- execute ESP unit and integration tests
- run ESP locally with an example Node.js Bookstore backend application

# Prerequisites #

On Mac OS X, install:

* [XCode](https://developer.apple.com/xcode/)
* [Java JDK 8](http://www.oracle.com/technetwork/java/javase/downloads/jdk8-downloads-2133151.html)

On Linux, install:

    # Software packages needed for building ESP
    sudo apt-get install -y \
         g++ git openjdk-8-jdk openjdk-8-source \
         pkg-config unzip uuid-dev zip zlib1g-dev

    # Software packages needed for building ESP
    sudo apt-get install libtool m4 autotools-dev automake

    # Software packages needed for running ESP tests
    sudo apt-get install libio-socket-ssl-perl

## Bazel ##

ESP is built using [Bazel](http://bazel.io) build tool. Install
[Bazel](http://bazel.io) version 0.5.4, following the [Bazel
documentation](http://bazel.io/docs/install.html).

*Note:* Bazel is under active development and from time to time, ESP continuous
integration systems are upgraded to a new version of Bazel. Currently, ESP
requires Bazel 0.5.4.

The version of Bazel used by ESP continuous integration systems can be found in
the [linux-install-software](/script/linux-install-software)
script variable `BAZEL_VERSION=<SHA>`.

# Building ESP #

Clone the ESP [GitHub repository](https://github.com/cloudendpoints/esp),
initialize Git submodules, and build ESP using Bazel. Detailed instructions
for building ESP on Ubuntu 16.04 can be found in the [document](/doc/build-esp-on-ubuntu-16-04.md).

    # Clone the ESP repository
    git clone https://github.com/cloudendpoints/esp

    cd esp

    # Initialize Git submodules
    git submodule update --init --recursive

    # Build ESP binary
    bazel build //src/nginx/main:nginx-esp

    # Run ESP unit and integration tests
    bazel test //src/... //third_party:all


The ESP binary location is:

    ./bazel-bin/src/nginx/main/nginx-esp


# Running ESP #

For the remainder of the tutorial we'll use Shell environment variable
`ESP_BINARY` to store the location of the ESP binary:

    ESP="$(git rev-parse --show-toplevel)"
    ESP_BINARY="${ESP}/bazel-bin/src/nginx/main/nginx-esp"

## Start the Node.js backend ##

Make sure you have [Node.js](https://nodejs.org) installed before proceeding.
To install Node.js on Linux, you can download the binary distribution and
unpack it:

    cd /usr/local
    sudo tar --strip-components 1 -xzf /path/to/node/tar/file

The example backend application is a simple
[bookstore service](/test/bookstore).
Start the application using [npm](https://www.npmjs.com/):

    cd "${ESP}/test/bookstore"
    npm install         # one-time installation of dependencies
    npm test            # run bookstore unit tests

    # Run the bookstore backend
    npm start

    # Call the backend (this calls the backend directly)
    curl http://localhost:8080/shelves


## Start ESP ##

    # Create a directory for Nginx log files
    mkdir -p "${TMPDIR}/esp/logs"

    # Start ESP
    "${ESP_BINARY}" \
        -p "${TMPDIR}/esp" \
        -c "${ESP}/src/nginx/conf/esp.conf"

## Call the backend ##

Now we are ready to call the the backend via Extensible Service Proxy, using curl:

    curl -v http://localhost:8090/shelves
    curl -v http://localhost:8090/shelves/1
    curl -v http://localhost:8090/shelves/1/books

The calls will fail because ESP is not yet completely configured (we'll do
that next) but you will be able to see the calls registered in the log files:

    cat "${TMPDIR}/esp/access.log"
    cat "${TMPDIR}/esp/error.log"

## Shutdown ESP ##

Shutdown ESP by running:

    "${ESP_BINARY}" \
        -p "${TMPDIR}/esp" \
        -c "${ESP}/src/nginx/conf/esp.conf" \
        -s quit

# Run ESP with Cloud Endpoints #

In the previous part of the tutorial we ran ESP locally, without [Google Cloud
Endpoints](https://cloud.google.com/endpoints) integration. In the next section
we'll enable integration with Google Cloud Endpoints and use ESP to manage your
local Bookstore API.

## Configure your cloud project ##

If you don't have a project, create one in [Google Developer
Console](https://console.developers.google.com).

In the `${ESP}/src/nginx/conf/bookstore.json` file, replace the two occurrences
of `MY_PROJECT_ID` with the ID of your Google cloud project. These two
occurrences are at the beginning of the `bookstore.json` configuration file:

- the `producerProjectId` value
- the service `name` value

```
{
  "name": "MY_PROJECT_ID.appspot.com",
  "producerProjectId": "MY_PROJECT_ID",
  "apis": [
    ...
```

With your bookstore.json configuration file updated, deploy the configuration
to Google Cloud Endpoints:

    gcloud components update
    gcloud endpoints services deploy \
        --project=MY_PROJECT_ID \
        "${ESP}/src/nginx/conf/bookstore.json"

## Authenticating ESP calls ##

In order to integrate with Google Cloud Endpoints, ESP needs to be able
to send authenticated requests to the Google Cloud Endpoints service
control. Service control enables logging, reporting of API related metrics,
validation of API keys, etc.

When ESP executes in a Google Compute Engine virtual machine, it will use
the virtual machine's service account to authenticate calls to the Endpoints
service control. To enable this integration for ESP executing locally, we will
give ESP the service account credentials from your project.

You can create a service account for ESP to use with your cloud project in the
[Service Accounts](https://console.cloud.google.com/iam-admin/serviceaccounts)
page of the Developer Console. Download the JSON file with the private key
to `${ESP}/src/nginx/conf/service_account.json` and uncomment the
`servicecontrol_secret` entry in `${ESP}/src/nginx/conf/esp.conf`:

```
endpoints {
  on;
  api bookstore.json;
  servicecontrol_secret service_account.json;
}
```

## Start ESP with Cloud Endpoints ##

Once you have deployed the Bookstore API configuration to Google Cloud
Endpoints and provided a service account credentials to ESP, you can
restart ESP:

    # Start ESP
    "${ESP_BINARY}" \
        -p "${TMPDIR}/esp" \
        -c "${ESP}/src/nginx/conf/esp.conf"

## Call ESP ##

First, we call an API method which doesn't require an API key.
The method lists all available shelves in the bookstore:

    curl -v http://localhost:8090/shelves

The response should include:

```
{"shelves":[
    {"name":"shelves/1","theme":"Fiction"},
    {"name":"shelves/2","theme":"Fantasy"}
]}
```

Next, we call a method which is configured to require an API key;
a method which returns books on a bookstore shelf:

    curl http://localhost:8090/shelves/1/books

This call fails with a message: `Method doesn't allow unregistered callers`.
To authenticate the method call, create a Browser or a Server API key in the
[Credentials](https://console.developer.google.com/apis/credentials) page of the
Developer Console. Repeat the call with the generated API key:

    curl http://localhost:8090/shelves/1/books?key=<YOUR_API_KEY>

This time, the call successfully returns the list of all fiction books:

```
{ "books": [
  { "name":"shelves/1/books/3",
    "author":"Neal Stephenson",
    "title":"REAMDE"
  }
]}
```

## Create an authenticated endpoint ##

Open `${ESP}/src/nginx/conf/bookstore.json` and replace the (initially empty)
`authentication` section in the global scope with:

```json
"authentication": {
 "providers": [
   {
     "id": "test-auth-provider",
     "issuer": "test-client@esp-test-client.iam.gserviceaccount.com",
     "jwksUri": "https://www.googleapis.com/service_accounts/v1/jwk/test-client@esp-test-client.iam.gserviceaccount.com"
   }
 ],
 "rules": [
   {
     "requirements": [
       {
         "audiences": "test-esp-audience",
         "providerId": "test-auth-provider"
       }
     ],
     "selector": "GetShelf"
   }
 ]
},
```

This tells ESP that the `GetShelf` operation is authenticated and
configures authentication parameters:

  - who should be the issuer of the auth tokens (`issuer`),
  - where to get the public keys of the issuer to validate the signature of
    the auth tokens (`jwksUri`),
  - what audiences the tokens should be issued for (`audiences`).

Reload the nginx configuration:

    "${ESP_BINARY}" \
        -c "${ESP}/src/nginx/conf/esp.conf" \
        -p "${TMPDIR}/esp" \
        -s reload

Call the updated endpoint without a token and observe that the request fails
with a JWT validation failure:

    curl http://localhost:8090/shelves/1

Now generate a JWT auth token using the `${ESP}/client/custom/gen-auth-token.sh`
utility and the private key of this particular issuer in the
`${ESP}/client/custom/esp-test-client-secret-jwk.json` file:

    "${ESP}/client/custom/gen-auth-token.sh" \
        -s "${ESP}/client/custom/esp-test-client-secret-jwk.json" \
        -a "test-esp-audience"

And finally call the authenticated endpoint with the token.

    curl http://localhost:8090/shelves/1?key=<YOUR_API_KEY> \
       -H "Authorization: Bearer <GENERATED_TOKEN>"

## View the logs ##

Visit the [Endpoints](https://console.developer.google.com/endpoints) page, in
the Google cloud developer console, select your project and click on the
Bookstore API. There you can find API metrics and links to logs that correspond
to the API calls you just made using curl.

# Where to go next? #

Congratulations on completing the ESP tutorial! You have successfully:

- built ESP from source
- ran all unit and integration tests locally
- ran ESP locally with a local Bookstore backend
- integrated your local ESP with Endpoins Service Control to enable Endpoints
  features (logging, monitoring, API Key validation).

To learn more about Endpoints, you can visit our
[Google Cloud Endpoints](https://cloud.google.com/endpoints) documentation
which includes more examples and information on using Endpoints with Google
Compute Engine, Google Container Engine, and Google App Engine Flex.
