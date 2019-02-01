#!groovy
/*
Copyright (C) Extensible Service Proxy Authors
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
SLAVE_IMAGE = 'gcr.io/endpoints-jenkins/debian-8:0.7'

// Release Qualification end to end tests.
// If RAPTURE_REPO build parameter is set only those test will run.
// We can filter down the number of tests by using the E2E_FILTERS build
// parameter.
// Please Update script/validate_release.py when adding or removing long-run-test.
RELEASE_QUALIFICATION_BRANCHES = [
    'flex',
    'gke-tight-https',
    'gke-tight-http2-echo',
    'gke-tight-http2-interop',
]

// Source Code related variables. Set in stashSourceCode.
GIT_SHA = ''
ESP_RUNTIME_VERSION = ''
// Global Variables defined in Jenkins
BUCKET = ''
BAZEL_ARGS = '--action_env=PERL5LIB=.'
BAZEL_BUILD_ARGS = ''
CLUSTER = ''
PROJECT_ID = ''
TOOLS_BUCKET = ''
ZONE = ''

DefaultNode {
  BUCKET = failIfNullOrEmpty(env.BUCKET, 'BUCKET must be set.')
  BAZEL_ARGS = getWithDefault(env.BAZEL_ARGS)
  BAZEL_BUILD_ARGS = getWithDefault(env.BAZEL_BUILD_ARGS)
  CLUSTER = failIfNullOrEmpty(env.GKE_CLUSTER, 'GKE_CLUSTER must be set')
  PROJECT_ID = failIfNullOrEmpty(env.PROJECT_ID, 'PROJECT_ID must be set')
  TOOLS_BUCKET = failIfNullOrEmpty(env.TOOLS_BUCKET, 'TOOLS_BUCKET must be set')
  ZONE = failIfNullOrEmpty(env.ZONE, 'ZONE must be set')
  stashSourceCode()
  setArtifactsLink()
}

node('master') {
  def builtArtifacts = false
  try {
    if (runStage(CLEANUP_STAGE)) {
      stage('Test Cleanup') {
        cleanupOldTests()
      }
    }
    if (runStage(PRESUBMIT)) {
      stage('Unit / Integration Tests') {
        ws {
          def success = true
          updatePresubmit('run')
          try {
            presubmit()
          } catch (Exception e) {
            success = false
            throw e
          } finally {
            updatePresubmit('verify', success)
          }
        }
      }
    }
    if (runStage(E2E_STAGE)) {
      stage('Build Artifacts') {
        buildArtifacts()
        builtArtifacts = true
      }
      stage('E2E Tests') {
        e2eTest()
      }
    }
    if (runStage(PERFORMANCE_STAGE)) {
      if (!builtArtifacts) {
        stage('Build Artifacts') {
          buildArtifacts(false, false)
        }
      }
      stage('Performance Test') {
        performance()
      }
    }
    def releaseQualJob = getParam('RELEASE_QUAL_JOB')
    if (releaseQualJob != '') {
      // If all stages passed, queue up a release qualification.
      build(
          job: releaseQualJob,
          parameters: [[$class: 'StringParameterValue', name: 'BRANCH_SPEC', value: GIT_SHA],
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
        recipients: 'esp-alerts-jenkins@google.com',
        sendToIndividuals: true])
  }
}

def cleanupOldTests() {
  def branches = [
      'cleanup_endpoints-jenkins': {
        // Delete 6 days old namespaces and GCE instances on endpoints-jenkins.
        DefaultNode {
          testCleanup(6, PROJECT_ID, '-i -n')
        }
      },
      'cleanup_esp-load-test': {
        // Delete 2 days old Flex versions on esp-load-test.
        DefaultNode {
          testCleanup(2, 'esp-load-test', '-v')
        }
      },
      'cleanup_esp-long-run': {
        // Delete 7 days old Flex versions on esp-long-run.
        DefaultNode {
          testCleanup(7, 'esp-long-run', '-v')
        }
      }
  ]
  parallel(branches)
}

def buildArtifacts(buildBookstore = true, buildGrpcTest = true) {
  def branches = [
      'packages': {
        BuildNode {
          buildPackages()
        }
      }
  ]
  if (buildBookstore) {
    branches['bookstore'] = {
      BuildNode {
        buildBookstoreImage()
      }
    }
  }
  if (buildGrpcTest) {
    branches['grpc_test'] = {
      BuildNode {
        buildGrcpTest()
      }
    }
  }
  parallel(branches)
}

def presubmit() {
  def branches = [
      'asan': {
        BuildNode {
          presubmitTests('asan')           
        }
      },
      'build-and-test': {
        BuildNode {
          presubmitTests('build-and-test')
        }
      },
      'release': {
        BuildNode {
          presubmitTests('release')
          presubmitTests('docker-tests', false)
        }
      },
      'tsan': {
        BuildNode {
          //Temporarily disable the tsan presubmit tests
          //as a workaround before the Jenkins problem is resolved.
          //To-do: enable the tsan presubmit tests after 
          //the Jenkins problem is resolved.
          //presubmitTests('tsan')
        }
      },
  ]
  // Do validation and
  BuildNode {
    presubmitTests('check-files')
  }
  parallel(branches)
}


def performance() {
  def branches = [
      'jenkins-post-submit-perf-test': {
        DefaultNode {
          localPerformanceTest()
        }
      },
      'jenkins-perf-test-vm-esp': {
        DefaultNode {
          flexPerformance()
        }
      }
  ]
  parallel(branches)
}

def e2eTest() {
  // Please Update script/validate_release.py when adding or removing test.
  // Storing as [key, value] as Jenkins groovy cannot iterate over maps :(.
  // Please don't remove gke-custom-http test. It is important to test
  // custom nginx config.
  def branches = [
      ['gke-tight-http', {
        DefaultNode {
          e2eGKE('tight', 'http', 'fixed')
        }
      }],
      ['gke-tight-http-managed', {
        DefaultNode {
          e2eGKE('tight', 'http', 'managed')
        }
      }],
      ['gke-loose-http', {
        DefaultNode {
          e2eGKE('loose', 'http', 'fixed')
        }
      }],
      ['gke-custom-http', {
        DefaultNode {
          e2eGKE('custom', 'http', 'fixed')
        }
      }],
      ['gke-tight-https', {
        DefaultNode {
          e2eGKE('tight', 'https', 'fixed')
        }
      }],
      ['gke-loose-https', {
        DefaultNode {
          e2eGKE('loose', 'https', 'fixed')
        }
      }],
      ['flex', {
        DefaultNode {
          e2eFlex()
        }
      }],
      ['gke-tight-http2-echo', {
        DefaultNode {
          e2eGKE('tight', 'http2', 'fixed', 'echo')
        }
      }],
      ['gke-tight-http2-interop', {
        DefaultNode {
          e2eGKE('tight', 'http2', 'fixed', 'interop')
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

def espDockerImage() {
  if (isRelease()) {
    if (getParam('USE_LATEST_RELEASE', false)) {
      return "gcr.io/endpoints-release/endpoints-runtime:1"
    }
    return "gcr.io/endpoints-release/endpoints-runtime:${ESP_RUNTIME_VERSION}"
  }
  return espGenericDockerImage()
}

def espServerlessDockerImage() {
  if (isRelease()) {
    if (getParam('USE_LATEST_RELEASE', false)) {
      return "gcr.io/endpoints-release/endpoints-runtime-serverless:1"
    }
    return "gcr.io/endpoints-release/endpoints-runtime-serverless:${ESP_RUNTIME_VERSION}"
  }
  return espGenericDockerImage('-serverless')
}

def espFlexDockerImage() {
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

// One of bookstore, echo, interop
def backendImage(app) {
  return "gcr.io/${PROJECT_ID}/${app}:${GIT_SHA}"
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
  return readFile('src/nginx/version').trim()
}

/*
Stages
 */

