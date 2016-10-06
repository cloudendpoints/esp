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


// Supported Stages.
// ALL will run all stage but the one starting with '_'.
// By default all ALL stages will run.
ALL_STAGES = 'ALL'
E2E_STAGE = 'E2E'
PERFORMANCE_STAGE = 'PERFORMANCE'
PRESUBMIT = 'PRESUBMIT'
// If stage starts with '_' it should be called explicitly.
// To do so, add a String Build Parameter in your project and call it 'STAGE'
// Set that build parameter to the stage that you want to start.
CLEANUP_STAGE = '_CLEANUP'
SLAVE_UPDATE_STAGE = '_SLAVE_UPDATE'

SUPPORTED_STAGES = [
    ALL_STAGES,
    E2E_STAGE,
    CLEANUP_STAGE,
    PERFORMANCE_STAGE,
    PRESUBMIT,
    SLAVE_UPDATE_STAGE,
]

// Supported VM Images
DEBIAN_JESSIE = 'debian-8'
CONTAINER_VM = 'container-vm'

// Slaves Docker tags
DOCKER_SLAVES = [
    (DEBIAN_JESSIE): 'gcr.io/endpoints-jenkins/debian-8-slave',
]

// Release Qualification end to end tests.
// If RAPTURE_REPO build parameter is set only those test will run.
// We can filter down the number of tests by using the E2E_FILTERS build
// parameter.
// Please Update script/validate_release.py when adding or removing long-run-test.
RELEASE_QUALIFICATION_BRANCHES = [
    'flex-off-endpoints-on',
    'gce-debian-8',
    'gke-tight-https',
    'gke-tight-http2-echo',
    'gke-tight-http2-interop',
]

// Source Code related variables. Set in stashSourceCode.
GIT_SHA = ''
ESP_RUNTIME_VERSION = ''
// Global Variables defined in Jenkins
BUCKET = ''
BAZEL_ARGS = ''
CLUSTER = ''
PROJECT_ID = ''
ZONE = ''

node {
  BUCKET = failIfNullOrEmpty(env.BUCKET, 'BUCKET must be set.')
  BAZEL_ARGS = getWithDefault(env.BAZEL_ARGS)
  CLUSTER = failIfNullOrEmpty(env.GKE_CLUSTER, 'GKE_CLUSTER must be set')
  PROJECT_ID = failIfNullOrEmpty(env.PROJECT_ID, 'PROJECT_ID must be set')
  ZONE = failIfNullOrEmpty(env.ZONE, 'ZONE must be set')
  stashSourceCode()
  setArtifactsLink()
}

