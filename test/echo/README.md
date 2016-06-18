# Simple echo server for esp testing.

Node.js implementation of echo server.

Instructions:

Local nginx test:

1.  Start backend: `npm start`
2.  Start nginx: `../../script/update ${PWD}/local.conf`
3.  `curl [-d "xxx"] http://localhost:8090/echo?key=$API_KEY`
4.  For auth test `curl http://localhost:8090/echo/auth`, see below for details.

GAE test:

1.  You can test against deployed 'https://esp-echo.appspot.com' and skip 2 and 3
2.  Build docker: `docker/build-docker.sh`
3.  Deploy to GAE: `deploy-gae.sh`
    1.  deploy to a specific version `deploy-gae.sh '--set-default --version=2'`
4.  `curl [-d "xxx"] https://$GAE_HOST/echo?key=$API_KEY`
5.  For auth test `curl https://$GAE_HOST/echo/auth`, see below for details.

Auth test:

1.  "/echo/auth" endpoint requires auth credentials
2.  Generate an auth token with `gen-auth-token.sh [secret.json] [audience]`
    1.  Use one of `esp-echo-client-secret-jwk|x509|symmetric.json` to test
        jwk|x509|symmetric key cases, by default jwk is used.
    2.  You can also generate one by creating a service accout and download its client
        secret json file and use it to test jwk|x509 key. To test symmetric key, just
        create a json with "client_secret", "issuer" and "subject" field similar to
        `esp-echo-client-secret-symmetric.json`. You also need to modify service config
        to allow the issuer you specified.
    3.  "audience" by default is the service name `esp-echo.cloudendpointsapis.com`,
        other allowed audiences are specified in service config.
3.  `curl [-d "xxx"] https://$GAE_HOST/echo/auth?key=$API_KEY -H"Authorization: Bearer $token"`
