#!groovy
/*
Copyright (C) Endpoints Server Proxy Authors
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.

##############################################################################
*/

import static java.util.UUID.randomUUID
import java.io.File;

BUCKET = 'endpoints-jenkins.appspot.com'
CLUSTER = 'jenkins-cluster-tmp'
PROJECT_ID = 'endpoints-jenkins'
ZONE = 'us-central1-f'
SERVICE_MGMT_URL = 'https://servicemanagement.googleapis.com/'

// Supported Stages.
// ALL will run all stage but the one starting with '_'.
// By default all ALL stages will run.
ALL_STAGES = 'ALL'
E2E_STAGE = 'E2E'
// If stage starts with '_' it should be called explicitly.
// To do so, add a String Build Parameter in your project and call it 'STAGE'
// Set that build parameter to the stage that you want to start.
CLEANUP_STAGE = '_CLEANUP'
PERFORMANCE_STAGE = '_PERFORMANCE'
SLAVE_UPDATE_STAGE = '_SLAVE_UPDATE'

SUPPORTED_STAGES = [
    ALL_STAGES,
    E2E_STAGE,
    CLEANUP_STAGE,
    PERFORMANCE_STAGE,
    SLAVE_UPDATE_STAGE
]

// Supported VM Images
DEBIAN_JESSIE = 'debian-8'
CONTAINER_VM = 'container-vm'

// Slaves Docker tags
DOCKER_SLAVES = [
    (DEBIAN_JESSIE): 'gcr.io/endpoints-jenkins/debian-8-slave',
]

// Release Qualification end to end tests.
// If DEBIAN_PACKAGE_REPO build parameter is set only those test will run.
// We can filter down the number of tests by using the E2E_FILTERS build
// parameter.
// Please Update script/validate_release.py when adding or removing long-run-test.
RELEASE_QUALIFICATION_BRANCHES = [
    'flex-off-endpoints-on',
    'gce-container-vm',
    'gce-debian-8',
    'gke-tight-coupling-grpc',
    'gke-tight-coupling-https'
]

// Source Code related variables. Set in stashSourceCode.
GIT_SHA = ''
ESP_RUNTIME_VERSION = ''

node {
  stashSourceCode()
  setArtifactsLink()
}

node('master') {
  def nodeLabel = getSlaveLabel(DEBIAN_JESSIE)
  try {
    if (runStage(CLEANUP_STAGE)) {
      stage 'Test Cleanup'
      cleanupOldTests(nodeLabel)
    }
    if (runStage(SLAVE_UPDATE_STAGE)) {
      stage 'Slave Update'
      // This needs to run on master as GKE uses a very old version of docker.
      // Using a new workspace to checkout code in.
      ws {
        buildNewDockerSlave(nodeLabel)
      }
    }
    if (runStage(PERFORMANCE_STAGE)) {
      stage 'Build Artifacts'
      buildArtifacts(nodeLabel, false, false)
      stage 'Performance Test'
      performance(nodeLabel)

    }
    if (runStage(E2E_STAGE)) {
      stage 'Build Artifacts'
      buildArtifacts(nodeLabel)
      stage 'E2E Tests'
      e2eTest(nodeLabel)
    }
  } catch (Exception e) {
    sendFailureNotification()
    error(e.getMessage())
  }
}

def cleanupOldTests(nodeLabel) {
  def branches = [
      'cleanup_endpoints-jenkins': {
        // Delete 6 days old namespaces and GCE instances on endpoints-jenkins.
        node(nodeLabel) {
          testCleanup(6, PROJECT_ID, '-i -n')
        }
      },
      'cleanup_esp-load-test': {
        // Delete 2 days old Flex versions on esp-load-test.
        node(nodeLabel) {
          testCleanup(2, 'esp-load-test', '-v')
        }
      },
      'cleanup_esp-long-run': {
        // Delete 7 days old Flex versions on esp-long-run.
        node(nodeLabel) {
          testCleanup(7, 'esp-long-run', '-v')
        }
      }
  ]
  parallel branches
}

