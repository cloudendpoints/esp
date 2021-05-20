# Debug ESP CI  on Jenkins

ESP runs the presubmit unit/integration/e2e tests on Jenkins. The local tests pass
may not necessarily make ESP CI pass as different environment. The most common reasons is
some dependency incompatibility.

## How to locally reproduce the CI failure

#### Getting the CI image
The address of docker image used to run CI is is located in [Jenkinsfile](https://github.com/cloudendpoints/esp/blob/master/Jenkinsfile) under var `SLAVE_IMAGE`, like 
```
SLAVE_IMAGE = 'gcr.io/endpoints-jenkins/debian-9:0.13'
```

Pull the image from registry
```
gcloud docker -- pull ${SLAVE_IMAGE}
```

#### Locally reproduce
Run the docker image with attaching your local `esp` repo path
```shell script
docker container run -v ${LOCAL_ESP_REPO_PATH}:/home/esp \
 -it  ${SLAVE_IMAGE}  /bin/bash 
```

After sshing inside the container, reproduce the failed test target found in the test jobs, like 
```
cd /hom/esp
bazel test start_esp/test/...
```

####  Start investigating...


### Related file Locations
- [Jenkinsfile](https://github.com/cloudendpoints/esp/blob/master/Jenkinsfile): the central place to config all the CI tests
- [script/presubmits](https://github.com/cloudendpoints/esp/blob/master/script/presubmits): the entry of presumit tests
- [jenkins/slaves/debian-9.Dockerfile](https://github.com/cloudendpoints/esp/blob/master/jenkins/slaves/debian-9.Dockerfile): the dockerfile of CI image
- [script/linux-install-software](https://github.com/cloudendpoints/esp/blob/master/script/linux-install-software): the script of installing related dependencies of CI image