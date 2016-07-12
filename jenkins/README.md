# Jenkins Install #

## Creating Jenkins Cluster ##

Jenkins runs all its slaves in a Kubernetes Cluster. The cluster name is
hardcoded in the Jenkinsfile.

    $ export CLUSTER_NAME='jenkins-cluster-tmp'
    $ export PROJECT_ID='endpoints-jenkins'
    $ export ZONE='us-central1-f'

    $ gcloud container \
    --project "${PROJECT_ID}" \
    clusters create "${CLUSTER_NAME}" \
    --zone "${ZONE}" \
    --machine-type "n1-highmem-32" \
    --scopes\
    "https://www.googleapis.com/auth/appengine.admin",\
    "https://www.googleapis.com/auth/appengine.apis",\
    "https://www.googleapis.com/auth/bigquery",\
    "https://www.googleapis.com/auth/cloud-platform",\
    "https://www.googleapis.com/auth/compute",\
    "https://www.googleapis.com/auth/devstorage.full_control",\
    "https://www.googleapis.com/auth/devstorage.read_only",\
    "https://www.googleapis.com/auth/gerritcodereview",\
    "https://www.googleapis.com/auth/logging.write",\
    "https://www.googleapis.com/auth/projecthosting",\
    "https://www.googleapis.com/auth/service.management",\
    "https://www.googleapis.com/auth/servicecontrol" \
    --num-nodes "1" \
    --network "default" \
    --enable-cloud-logging \
    --no-enable-cloud-monitoring

## Adding jenkins cluster to kubectl config ##

You should be running this on Jenkins machine after cluster creation.

    # To ssh to jenkins
    $ gcloud container --project "${project_id}" --zone "${zone}" \
    compute ssh jenkins

    # On Jenkins VM, sudo as jenkins
    $ sudo su - jenkins

    # Update kubectl config.
    # You might want to run this command as well on your desktop.
    $ gcloud container --project "${project_id}" --zone "${zone}" \
    clusters get-credentials "${CLUSTER_NAME}"

    # You can run this command from jenkins or from your desktop
    $ kubectl create namespace jenkins-slaves

## Updating Jenkins to point to the cluster ##

First we need to find out where the kubernetes service is running

    $ kubectl  describe service kubernetes
    Name:                   kubernetes
    Namespace:              default
    Labels:                 component=apiserver,provider=kubernetes
    Selector:               <none>
    Type:                   ClusterIP
    IP:                     10.39.240.1
    Port:                   https   443/TCP
    Endpoints:              104.154.116.242:443
    Session Affinity:       None
    No events.

In this case the Endpoint is 104.154.116.242, so jenkins needs to
connect to https://104.154.116.242.

Point your browser to to go/esp-j/configure and find the 'Kubernetes'
section at the end of the page. The only things that needs to be updated
is 'Kubernetes URL' which should point to the URL above.