def buildArtifacts(nodeLabel, buildBookstore = true, buildGrpcTest = true) {
  def branches = [
      'esp_debian': {
        node(nodeLabel) {
          buildPackages()
        }
      },
      'auth_token_gen': {
        node(nodeLabel) {
          buildAndStash(
              '//src/tools:auth_token_gen',
              'bazel-bin/src/tools/auth_token_gen',
              'auth_token_gen')
        }
      }
  ]
  if (buildBookstore) {
    branches['bookstore'] = {
      node(nodeLabel) {
        buildBookstoreImage()
      }
    }
  }
  if (buildGrpcTest) {
    branches['grpc_test'] = {
      node(nodeLabel) {
        buildGrcpTest()
      }
    }
  }
  parallel branches
}

def performance(nodeLabel) {
  def branches = [
      'jenkins-post-submit-perf-test': {
        node(nodeLabel) {
          localPerformanceTest()
        }
      },
      'jenkins-perf-test-vm-esp': {
        node(nodeLabel) {
          flexPerformance()
        }
      }
  ]
  parallel branches
}

def e2eTest(nodeLabel) {
  // Please Update script/validate_release.py when adding or removing test.
  def branches = [
      'gke-tight-coupling-https': {
        node(nodeLabel) {
          e2eGKE('tight', 'https')
        }
      },
      'gce-debian-8': {
        node(nodeLabel) {
          e2eGCE(DEBIAN_JESSIE)
        }
      },
      'gce-container-vm': {
        node(nodeLabel) {
          e2eGCEContainer(CONTAINER_VM)
        }
      },
      'gke-tight-coupling-http': {
        node(nodeLabel) {
          e2eGKE('tight', 'http')
        }
      },
      'gke-tight-coupling-custom': {
        node(nodeLabel) {
          e2eGKE('tight', 'custom')
        }
      },
      'gke-loose-coupling-http': {
        node(nodeLabel) {
          e2eGKE('loose', 'http')
        }
      },
      'gke-loose-coupling-https': {
        node(nodeLabel) {
          e2eGKE('loose', 'https')
        }
      },
      'gke-loose-coupling-custom': {
        node(nodeLabel) {
          e2eGKE('loose', 'custom')
        }
      },
      'flex-off-endpoints-on': {
        node(nodeLabel) {
          e2eFlex(true, false)
        }
      },
      'flex-off-endpoints-off': {
        node(nodeLabel) {
          e2eFlex(false, false)
        }
      },
      'gce-container-vm-grpc': {
        node(nodeLabel) {
          e2eGCEContainer(CONTAINER_VM, true)
        }
      },
      'gke-tight-coupling-grpc': {
        node(nodeLabel) {
          e2eGKE('tight', 'grpc')
        }
      },
  ]

  branches = filterBranches(branches, getE2eFilters())
  if (isReleaseQualification()) {
    branches = filterBranches(branches, RELEASE_QUALIFICATION_BRANCHES.join('|'))
  }

  withEnv(
      ["CLOUDSDK_API_ENDPOINT_OVERRIDES_SERVICEMANAGEMENT=${getServiceManagementUrl()}"]) {
    parallel branches
  }
}
/*
Esp code related method.
Need to call checkoutSourceCode() for those to work.
 */

def espGenericDockerImage(suffix = '') {
  def serverConfigTag = createServerConfigTag()
  if (serverConfigTag != '') {
    suffix = "${suffix}-${serverConfigTag}"
  }
  if (getServiceManagementUrl() != '') {
    suffix = "${suffix}-staging"
  }
  return "gcr.io/${PROJECT_ID}/endpoints-runtime${suffix}:debian-git-${GIT_SHA}"
}

def espDockerImage() {
  if (isRelease()) {
    return "b.gcr.io/endpoints/endpoints-runtime:${ESP_RUNTIME_VERSION}"
  }
  return espGenericDockerImage()
}