def buildPackages() {
  setupNode()
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
    def espImgServerless = espServerlessDockerImage()

    sh("script/robot-release " +
        "-m ${espImgFlex} " +
        "-g ${espImgGeneric} " +
        "-h ${espImgServerless} " +
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
      '//test/grpc:grpc-test-client',
      '//test/grpc:interop-client',
      '@org_golang_google_grpc//stress/metrics_client',
      '@org_golang_google_grpc//interop/server',
      '@org_golang_google_grpc//stress/client',
      '//test/grpc:grpc-test_descriptor',
      '//test/grpc:grpc-interop_descriptor',
  ]
  def stashPaths = [
      'bazel-bin/src/tools/auth_token_gen',
      'bazel-bin/test/grpc/grpc-test-client',
      'bazel-bin/test/grpc/interop-client',
      'bazel-bin/external/org_golang_google_grpc/stress/metrics_client/metrics_client',
      'bazel-bin/external/org_golang_google_grpc/interop/server/server',
      'bazel-bin/external/org_golang_google_grpc/stress/client/client',
      'bazel-genfiles/test/grpc/grpc-test.descriptor',
      'bazel-genfiles/test/grpc/grpc-interop.descriptor',
  ]
  buildAndStash(
      tools.join(' '),
      stashPaths.join(' '),
      'tools')
}

