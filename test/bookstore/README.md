# Google Cloud Endpoints Bookstore App in Node.js

## Running locally

    npm install
    npm test
    npm start

In your web browser, go to the following address: http://localhost:8080.

## Deploying to Google App Engine

Step 1: Open the swagger.json file and in the `host` property, replace
`${MY_PROJECT_ID}` with the ID of the Google Cloud Platform project
where you'd like to deploy the sample application.

Step 2: Deploy the sample using `gcloud`:

    gcloud preview app deploy app.yaml --promote --project=<MY_PROJECT_ID>

Step 3: Test using the URL: https://${MY_PROJECT_ID}.appspot.com/.