def espGrpcDockerImage() {
  return espGenericDockerImage('-grpc')
}

def espFlexDockerImage() {
  return espGenericDockerImage('-flex')
}

def isRelease() {
  return getDebianPackageRepo() != ''
}

def isReleaseQualification() {
  if (isRelease()) {
    return true
  }
  return getReleaseQualification()
}

def isDefaultCleanNginxBinary() {
  return (getServiceManagementUrl() == '' && serverConfig == '')
}

def bookstoreDockerImage() {
  return "gcr.io/${PROJECT_ID}/bookstore:${GIT_SHA}"
}

def gRpcTestServerImage() {
  return "gcr.io/${PROJECT_ID}/grpc-test-server:${GIT_SHA}"
}

def espDebianPackage() {
  def suffix = ''
  if (getServiceManagementUrl() != '') {
    suffix = "${suffix}-staging"
  }
  return "gs://${BUCKET}/${GIT_SHA}/artifacts/endpoints-runtime-amd64${suffix}.deb"
}

def getEndpointsRuntimeVersion() {
  // Need to checkoutSourceCode() first
  return readFile('include/version').trim()
}

/*
Stages
 */

def buildPackages() {
  checkoutSourceCode()
  def espImgGeneric = espDockerImage()
  def serverConfig = getServerConfig()
  def serverConfigFlag = ''
  if (serverConfig != '') {
    serverConfigFlag = "-c ${serverConfig}"
  }
  if (isRelease()) {
    // Docker and debian package are built from Robot.
    sh "docker pull ${espImgGeneric} || { echo 'Cannot find ${espImgGeneric}'; exit 1; }"
  } else {
    def espDebianPackage = espDebianPackage()
    def espImgFlex = espFlexDockerImage()
    def espImgGrpc = espGrpcDockerImage()
    def serviceManagementUrl = getServiceManagementUrl()
    if (serviceManagementUrl != '') {
      sh "sed -i s,${SERVICE_MGMT_URL},${serviceManagementUrl},g " +
          "src/nginx/main/fetch_service_config.sh"
    }

    sh "script/robot-release " +
        "-m ${espImgFlex} " +
        "-g ${espImgGeneric} " +
        "-d ${espDebianPackage} " +
        "-r ${espImgGrpc}" +
        "${serverConfigFlag} -s"
    // local perf builds its own esp binary package.
    // Using the one built here instead if it exists.
    if (isDefaultCleanNginxBinary()) {
      // We only want to stash for the clean default binary.
      def binaryExists = fileExists 'bazel-bin/src/nginx/main/nginx-esp'
      if (binaryExists) {
        fastStash('nginx-esp', 'bazel-bin/src/nginx/main/nginx-esp')
      }
    }
  }
}

def buildBookstoreImage() {
  setGCloud()
  checkoutSourceCode()
  def bookstoreImg = bookstoreDockerImage()
  sh "test/bookstore/linux-build-bookstore-docker -i ${bookstoreImg}"
}

def buildGrcpTest() {
  def gRpcServerImg = gRpcTestServerImage()
  buildAndStash(
      '//test/grpc:grpc-test-client //test/grpc:grpc-test-server',
      'bazel-bin/test/grpc/grpc-test-client',
      'grpc_test_client')
  def codeCheckedOut = fileExists 'test/grpc/linux-build-grpc-docker'
  if (!codeCheckedOut) {
    setGCloud()
    checkoutSourceCode()
  }
  sh "test/grpc/linux-build-grpc-docker -i ${gRpcServerImg}"
}

def buildAndStash(buildTarget, stashTarget, name) {
  if (pathExistsCloudStorage(stashArchivePath(name))) return
  setGCloud()
  checkoutSourceCode()
  // Turns out Bazel does not like to be terminated.
  // Setting this to 30 minutes.
  timeout(30) {
    sh "bazel build --config=release ${buildTarget}"
  }
  fastStash(name, stashTarget)
}