def buildBookstoreImage() {
  setupNode()
  def bookstoreImg = backendImage('bookstore')
  sh("test/bookstore/linux-build-bookstore-docker -i ${bookstoreImg}")
}

def buildGrcpTest() {
  def gRpcEchoServerImg = backendImage('echo')
  def gRpcInteropServerImg = backendImage('interop')
  setupNode()
  sh("test/grpc/linux-build-grpc-docker -i ${gRpcEchoServerImg}")
  sh("test/grpc/linux-build-grpc-docker -o -i ${gRpcInteropServerImg}")
}

def buildAndStash(buildTarget, stashTarget, name) {
  if (pathExistsCloudStorage(stashArchivePath(name))) return
  setupNode()
  // Turns out Bazel does not like to be terminated.
  // Timing out after 40 minutes.
  timeout(40) {
    retry(3) {
      sh("bazel ${BAZEL_ARGS} build ${BAZEL_BUILD_ARGS} --config=release ${buildTarget}")
      sleep(5)
    }
  }
  fastStash(name, stashTarget)
}

def testCleanup(daysOld, project, flags) {
  setupNode()
  sh("script/jenkins-tests-cleanup.sh -d ${daysOld} -p ${project} -f ${flags}")
}

// flow can be run or verify
def updatePresubmit(flow, success = false) {
  if (getParam('STAGE') != PRESUBMIT || !getParam('UPDATE_PR', true)) return
  switch (flow) {
    case 'run':
      state = 'PENDING'
      message = "Running presubmits at ${env.BUILD_URL} ..."
      break
    case 'verify':
      state = success ? 'SUCCESS' : 'FAILURE'
      message = "${success ? 'Successful' : 'Failed'} presubmits. " +
          "Details at ${env.BUILD_URL}."
      break
    default:
      error('flow can only be run or verify')
  }
  setGitHubPullRequestStatus(context: env.JOB_NAME, message: message, state: state)
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

// backend is a string; one of
//  'bookstore': HTTP bookstore
//  'echo': run grpc echo pass_through and transcoding tests
//  'interop': run grpc interop pass_through test.
def e2eGKE(coupling, proto, rollout_strategy, backend = 'bookstore') {
  setupNode()
  fastUnstash('tools')
  def uniqueID = getUniqueID("gke-${coupling}-${proto}-${backend}", true)
  sh("script/e2e-kube.sh " +
      " -c ${coupling}" +
      " -t ${proto}" +
      " -g ${backend}" +
      " -b " + backendImage(backend) +
      " -e " + espDockerImage() +
      " -i ${uniqueID} " +
      " -a ${uniqueID}.${PROJECT_ID}.appspot.com" +
      " -B ${BUCKET} " +
      " -l " + getParam('DURATION_HOUR', 0) +
      " -R ${rollout_strategy} " +
      (getParam('SKIP_CLEANUP', false) ? " -s" : ""))
}

def e2eGCE(vmImage, rolloutStrategy) {
  setupNode()
  fastUnstash('tools')
  // Use different service names for tests with different strategy.
  def commonOptions = e2eCommonOptions('gce-raw', rolloutStrategy)
  def espDebianPkg = espDebianPackage()
  def debianPackageRepo = getDebianPackageRepo()
  echo('Running GCE test')
  sh("test/bookstore/gce/e2e.sh " +
      commonOptions +
      "-V ${ESP_RUNTIME_VERSION} " +
      "-v ${vmImage} " +
      "-R ${rolloutStrategy} " +
      "-d \"${espDebianPkg}\" " +
      "-r \"${debianPackageRepo}\"")
}

def localPerformanceTest() {
  // This will not work for staging or server-config.
  // In order to fully support this we'll need to use the debian
  // packages created in buildPackages().
  setupNode()
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
  sh("script/linux-start-local-test " +
      "-t ${testEnv} " +
      "-b ${logBucket}")
}

def flexPerformance() {
  setupNode()
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

def e2eFlex() {
  setupNode()
  fastUnstash('tools')
  def espImgFlex = espFlexDockerImage()
  def rcTestVersion = getUniqueID('', false)
  def skipCleanup = getParam('SKIP_CLEANUP', false) ? '-k' : ''
  def logBucket = "gs://${BUCKET}/${GIT_SHA}/logs"
  def durationHour = getParam('DURATION_HOUR', 0)
  def useLatestVersion = getParam('USE_LATEST_RELEASE', false) ? '-L ' : ''

  sh("script/linux-test-vm-bookstore " +
      "-v ${rcTestVersion} " +
      "-i ${espImgFlex} " +
      "-l ${durationHour} " +
      "-b ${logBucket} " +
      "${useLatestVersion} " +
      "${skipCleanup}")
}

def presubmitTests(scenario, checkoutCode = true) {
  if (checkoutCode) {
    setupNode()
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

def setupNode() {
  checkoutSourceCode()
  sh "script/jenkins-init-slaves.sh -z ${ZONE} -c ${CLUSTER}"
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
  initialize()
  // Testing new sub-modules.
  // This script creates new commit for each sub-module.
  // GIT_SHA will use the last commit.
  submodules_update = getParam('SUBMODULES_UPDATE')
  if (submodules_update != '') {
    sh("script/update-submodules -s ${submodules_update}")
  }
  // Setting source code related global variable once so it can be reused.
  GIT_SHA = failIfNullOrEmpty(getRevision(), 'GIT_SHA must be set')
  ESP_RUNTIME_VERSION = failIfNullOrEmpty(getEndpointsRuntimeVersion(), 'ESP_RUNTIME_VERSION must be set')
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

def initialize() {
  retry(10) {
    // Timeout after 5 minute
    timeout(5) {
      checkout(scm)
    }
    sleep(5)
  }
  // Updating submodules and cleaning files.
  sh('script/setup && script/obliterate')
}

def DefaultNode(Closure body) {
  podTemplate(label: 'debian-8-pod', cloud: 'kubernetes', containers: [
      containerTemplate(
          name: 'debian-8',
          image: SLAVE_IMAGE,
          args: 'cat',
          ttyEnabled: true,
          privileged: true,
          alwaysPullImage: false,
          workingDir: '/home/jenkins',
          resourceRequestCpu: '2000m',
          resourceLimitCpu: '8000m',
          resourceRequestMemory: '4Gi',
          resourceLimitMemory: '64Gi',
          envVars: [
              envVar(key: 'PLATFORM', value: 'debian-8')
          ])]) {
    node('debian-8-pod') {
      container('debian-8') {
        body()
      }
    }
  }
}

def BuildNode(Closure body) {
  podTemplate(label: 'debian-8-pod', cloud: 'kubernetes', containers: [
      containerTemplate(
          name: 'debian-8',
          image: SLAVE_IMAGE,
          args: 'cat',
          ttyEnabled: true,
          privileged: true,
          alwaysPullImage: false,
          workingDir: '/home/jenkins',
          resourceRequestCpu: '2000m',
          resourceLimitCpu: '8000m',
          resourceRequestMemory: '8Gi',
          resourceLimitMemory: '64Gi',
          envVars: [
              envVar(key: 'PLATFORM', value: 'debian-8')
          ])]) {
    node('debian-8-pod') {
      container('debian-8') {
        body()
      }
    }
  }
}
