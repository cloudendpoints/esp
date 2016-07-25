# Running Endpoints Server Proxy #

The location of the ESP binary is `bazel-bin/src/nginx/main/nginx-esp`,
under the root directory of your ESP Git repository clone (`${ESP}`).
For the remainder of the instructions we'll use Shell environment variable
`ESP_BINARY` to denote the location of the ESP binary:

    ESP_BINARY=${ESP}/bazel-bin/src/nginx/main/nginx-esp

## Start the Node.js backend ##

Make sure you have [Node.js](https://nodejs.org) installed before proceeding.
To install Node.js on Linux, you can download the binary distribution and:

    cd /usr/local
    sudo tar --strip-components 1 -xzf /path/to/node/tar/file

The example backend application is [bookstore](/test/bookstore):

    cd test/bookstore
    npm install         # one-time installation of dependencies
    npm test            # run bookstore unit tests

    # Run the bookstore backend
    npm start

    # Call the backend.
    curl http://localhost:8080/shelves


## Start the Endpoints Server Proxy ##

    # Create a directory for Nginx log files.
    mkdir -p ${TMPDIR}/esp/logs

    # If you built ESP using Bazel, run:
    ${ESP_BINARY} \
        -c ${ESP}/src/nginx/conf/esp.conf \
        -p ${TMPDIR}/esp

Aternatively, you can use the `${ESP}/script/run_nginx` script

    cd ${ESP}
    ${ESP}/script/run_nginx  \
        -c ${ESP}/src/nginx/conf/esp.conf

The script will build and run ESP. Note that the script won't do any of the
build setup (e.g., installing Bazel -- see [bazel.md](/doc/bazel.md)).

## Try calling the backend ##

Now we are ready to call the the backend via Endpoints Server Proxy, using curl:

    curl -v http://localhost:8090/shelves
    curl -v http://localhost:8090/shelves/1
    curl -v http://localhost:8090/shelves/1/books

The calls will fail but you will be able to see the logs in:

    cat ${TMPDIR}/esp/access.log
    cat ${TMPDIR}/esp/error.log

## Shutdown ##

Shutdown backend by pressing Ctrl+C

Shutdown nginx by running:

    ${ESP_BINARY} \
        -c ${ESP}/src/nginx/conf/esp.conf \
        -p ${TMPDIR}/esp \
        -s quit

Or

    ${ESP}/script/run_nginx  \
        -c ${ESP}/src/nginx/conf/esp.conf \
        stop

## Configure your project

If you don't have a project you can create one in [Google Developer
Console](https://console.developers.google.com).
Edit the ${ESP}/src/nginx/conf/bookstore.json file and add a
`producerProjectId` key with the id of your project at the top level and
replace the value of `name` with `YOUR_PROJECT_ID.appspot.com`.

```
"name":"YOUR_PROJECT_ID.appspot.com",
"producerProjectId":"YOUR_PROJECT_ID",
```

To do the necessary checks and to upload the logs to your project ESP will need
to do an authenticated call to service control using a service account. You can
create a service account for your project on the
[ServiceAccounts](https://console.developer.google.com/permissions/serviceaccounts)
page of the Developer Console. Download the json file with the private key
to `${ESP}/src/nginx/conf/service_control_secret.json` and uncomment the
`servicecontrol_secret` entry in `${ESP}/src/nginx/conf/esp.conf`.

## Call the backend

First, make a call without an API key. Edit `${ESP}/src/nginx/conf/bookstore.json`
and add `"allowUnregisteredCalls" : true` next to one of the methods (e.g.
`"selector": "bookstore.v1.BookstoreService.ListShelves"`) in the `usage->rules`
section. This will tell ESP that the method can be called without and api key.
Shutdown and start nginx again such that it picks up the configuration change or
alternatively send a `reload` signal to nginx to reload the configuration.

```
${ESP_BINARY} \
    -c ${ESP}/src/nginx/conf/esp.conf \
    -p ${TMPDIR}/esp \
    -s reload
```
Now call the endpoint.

```
curl -v http://localhost:8090/shelves
```

## Call the backend with an API key

Create a Browser/Server API key in the
[Credentials](https://console.developer.google.com/apis/credentials) page of the
Developer Console and make a call with an api key.

```
curl http://localhost:8090/shelves/1/books?key=<YOUR_API_KEY>
```

## Call an Authenticated endpoint

Open `${ESP}/src/nginx/conf/bookstore.json` and add an authentication
section in the global scope.

```json
"authentication": {
 "providers": [
   {
     "id": "test-auth-provider",
     "issuer": "loadtest@esp-test-client.iam.gserviceaccount.com",
     "jwksUri": "https://www.googleapis.com/service_accounts/v1/jwk/loadtest@esp-test-client.iam.gserviceaccount.com"
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
     "selector": "bookstore.v1.BookstoreService.GetShelf"
   }
 ]
},
```

This will tell ESP that the `GetShelf` operation is authenticated and will
configure various auth parameters:
  - who should be the issuer of auth tokens (`issuer`),
  - where to get the public keys of the issuer to validate the signature of
    the auth token (`jwksUri`),
  - what audiences the token should be issued for (`audiences`).

Reload the nginx configuration:

```
${ESP_BINARY} \
    -c ${ESP}/src/nginx/conf/esp.conf \
    -p ${TMPDIR}/esp \
    -s reload
```

Call the authenticated endpoint without a token and see that the request fails
with 401:

```
curl http://localhost:8090/shelves/1
```

Now generate a JWT auth token using the `${ESP}/client/custom/gen-auth-token.sh`
utility and the private key of this particular issuer in the
`${ESP}/client/custom/esp-test-client-secret-jwk.json` file:

```
${ESP}/client/custom/gen-auth-token.sh \
    -s ${ESP}/client/custom/esp-test-client-secret-jwk.json \
    -a "test-esp-audience"
```

And finally call the authenticated endpoint with the token.

```
curl http://localhost:8090/shelves/1?key=<YOUR_API_EY> \
   -H "Authorization: Bearer <GENERATED_TOKEN>"
```

## View the logs

Go to the [Endpoints](https://console.developer.google.com/endpoints) page,
select your project and click on Bookstore. You'll see various Bookstore
graphs reflecting the calls that you just did. Click View logs to see
the individual calls and locate the ones that you just did.