def testCleanup(daysOld, project, flags) {
  setGCloud()
  checkoutSourceCode()
  sh "script/jenkins-tests-cleanup.sh -d ${daysOld} -p ${project} -f ${flags}"
}

def buildNewDockerSlave(nodeLabel) {
  setGCloud()
  checkoutSourceCode()
  def dockerImage = "${DOCKER_SLAVES[nodeLabel]}:${GIT_SHA}"
  // Test Slave image setup in Jenkins
  def testDockerImage = "${DOCKER_SLAVES[nodeLabel]}:test"
  // Slave image setup in Jenkins
  def finalDockerImage = "${DOCKER_SLAVES[nodeLabel]}:latest"
  echo "Building ${testDockerImage}"
  sh "script/jenkins-build-docker-slave -b " +
      "-i ${dockerImage} " +
      "-t ${testDockerImage} " +
      "-s ${nodeLabel}"
  echo "Testing ${testDockerImage}"
  node(getTestSlaveLabel(nodeLabel)) {
    setGCloud()
    checkoutSourceCode()
    sh "jenkins/slaves/slave-test"
  }
  echo "Retagging ${testDockerImage} to ${dockerImage}"
  sh "script/jenkins-build-docker-slave " +
      "-i ${testDockerImage} " +
      "-t ${finalDockerImage}"
}

def e2eCommonOptions(testId, prefix = '') {
  def uniqueID = getUniqueID(testId, true)
  def skipCleanup = getSkipCleanup() ? "-s" : ""
  def serviceName = generateServiceName(uniqueID, prefix)
  def durationHour = getDurationHour()
  return "-a ${serviceName} " +
      "-B ${BUCKET} " +
      "-i ${uniqueID} " +
      "-l ${durationHour} " +
      "${skipCleanup} "
}

def e2eGKE(coupling, testType) {
  setGCloud()
  checkoutSourceCode()
  fastUnstash('auth_token_gen')
  def testId = "gke-${coupling}-${testType}"
  def gRpc = testType == 'grpc'
  def gRpcFlag = ''
  def commonOptions = e2eCommonOptions(testId, gRpc ? 'grpc-' : '')
  def espImage = espDockerImage()
  def backendImage = bookstoreDockerImage()
  if (gRpc) {
    fastUnstash('grpc_test_client')
    backendImage = gRpcTestServerImage()
    espImage = espGrpcDockerImage()
    gRpcFlag = '-g'
  }
  echo 'Running GKE test'
  sh "test/bookstore/gke/e2e.sh " +
      commonOptions +
      "-b ${backendImage} " +
      "-e ${espImage} " +
      "-t ${testType} " +
      "-c ${coupling} " +
      "${gRpcFlag}"
}

def e2eGCE(vmImage) {
  setGCloud()
  checkoutSourceCode()
  fastUnstash('auth_token_gen')
  def commonOptions = e2eCommonOptions('gce-raw')
  def espDebianPkg = espDebianPackage()
  def debianPackageRepo = getDebianPackageRepo()
  echo 'Running GCE test'
  sh "test/bookstore/gce/e2e.sh " +
      commonOptions +
      "-d ${espDebianPkg} " +
      "-V ${ESP_RUNTIME_VERSION} " +
      "-v ${vmImage} " +
      "-r \"${debianPackageRepo}\""
}

def e2eGCEContainer(vmImage, gRpc = false) {
  setGCloud()
  checkoutSourceCode()
  fastUnstash('auth_token_gen')
  def commonOptions = e2eCommonOptions('gce-container', gRpc ? 'grpc-' : '')
  def espImage = espDockerImage()
  def backendImage = bookstoreDockerImage()
  def gRpcFlag = ''
  if (gRpc) {
    fastUnstash('grpc_test_client')
    backendImage = gRpcTestServerImage()
    espImage = espGrpcDockerImage()
    gRpcFlag = '-g'
  }
  echo 'Running GCE container test'
  sh "test/bookstore/gce-container/e2e.sh " +
      commonOptions +
      "-b ${backendImage} " +
      "-e ${espImage} " +
      "-v ${vmImage} " +
      "${gRpcFlag}"
}

