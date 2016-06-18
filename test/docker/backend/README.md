# Google Cloud Endpoints Bookstore App in Node.js

A simple backend application to be used for Docker test of the
Google Cloud Endpoints proxy.

The service bookstore-backend.endpointsv2.appspot.com is produced
by the 'endpointsv2' project. Project running continuous integration
must have access to endpointsv2 project in order to fetch the service
configuration from Service Management.

The service version 2016-04-25R1 is the current version of the API
specification defined in swagger.json. If swagger.json is modified
it should be uploaded to Service Management using gcloud. After that,
the service configuration version will need to be updated.