node('master') {
  def nodeLabel = getSlaveLabel(DEBIAN_JESSIE)
  def buildNodeLabel = getBuildSlaveLabel(DEBIAN_JESSIE)
  def builtArtifacts = false
  try {
    if (runStage(CLEANUP_STAGE)) {
      stage('Test Cleanup') {
        cleanupOldTests(nodeLabel)
      }
    }
    if (runStage(SLAVE_UPDATE_STAGE)) {
      stage('Slave Update') {
        node(nodeLabel) {
          buildNewDockerSlave(nodeLabel)
        }
      }
    }
    if (runStage(PRESUBMIT)) {
      stage('Unit / Integration Tests') {
        ws {
          def success = true
          checkoutSourceCode()
          updateGerrit('run')
          try {
            presubmit(buildNodeLabel)
          } catch (Exception e) {
            success = false
            throw e
          } finally {
            updateGerrit('verify', success)
          }
        }
      }
    }
    if (runStage(E2E_STAGE)) {
      stage('Build Artifacts') {
        buildArtifacts(buildNodeLabel)
        builtArtifacts = true
      }
      stage('E2E Tests') {
        e2eTest(nodeLabel)
      }
    }
    if (runStage(PERFORMANCE_STAGE)) {
      if (!builtArtifacts) {
        stage('Build Artifacts') {
          buildArtifacts(buildNodeLabel, false, false)
        }
      }
      stage('Performance Test') {
        performance(nodeLabel)
      }
    }
    if (runStage(ALL_STAGES)) {
      // If all stages passed, queue up a release qualification.
      build(
          job: 'esp/esp-release-qual',
          parameters: [[$class: 'StringParameterValue', name: 'GIT_COMMIT', value: GIT_SHA],
                       [$class: 'StringParameterValue', name: 'DURATION_HOUR', value: '10'],
                       [$class: 'StringParameterValue', name: 'STAGE', value: 'E2E'],
                       [$class: 'BooleanParameterValue', name: 'RELEASE_QUAL', value: true]],
          wait: false)
    }
  } catch (Exception e) {
    currentBuild.result = 'FAILURE'
    throw e
  } finally {
    step([
        $class: 'Mailer',
        notifyEveryUnstableBuild: false,
        recipients: '',
        sendToIndividuals: true])
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
  parallel(branches)
}

def buildArtifacts(nodeLabel, buildBookstore = true, buildGrpcTest = true) {
  def branches = [
      'packages': {
        node(nodeLabel) {
          buildPackages()
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
  parallel(branches)
}

def presubmit(nodeLabel) {
  def branches = [
      'asan': {
        node(nodeLabel) {
          presubmitTests('asan')
        }
      },
      'build-and-test': {
        node(nodeLabel) {
          presubmitTests('service-control')
          presubmitTests('build-and-test', false)
        }
      },
      'release': {
        node(nodeLabel) {
          presubmitTests('release')
          presubmitTests('docker-tests', false)
        }
      },
      'tsan': {
        node(nodeLabel) {
          presubmitTests('tsan')
        }
      },
  ]
  // Do validation and
  node(nodeLabel) {
    presubmitTests('check-files')
  }
  parallel(branches)
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
  parallel(branches)
}

def e2eTest(nodeLabel) {
  // Please Update script/validate_release.py when adding or removing test.
  // Storing as [key, value] as Jenkins groovy cannot iterate over maps :(.
  def branches = [
      ['gce-debian-8', {
        node(nodeLabel) {
          e2eGCE(DEBIAN_JESSIE)
        }
      }],
      ['gke-tight-http', {
        node(nodeLabel) {
          e2eGKE('tight', 'http')
        }
      }],
      ['gke-loose-http', {
        node(nodeLabel) {
          e2eGKE('loose', 'http')
        }
      }],
      ['gke-custom-http', {
        node(nodeLabel) {
          e2eGKE('custom', 'http')
        }
      }],
      ['gke-tight-https', {
        node(nodeLabel) {
          e2eGKE('tight', 'https')
        }
      }],
      ['gke-loose-https', {
        node(nodeLabel) {
          e2eGKE('loose', 'https')
        }
      }],
      ['flex-off-endpoints-on', {
        node(nodeLabel) {
          e2eFlex(true, false)
        }
      }],
      ['flex-off-endpoints-off', {
        node(nodeLabel) {
          e2eFlex(false, false)
        }
      }],
      ['gke-tight-http2-echo', {
        node(nodeLabel) {
          e2eGKE('tight', 'http2', 'echo')
        }
      }],
      ['gke-tight-http2-interop', {
        node(nodeLabel) {
          e2eGKE('tight', 'http2', 'interop')
        }
      }],
  ]

  branches = filterBranches(branches, getParam('E2E_FILTERS', '.*'))
  if (isReleaseQualification()) {
    branches = filterBranches(branches, RELEASE_QUALIFICATION_BRANCHES.join('|'))
  }
  branches = convertToMap(branches)

  withEnv([
      "LANG=C.UTF-8",
      "CLOUDSDK_API_ENDPOINT_OVERRIDES_SERVICEMANAGEMENT=${getParam('SERVICE_MANAGEMENT_URL')}",
      "CLOUDSDK_COMPONENT_MANAGER_SNAPSHOT_URL=${getParam('GCLOUD_URL')}"]) {
    parallel(branches)
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
  if (getParam('SERVICE_MANAGEMENT_URL') != '') {
    suffix = "${suffix}-staging"
  }
  return "gcr.io/${PROJECT_ID}/endpoints-runtime${suffix}:debian-git-${GIT_SHA}"
}

def espDockerImage(suffix = '') {
  if (isRelease()) {
    if (getParam('USE_LATEST_RELEASE', false)) {
      return "b.gcr.io/endpoints/endpoints-runtime${suffix}:latest"
    }
    return "b.gcr.io/endpoints/endpoints-runtime${suffix}:${ESP_RUNTIME_VERSION}"
  }
  return espGenericDockerImage(suffix)
}

def espFlexDockerImage() {
  if (isRelease()) {
    return 'does_not_exist'
  }
  return espGenericDockerImage('-flex')
}

def isRelease() {
  return getDebianPackageRepo() != '' || getParam('USE_LATEST_RELEASE', false)
}

def isReleaseQualification() {
  if (isRelease()) {
    return true
  }
  return getParam('RELEASE_QUAL', false)
}

def isDefaultCleanNginxBinary() {
  return (getParam('SERVICE_MANAGEMENT_URL') == '' && getParam('SERVER_CONFIG') == '')
}

def bookstoreDockerImage() {
  return "gcr.io/${PROJECT_ID}/bookstore:${GIT_SHA}"
}

def gRpcTestServerImage(grpc) {
  return "gcr.io/${PROJECT_ID}/grpc-${grpc}-server:${GIT_SHA}"
}

def espDebianPackage() {
  if (getParam('USE_LATEST_RELEASE', false)) {
    return ''
  }
  def suffix = ''
  if (getParam('SERVICE_MANAGEMENT_URL') != '') {
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
  def serverConfig = getParam('SERVER_CONFIG')
  def serverConfigFlag = ''
  if (serverConfig != '') {
    serverConfigFlag = "-c ${serverConfig}"
  }
  if (isRelease()) {
    // Release Docker and debian package should already be built.
    sh("docker pull ${espImgGeneric} || { echo 'Cannot find ${espImgGeneric}'; exit 1; }")
  } else {
    def espDebianPackage = espDebianPackage()
    def espImgFlex = espFlexDockerImage()

    sh("script/robot-release " +
        "-m ${espImgFlex} " +
        "-g ${espImgGeneric} " +
        "-d ${espDebianPackage} " +
        "${serverConfigFlag} -s")
    // local perf builds its own esp binary package.
    // Using the one built here instead if it exists.
    if (isDefaultCleanNginxBinary()) {
      // We only want to stash for the clean default binary.
      if (fileExists('bazel-bin/src/nginx/main/nginx-esp')) {
        fastStash('nginx-esp', 'bazel-bin/src/nginx/main/nginx-esp')
      }
    }
  }
  // Building tools
  def tools = [
      '//src/tools:auth_token_gen',
      '//test/src:espcli',
      '//test/grpc:grpc-test-client',
      '//test/grpc:interop-client',
      '//test/grpc:interop-metrics-client',
      '//test/grpc:interop-server',
      '//test/grpc:interop-stress-client',
  ]
  def stashPaths = [
      'bazel-bin/src/tools/auth_token_gen',
      'bazel-bin/test/src/espcli',
      'bazel-bin/test/grpc/grpc-test-client',
      'bazel-bin/test/grpc/interop-client',
      'bazel-bin/test/grpc/interop-metrics-client',
      'bazel-bin/test/grpc/interop-server',
      'bazel-bin/test/grpc/interop-stress-client',
  ]
  buildAndStash(
      tools.join(' '),
      stashPaths.join(' '),
      'tools')
}

def buildBookstoreImage() {
  setGCloud()
  checkoutSourceCode()
  def bookstoreImg = bookstoreDockerImage()
  sh("test/bookstore/linux-build-bookstore-docker -i ${bookstoreImg}")
}

def buildGrcpTest() {
  def gRpcEchoServerImg = gRpcTestServerImage('echo')
  def gRpcInteropServerImg = gRpcTestServerImage('interop')
  setGCloud()
  checkoutSourceCode()
  sh("test/grpc/linux-build-grpc-docker -i ${gRpcEchoServerImg}")
  sh("test/grpc/linux-build-grpc-docker -o -i ${gRpcInteropServerImg}")
}

def buildAndStash(buildTarget, stashTarget, name) {
  if (pathExistsCloudStorage(stashArchivePath(name))) return
  setGCloud()
  checkoutSourceCode()
  // Turns out Bazel does not like to be terminated.
  // Timing out after 40 minutes.
  timeout(40) {
    retry(3) {
      sh("bazel build ${BAZEL_ARGS} --config=release ${buildTarget}")
      sleep(5)
    }
  }
  fastStash(name, stashTarget)
}

def testCleanup(daysOld, project, flags) {
  setGCloud()
  checkoutSourceCode()
  sh("script/jenkins-tests-cleanup.sh -d ${daysOld} -p ${project} -f ${flags}")
}

// flow can be run or verify
def updateGerrit(flow, success = false) {
  def gerritUrl = getParam('GERRIT_URL')
  if (gerritUrl == '') {
    return
  }
  def successFlag = success ? '--success' : ''
  retry(3) {
    sh("script/update-gerrit.py " +
        "--build_url=\"${env.BUILD_URL}\" " +
        "--change_id=\"${CHANGE_ID}\" " +
        "--gerrit_url=\"${gerritUrl}\" " +
        "--commit=\"${GIT_SHA}\" " +
        "--flow=\"${flow}\" " +
        "${successFlag}")
    sleep(5)
  }

}

def buildNewDockerSlave(nodeLabel) {
  setGCloud()
  checkoutSourceCode()
  def dockerImage = "${DOCKER_SLAVES[nodeLabel]}:${GIT_SHA}"
  // Test Slave image setup in Jenkins
  def testDockerImage = "${DOCKER_SLAVES[nodeLabel]}:test"
  // Slave image setup in Jenkins
  def finalDockerImage = "${DOCKER_SLAVES[nodeLabel]}:latest"
  echo("Building ${testDockerImage}")
  sh("script/jenkins-build-docker-slave -b " +
      "-i ${dockerImage} " +
      "-t ${testDockerImage} " +
      "-s ${nodeLabel}")
  echo("Testing ${testDockerImage}")
  node(getTestSlaveLabel(nodeLabel)) {
    setGCloud()
    checkoutSourceCode()
    sh('jenkins/slaves/slave-test')
  }
  echo("Retagging ${testDockerImage} to ${dockerImage}")
  sh("script/jenkins-build-docker-slave " +
      "-i ${testDockerImage} " +
      "-t ${finalDockerImage}")
}

def e2eCommonOptions(testId, prefix = '') {
  def uniqueID = getUniqueID(testId, true)
  def skipCleanup = getParam('SKIP_CLEANUP', false) ? "-s" : ""
  def serviceName = generateServiceName(uniqueID, prefix)
  def durationHour = getParam('DURATION_HOUR', 0)
  return "-a ${serviceName} " +
      "-B ${BUCKET} " +
      "-i ${uniqueID} " +
      "-l ${durationHour} " +
      "${skipCleanup} "
}

// grpc is a string; one of
//  'off': not use grpc
//  'echo': run grpc echo pass_through and transcoding tests
//  'interop': run grpc interop pass_through test.
def e2eGKE(coupling, proto, grpc = 'off') {
  setGCloud()
  checkoutSourceCode()
  fastUnstash('tools')
  def uniqueID = getUniqueID("gke-${coupling}-${proto}", true)
  def serviceName = generateServiceName(uniqueID)
  def backendDockerImage = bookstoreDockerImage()
  if (grpc != 'off') {
    serviceName = "grpc-${grpc}-dot-${PROJECT_ID}.appspot.com"
    backendDockerImage = gRpcTestServerImage(grpc)
  }
  sh("script/e2e-kube.sh " +
      " -b ${backendDockerImage}" +
      " -g ${grpc}" +
      " -e " + espDockerImage() +
      " -t ${proto}" +
      " -c ${coupling}" +
      " -a ${serviceName}" +
      " -B ${BUCKET} " +
      " -i ${uniqueID} " +
      " -l " + getParam('DURATION_HOUR', 0) +
      (getParam('SKIP_CLEANUP', false) ? " -s" : ""))
}

def e2eGCE(vmImage) {
  setGCloud()
  checkoutSourceCode()
  fastUnstash('tools')
  def commonOptions = e2eCommonOptions('gce-raw')
  def espDebianPkg = espDebianPackage()
  def debianPackageRepo = getDebianPackageRepo()
  echo('Running GCE test')
  sh("test/bookstore/gce/e2e.sh " +
      commonOptions +
      "-V ${ESP_RUNTIME_VERSION} " +
      "-v ${vmImage} " +
      "-d \"${espDebianPkg}\" " +
      "-r \"${debianPackageRepo}\"")
}

def localPerformanceTest() {
  // This will not work for staging or server-config.
  // In order to fully support this we'll need to use the debian
  // packages created in buildPackages().
  setGCloud()
  checkoutSourceCode()
  if (pathExistsCloudStorage(stashArchivePath('nginx-esp'))) {
    // Using binary build by buildArtifacts()
    fastUnstash('tools')
    fastUnstash('nginx-esp')
  }
  def testId = 'jenkins-post-submit-perf-test'
  def uniqueId = getUniqueID('local-perf', false)
  def logBucket = "gs://${BUCKET}/${GIT_SHA}/logs"
  def testEnv = sh(
      returnStdout: true,
      script: "script/create-test-env-json " +
          "-t ${testId} " +
          "-i ${uniqueId}").trim()
  sh('script/linux-prep-machine')
  sh("script/linux-start-local-test " +
      "-t ${testEnv} " +
      "-b ${logBucket}")
}

def flexPerformance() {
  setGCloud()
  checkoutSourceCode()
  fastUnstash('tools')
  def testId = 'jenkins-perf-test-vm-esp'
  def uniqueId = getUniqueID('flex-perf', false)
  def testEnv = sh(
      returnStdout: true,
      script: "script/create-test-env-json " +
          "-t ${testId} " +
          "-i ${uniqueId}").trim()
  def logBucket = "gs://${BUCKET}/${GIT_SHA}/logs"
  def espImgFlex = espFlexDockerImage()
  sh("script/linux-test-vm-echo " +
      "-i ${espImgFlex} " +
      "-t ${testEnv} " +
      "-b ${logBucket}")
}

def e2eFlex(endpoints, flex) {
  setGCloud()
  checkoutSourceCode()
  fastUnstash('tools')
  def espImgFlex = espFlexDockerImage()
  def rcTestVersion = getUniqueID('', false)
  def skipCleanup = getParam('SKIP_CLEANUP', false) ? '-k' : ''
  def logBucket = "gs://${BUCKET}/${GIT_SHA}/logs"
  def durationHour = getParam('DURATION_HOUR', 0)
  def endpointsFlag = endpoints ? '-e ' : ''
  def flexFlag = flex ? '-f ' : ''
  def useLatestVersion = getParam('USE_LATEST_RELEASE', false) ? '-L ' : ''

  sh("script/linux-test-vm-bookstore " +
      "${endpointsFlag}" +
      "${flexFlag}" +
      "-v ${rcTestVersion} " +
      "-i ${espImgFlex} " +
      "-l ${durationHour} " +
      "-b ${logBucket} " +
      "${useLatestVersion} " +
      "${skipCleanup}")
}

def presubmitTests(scenario, checkoutCode = true) {
  if (checkoutCode) {
    setGCloud()
    checkoutSourceCode()
  }
  def logBucket = "gs://${BUCKET}/${GIT_SHA}/logs"
  def uniqueId = getUniqueID(scenario, true)
  timeout(time: 1, unit: 'HOURS') {
    sh("script/run-presubmit " +
        "-b ${logBucket} " +
        "-s ${scenario} " +
        "-r ${uniqueId}")
  }
}

/*
Build Parameters
 */

def failIfNullOrEmpty(value, message) {
  if (value == null || value == '') {
    error(message)
  }
  return value
}

def getWithDefault(value, defaultValue = '') {
  if (value == null || value == '') {
    return defaultValue
  }
  return value
}

def getParam(name, defaultValue = '') {
  return getWithDefault(params.get(name), defaultValue)
}

def getGitCommit() {
  // Using a parameterized build with GIT_COMMIT env variable
  def gitCommit = getParam('GIT_COMMIT', 'HEAD')
  if (gitCommit == 'INVALID') {
    failBranch('You must specify a valid GIT_COMMIT.')
  }
  return gitCommit
}

def getDebianPackageRepo() {
  // Using a parameterized build with RAPTURE_REPO env variable
  debianPackageRepo = getParam('RAPTURE_REPO')
  if (debianPackageRepo == 'INVALID') {
    failBranch('You must specify a valid RAPTURE_REPO.')
  }
  return debianPackageRepo
}

/*
Convenience methods
 */

def runStage(stage) {
  def stageToRun = getParam('STAGE', ALL_STAGES)
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
  if (getParam('USE_LATEST_RELEASE', false)) {
    return getTestSlaveLabel(label)
  }
  return label
}

def getBuildSlaveLabel(label) {
  return "${label}-build"
}

def createServerConfigTag() {
  def serverConfig = getParam('SERVER_CONFIG')
  if (serverConfig != '') {
    def f = new File(serverConfig)
    return f.name.tokenize('.').first()
  }
  return ''
}

def failBranch(errorMessage) {
  echo(errorMessage)
  error(errorMessage)
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
  def serviceUrl = getParam('SERVICE_MANAGEMENT_URL')
  if (serviceUrl != '') {
    servicePrefix = 'staging-${servicePrefix}'
  }
  if (getParam('USE_LATEST_RELEASE', false) || getParam('GCLOUD_URL')) {
    return "${uniqueID}-dot-${PROJECT_ID}.appspot.com"
  }
  return "${servicePrefix}testing-dot-${PROJECT_ID}.appspot.com"
}

// Converts a list of [key, value] to a map
def convertToMap(list) {
  def map = [:]
  for (int i = 0; i < list.size(); i++) {
    def key = list.get(i).get(0)
    def value = list.get(i).get(1)
    map[key] = value
  }
  return map
}

def filterBranches(branches, regex) {
  def filteredBranches = []
  for (int i = 0; i < branches.size(); i++) {
    def keyValue = branches.get(i)
    def key = keyValue.get(0)
    if (key ==~ regex) {
      filteredBranches.add(keyValue)
    }
  }
  return filteredBranches
}

/*
Git Helper Methods
*/

// Stashing source code to make sure that all branches uses the same version.
// See JENKINS-35245 bug for more info.
def stashSourceCode() {
  initialize(true)
  // Setting source code related global variable once so it can be reused.
  GIT_SHA = failIfNullOrEmpty(getRevision(), 'GIT_SHA must be set')
  ESP_RUNTIME_VERSION = failIfNullOrEmpty(getEndpointsRuntimeVersion(), 'ESP_RUNTIME_VERSION must be set')
  CHANGE_ID = failIfNullOrEmpty(getChangeId(), 'CHANGE_ID must be set')
  echo('Stashing source code')
  fastStash('src-code', '.')
}

def checkoutSourceCode() {
  deleteDir()
  echo('Unstashing source code')
  fastUnstash('src-code')
  sh("git diff")
}

def pathExistsCloudStorage(filePath) {
  def status = sh(returnStatus: true, script: "gsutil stat ${filePath}")
  return status == 0
}


def stashArchivePath(name) {
  return "gs://${BUCKET}/${GIT_SHA}/tmp/${name}.tar.gz"
}

def fastStash(name, stashPaths) {
  // Checking if archive already exists
  def archivePath = stashArchivePath(name)
  if (!pathExistsCloudStorage(archivePath)) {
    echo("Stashing ${stashPaths} to ${archivePath}")
    retry(5) {
      sh("tar czf - ${stashPaths} | gsutil " +
          "-h Content-Type:application/x-gtar cp - ${archivePath}")
      sleep(5)
    }
  }
}

def fastUnstash(name) {
  def archivePath = stashArchivePath(name)
  retry(5) {
    sh("gsutil cp ${archivePath} - | tar zxf - ")
    sleep(5)
  }
}

def getRevision() {
  // Code needs to be checked out for this.
  return sh(returnStdout: true, script: 'git rev-parse --verify HEAD').trim()
}

def getChangeId() {
  // Code needs to be checked out for this.
  return sh(
      returnStdout: true,
      script: "git log --format=%B -n 1 HEAD | awk '/^Change-Id: / {print \$2}'").trim()
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
      sleep(5)
    }
  }
  retry(10) {
    // Timeout after 5 minute
    timeout(5) {
      checkout(scm)
    }
    sleep(5)
  }
  def gitCommit = getGitCommit()
  if (gitCommit != 'HEAD') {
    sh("git fetch origin ${gitCommit} && git checkout -f ${gitCommit}")

  }
  // Updating submodules and cleaning files.
  if (setup) {
    sh('script/setup && script/obliterate')
  }
}

def setGCloud() {
  retry(5) {
    timeout(1) {
      sh("gcloud config set compute/zone ${ZONE}")
      sh("gcloud container clusters get-credentials ${CLUSTER}")
      if (getParam('GCLOUD_URL') != '') {
        sh('sudo gcloud components update -q')
      }
    }
    sleep(5)
  }
}

def setGitAuthDaemon() {
  sh('''#!/bin/bash
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
trap \'kill %gcompute-tools/git-cookie-authdaemon\' EXIT''')
}

def setGit() {
}