def localPerformanceTest() {
  // This will not work for staging or server-config.
  // In order to fully support this we'll need to use the debian
  // packages created in buildPackages().
  setGCloud()
  checkoutSourceCode()
  if (pathExistsCloudStorage(stashArchivePath('nginx-esp'))) {
    // Using binary build by buildArtifacts()
    fastUnstash('auth_token_gen')
    fastUnstash('nginx-esp')
  }
  def testId = 'jenkins-post-submit-perf-test'
  def uniqueId = getUniqueID('local-perf', false)
  def logBucket = "gs://${BUCKET}/${GIT_SHA}/logs"
  sh "script/create-test-env-json " +
      "-t ${testId} " +
      "-i ${uniqueId} > TEST_ENV"
  def testEnv = readFile('TEST_ENV').trim()
  sh 'script/linux-prep-machine'
  sh "script/linux-start-local-test " +
      "-t ${testEnv} " +
      "-b ${logBucket}"
}

def flexPerformance() {
  setGCloud()
  checkoutSourceCode()
  fastUnstash('auth_token_gen')
  def testId = 'jenkins-perf-test-vm-esp'
  def uniqueId = getUniqueID('flex-perf', false)
  sh "script/create-test-env-json " +
      "-t ${testId} " +
      "-i ${uniqueId} > TEST_ENV"
  def testEnv = readFile('TEST_ENV').trim()
  def logBucket = "gs://${BUCKET}/${GIT_SHA}/logs"
  def espImgFlex = espFlexDockerImage()
  sh "script/linux-test-vm-echo " +
      "-i ${espImgFlex} " +
      "-t ${testEnv} " +
      "-b ${logBucket}"
}

def e2eFlex(endpoints, flex) {
  setGCloud()
  checkoutSourceCode()
  fastUnstash('auth_token_gen')
  def espImgFlex = espFlexDockerImage()
  def rcTestVersion = getUniqueID('', false)
  def skipCleanup = getSkipCleanup() ? "-k" : ""
  def logBucket = "gs://${BUCKET}/${GIT_SHA}/logs"
  def durationHour = getDurationHour()
  def endpointsFlag = endpoints ? "-e " : ""
  def flexFlag = flex ? "-f " : ""

  sh "script/linux-test-vm-bookstore " +
      "${endpointsFlag}" +
      "${flexFlag}" +
      "-v ${rcTestVersion} " +
      "-i ${espImgFlex} " +
      "-l ${durationHour} " +
      "-b ${logBucket} " +
      "${skipCleanup}"
}

/*
Build Parameters
 */

def getSkipCleanup() {
  // Using a parameterized build with SKIP_CLEANUP env variable
  try {
    return "${SKIP_CLEANUP}" == "true"
  } catch (MissingPropertyException e) {
    return false
  }
}

def getGitCommit() {
  // Using a parameterized build with GIT_COMMIT env variable
  def gitCommit = 'HEAD'
  try {
    gitCommit = "${GIT_COMMIT}"
    if (gitCommit == 'INVALID') {
      failBranch('You must specify a valid GIT_COMMIT.')
    }
  } catch (MissingPropertyException e) {
  }
  return gitCommit
}

def getServiceManagementUrl() {
  // Using a parameterized build with SERVICE_MANAGEMENT_URL env variable
  def url = ''
  try {
    url = "${SERVICE_MANAGEMENT_URL}"
  } catch (MissingPropertyException e) {
  }
  return url
}

def getServerConfig() {
  // Using a parameterized build with SERVER_CONFIG env variable
  try {
    return "${SERVER_CONFIG}"
  } catch (MissingPropertyException e) {
    return '';
  }
}

def getDebianPackageRepo() {
  // Using a parameterized build with DEBIAN_PACKAGE_REPO env variable
  def debianPackageRepo = ''
  try {
    debianPackageRepo = "${DEBIAN_PACKAGE_REPO}"
    if (debianPackageRepo == 'INVALID') {
      failBranch('You must specify a valid DEBIAN_PACKAGE_REPO.')
    }
  } catch (MissingPropertyException e) {
  }
  return debianPackageRepo
}

def getDurationHour() {
  // Using a parameterized build with DURATION_HOUR env variable
  def durationHour = 0
  try {
    durationHour = "${DURATION_HOUR}".toInteger()
  } catch (MissingPropertyException e) {
  }
  return durationHour
}

def getUseTestSlave() {
  try {
    return "${USE_TEST_SLAVE}" == 'true'
  } catch (MissingPropertyException e) {
    return false
  }
}

def getReleaseQualification() {
  // Using a parameterized build with RELEASE_QUAL env variable
  try {
    return "${RELEASE_QUAL}" == 'true'
  } catch (MissingPropertyException e) {
    return false
  }
}

def getStage() {
  // Using a parameterized build with STAGE env variable
  def defaultStage = ALL_STAGES
  try {
    def stage = "${STAGE}".toString().trim()
    if (stage != '') return stage
  } catch (MissingPropertyException e) {

  }
  return defaultStage
}

def getE2eFilters() {
  // Using a parameterized build with E2E_FILTERS env variable
  def defaultFilters = '.*'
  try {
    def filters = "${E2E_FILTERS}".toString().trim()
    if (filters != '') return filters
  } catch (MissingPropertyException e) {
  }
  return defaultFilters
}

/*
Convenience methods
 */

def runStage(stage) {
  def stageToRun = getStage()
  assert stageToRun in SUPPORTED_STAGES,
      "Stage ${stageToRun} is not supported."
  if (stageToRun == ALL_STAGES) {
    // Stage starting with '_' should be called explicitly
    if (stage.startsWith('_')) {
      return false
    }
    return true
  }
  return stageToRun == stage
}

def sendFailureNotification() {
  mail to: 'esp-alerts-jenkins@google.com',
      subject: "Job '${env.JOB_NAME}' (${env.BUILD_NUMBER}) failed",
      body: "Please go to ${env.BUILD_URL} to investigate",
      from: 'ESP Jenkins Alerts <esp-alerts-jenkins@google.com>',
      replyTo: 'esp-alerts-jenkins@google.com'
}

def getTestSlaveLabel(label) {
  return "${label}-test"
}

def getSlaveLabel(label) {
  if (getUseTestSlave()) {
    return getTestSlaveLabel(label)
  }
  return label
}

def createServerConfigTag() {
  def serverConfig = getServerConfig()
  if (serverConfig != '') {
    def f = new File(serverConfig)
    return f.name.tokenize('.').first()
  }
  return ''
}

def failBranch(errorMessage) {
  echo errorMessage
  error errorMessage
}

def getUniqueID(testId, useSha) {
  // This is used also for Kubernetes namespace and should not > 62 chars.
  def date = new Date().format("yyMMddHHmm").toString()
  def prefix = isReleaseQualification() ? 'rc-test' : 'test'
  def uuid = randomUUID().toString()
  def sha = useSha ? "${GIT_SHA.take(7)}-" : ''
  def identifier = testId != '' ? "${testId}-" : ''
  return "${prefix}-${sha}${identifier}${uuid.take(4)}-${date}"
}

def generateServiceName(uniqueID, servicePrefix = '') {
  //TODO: Use uniqueID when it becomes possible.
  def serviceUrl = getServiceManagementUrl()
  if (serviceUrl != '') {
    servicePrefix = 'staging-${servicePrefix}'
  }
  return "${servicePrefix}testing-dot-${PROJECT_ID}.appspot.com"
}

def filterBranches(branches, regex) {
  filtered_branch = [:]
  for (branch in branches) {
    if (branch.key ==~ regex) {
      filtered_branch[branch.key] = branch.value
    }
  }
  return filtered_branch
}

/*
Git Helper Methods
*/

// Stashing source code to make sure that all branches uses the same version.
// See JENKINS-35245 bug for more info.
def stashSourceCode() {
  initialize(true)
  // Setting source code related global variable once so it can be reused.
  GIT_SHA = getRevision()
  ESP_RUNTIME_VERSION = getEndpointsRuntimeVersion()
  echo 'Stashing source code'
  fastStash('src-code', '.')
}

def checkoutSourceCode() {
  deleteDir()
  echo 'Unstashing source code'
  fastUnstash('src-code')
  sh "git diff"
}

def pathExistsCloudStorage(filePath) {
  sh "echo '0' > STATUS; gsutil stat ${filePath} || echo \$? > STATUS"
  def status = readFile 'STATUS'
  return status.toInteger() == 0
}


def stashArchivePath(name) {
  return "gs://${BUCKET}/${GIT_SHA}/tmp/${name}.tar.gz"
}

def fastStash(name, stashPath) {
  // Checking if archive already exists
  def archivePath = stashArchivePath(name)
  if (!pathExistsCloudStorage(archivePath)) {
    echo "Stashing ${stashPath} to ${archivePath}"
    retry(5) {
      sh "tar czf - ${stashPath} | gsutil " +
          "-h Content-Type:application/x-gtar cp - ${archivePath}"
      sleep 5
    }
  }
}

def fastUnstash(name) {
  def archivePath = stashArchivePath(name)
  retry(5) {
    sh "gsutil cp ${archivePath} - | tar zxf - "
    sleep 5
  }
}

def getRevision() {
  // Code needs to be checked out for this.
  sh 'git rev-parse --verify HEAD > GIT_COMMIT'
  return readFile('GIT_COMMIT').trim()
}

def setArtifactsLink() {
  def url = "https://console.cloud.google.com/storage/browser/${BUCKET}/${GIT_SHA}"
  def html = """
<!DOCTYPE HTML>
Find <a href='${url}'>artifacts</a> here
"""
  def artifactsHtml = 'artifacts.html'
  writeFile file: artifactsHtml, text: html
  archive artifactsHtml
}

def initialize(setup = false, authDaemon = true) {
  setGit()
  if (authDaemon) {
    retry(10) {
      // Timeout after 1 minute
      timeout(1) {
        setGitAuthDaemon()
      }
      sleep 5
    }
  }
  retry(10) {
    // Timeout after 5 minute
    timeout(5) {
      checkout scm
    }
    sleep 5
  }
  def gitCommit = getGitCommit()
  if (gitCommit != 'HEAD') {
    sh "git fetch origin ${gitCommit} && git checkout -f ${gitCommit}"

  }
  // Updating submodules and cleaning files.
  if (setup) {
    sh 'script/setup && script/obliterate'
  }
}

def setGCloud() {
  retry(5) {
    timeout(1) {
      sh "gcloud config set compute/zone ${ZONE}"
      sh "gcloud container clusters get-credentials ${CLUSTER}"
    }
    sleep 5
  }
}

def setGitAuthDaemon() {
  sh '''#!/bin/bash
echo "Installing Git Auth Daemon."
rm -rf ./gcompute-tools .git-credential-cache/cookie
git clone https://gerrit.googlesource.com/gcompute-tools
echo "Authenticating to googlesource.com."
AUTH_DAEMON=0
gcompute-tools/git-cookie-authdaemon --nofork & AUTH_DAEMON=$!
echo "Waiting on authentication to googlesource: PID=${AUTH_DAEMON}."
sleep 5
[[ -s ${HOME}/.git-credential-cache/cookie ]] || \
  { echo 'Failed to authenticate on google'; exit 1; }
trap \'kill %gcompute-tools/git-cookie-authdaemon\' EXIT'''
}

def setGit() {
}
