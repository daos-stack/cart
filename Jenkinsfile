// To use a test branch (i.e. PR) until it lands to master
// I.e. for testing library changes
@Library(value="pipeline-lib@debug") _

def singleNodeTest() {
    provisionNodes NODELIST: env.NODELIST,
                   node_count: 1,
                   snapshot: true
    runTest stashes: [ 'CentOS-install', 'CentOS-build-vars' ],
            script: """pwd
                       ls -l
                       . ./.build_vars-Linux.sh
                       CART_BASE=\${SL_PREFIX%/install}
                       NODELIST=$env.NODELIST
                       NODE=\${NODELIST%%,*}
                       trap 'set +e; set -x; ssh \$NODE "set -ex; sudo umount \$CART_BASE"' EXIT
                       ssh \$NODE "set -x
                       set -e
                       sudo mkdir -p \$CART_BASE
                       sudo mount -t nfs \$HOSTNAME:\$PWD \$CART_BASE
                       if RUN_UTEST=false bash -x utils/run_test.sh; then
                           echo \"run_test.sh exited successfully with \${PIPESTATUS[0]}\"
                       else
                           echo \"trying again with LD_LIBRARY_PATH set\"
                           export LD_LIBRARY_PATH=\$SL_PREFIX/lib
                           if RUN_UTEST=false bash -x utils/run_test.sh; then
                               echo \"run_test.sh exited successfully with \${PIPESTATUS[0]}\"
                           else
                               rc=\${PIPESTATUS[0]}
                               echo \"run_test.sh exited failure with \$rc\"
                               exit \$rc
                           fi
                       fi"
                       """,
          junit_files: null
}

def arch="-Linux"

pipeline {
    agent any

    environment {
        GITHUB_USER = credentials('aa4ae90b-b992-4fb6-b33b-236a53a26f77')
        BAHTTPS_PROXY = "${env.HTTP_PROXY ? '--build-arg HTTP_PROXY="' + env.HTTP_PROXY + '" --build-arg http_proxy="' + env.HTTP_PROXY + '"' : ''}"
        BAHTTP_PROXY = "${env.HTTP_PROXY ? '--build-arg HTTPS_PROXY="' + env.HTTPS_PROXY + '" --build-arg https_proxy="' + env.HTTPS_PROXY + '"' : ''}"
        UID=sh(script: "id -u", returnStdout: true)
        BUILDARGS = "--build-arg NOBUILD=1 --build-arg UID=$env.UID $env.BAHTTP_PROXY $env.BAHTTPS_PROXY"
    }

    options {
        // preserve stashes so that jobs can be started at the test stage
        preserveStashes(buildCount: 5)
    }

    stages {
        stage('Pre-build') {
            parallel {
                stage('checkpatch') {
                    agent {
                        dockerfile {
                            filename 'Dockerfile.centos:7'
                            dir 'utils/docker'
                            label 'docker_runner'
                            additionalBuildArgs '$BUILDARGS'
                        }
                    }
                    steps {
                        checkPatch user: GITHUB_USER_USR,
                                   password: GITHUB_USER_PSW,
                                   ignored_files: "src/control/vendor/*"
                    }
                    post {
                        always {
                            archiveArtifacts artifacts: 'pylint.log', allowEmptyArchive: true
                            /* when JENKINS-39203 is resolved, can probably use stepResult
                               here and remove the remaining post conditions
                               stepResult name: env.STAGE_NAME,
                                          context: 'build/' + env.STAGE_NAME,
                                          result: ${currentBuild.currentResult}
                            */
                        }
                        /* temporarily moved into stepResult due to JENKINS-39203
                        success {
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'pre-build/' + env.STAGE_NAME,
                                         status: 'SUCCESS'
                        }
                        unstable {
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'pre-build/' + env.STAGE_NAME,
                                         status: 'FAILURE'
                        }
                        failure {
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'pre-build/' + env.STAGE_NAME,
                                         status: 'ERROR'
                        }
                        */
                    }
                }
            }
        }
        stage('Build') {
            // abort other builds if/when one fails to avoid wasting time
            // and resources
            failFast true
            parallel {
                stage('Build on CentOS 7') {
                    agent {
                        dockerfile {
                            filename 'Dockerfile.centos:7'
                            dir 'utils/docker'
                            label 'docker_runner'
                            additionalBuildArgs '$BUILDARGS'
                        }
                    }
                    steps {
                        sconsBuild clean: "_build.external${arch}"
                        // this really belongs in the test stage CORCI-530
                        sh '''scons utest
                              scons utest --utest-mode=memcheck'''
                        stash name: 'CentOS-install', includes: 'install/**'
                        stash name: 'CentOS-build-vars', includes: ".build_vars${arch}.*"
                    }
                    post {
                        /* when JENKINS-39203 is resolved, can probably use stepResult
                           here and remove the remaining post conditions
                        always {
                               stepResult name: env.STAGE_NAME,
                                          context: 'build/' + env.STAGE_NAME,
                                          result: ${currentBuild.currentResult}
                        }
                        */
                        success {
                            /* JENKINS-55417
                            recordIssues enabledForFailure: true,
                                         aggregatingResults: true,
                                         id: "analysis-centos7",
                                         tools: [ gnuMakeGcc(), cppCheck() ],
                                         filters: [excludeFile('.*\\/_build\\.external\\/.*'),
                                                   excludeFile('_build\\.external\\/.*')]
                            */
                            sh 'echo "success"'
                            /* temporarily moved into stepResult due to JENKINS-39203
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'build/' + env.STAGE_NAME,
                                         status: 'SUCCESS'
                            */
                        }
                        /* temporarily moved into stepResult due to JENKINS-39203
                        unstable {
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'build/' + env.STAGE_NAME,
                                         status: 'FAILURE'
                        }
                        failure {
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'build/' + env.STAGE_NAME,
                                         status: 'ERROR'
                        }
                        */
                    }
                }
                stage('Build on CentOS 7 with Clang') {
                    when { branch 'master' }
                    agent {
                        dockerfile {
                            filename 'Dockerfile.centos:7'
                            dir 'utils/docker'
                            label 'docker_runner'
                            additionalBuildArgs '$BUILDARGS'
                        }
                    }
                    steps {
                        sconsBuild clean: "_build.external${arch}", COMPILER: "clang"
                    }
                    post {
                        /* when JENKINS-39203 is resolved, can probably use stepResult
                           here and remove the remaining post conditions
                        always {
                               stepResult name: env.STAGE_NAME,
                                          context: 'build/' + env.STAGE_NAME,
                                          result: ${currentBuild.currentResult}
                        }
                        */
                        success {
                            /* JENKINS-55417
                            recordIssues enabledForFailure: true,
                                         aggregatingResults: true,
                                         id: "analysis-centos7-clang",
                                         tools: [ clang(), cppCheck() ],
                                         filters: [excludeFile('.*\\/_build\\.external\\/.*'),
                                                   excludeFile('_build\\.external\\/.*')]
                            */
                            sh 'echo "success"'
                        /* temporarily moved into stepResult due to JENKINS-39203
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'build/' + env.STAGE_NAME,
                                         status: 'SUCCESS'
                            */
                        }
                        /* temporarily moved into stepResult due to JENKINS-39203
                        unstable {
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'build/' + env.STAGE_NAME,
                                         status: 'FAILURE'
                        }
                        failure {
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'build/' + env.STAGE_NAME,
                                         status: 'ERROR'
                        }
                        */
                    }
                }
                stage('Build on Ubuntu 18.04') {
                    when { branch 'master' }
                    agent {
                        dockerfile {
                            filename 'Dockerfile.ubuntu:18.04'
                            dir 'utils/docker'
                            label 'docker_runner'
                            additionalBuildArgs '$BUILDARGS'
                        }
                    }
                    steps {
                        sconsBuild clean: "_build.external${arch}"
                    }
                    post {
                        /* when JENKINS-39203 is resolved, can probably use stepResult
                           here and remove the remaining post conditions
                        always {
                               stepResult name: env.STAGE_NAME,
                                          context: 'build/' + env.STAGE_NAME,
                                          result: ${currentBuild.currentResult}
                        }
                        */
                        success {
                            /* JENKINS-55417
                            recordIssues enabledForFailure: true,
                                         aggregatingResults: true,
                                         id: "analysis-ubuntu18",
                                         tools: [ gnuMakeGcc(), cppCheck() ],
                                         filters: [excludeFile('.*\\/_build\\.external\\/.*'),
                                                   excludeFile('_build\\.external\\/.*')]
                            */
                            sh 'echo "success"'
                        /* temporarily moved into stepResult due to JENKINS-39203
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'build/' + env.STAGE_NAME,
                                         status: 'SUCCESS'
                            */
                        }
                        /* temporarily moved into stepResult due to JENKINS-39203
                        unstable {
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'build/' + env.STAGE_NAME,
                                         status: 'FAILURE'
                        }
                        failure {
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'build/' + env.STAGE_NAME,
                                         status: 'ERROR'
                        }
                        */
                    }
                }
                stage('Build on Ubuntu 18.04 with Clang') {
                    when { branch 'foobar' }
                    agent {
                        dockerfile {
                            filename 'Dockerfile.ubuntu:18.04'
                            dir 'utils/docker'
                            label 'docker_runner'
                            additionalBuildArgs '$BUILDARGS'
                        }
                    }
                    steps {
                        sconsBuild clean: "_build.external${arch}", COMPILER: "clang"
                    }
                    post {
                        /* when JENKINS-39203 is resolved, can probably use stepResult
                           here and remove the remaining post conditions
                        always {
                               stepResult name: env.STAGE_NAME,
                                          context: 'build/' + env.STAGE_NAME,
                                          result: ${currentBuild.currentResult}
                        }
                        */
                        success {
                            /* JENKINS-55417
                            recordIssues enabledForFailure: true,
                                         aggregatingResults: true,
                                         id: "analysis-ubuntu18-clang",
                                         tools: [ clang(), cppCheck() ],
                                         filters: [excludeFile('.*\\/_build\\.external\\/.*'),
                                                   excludeFile('_build\\.external\\/.*')]
                            */
                            sh 'echo "success"'
                        /* temporarily moved into stepResult due to JENKINS-39203
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'build/' + env.STAGE_NAME,
                                         status: 'SUCCESS'
                            */
                        }
                        /* temporarily moved into stepResult due to JENKINS-39203
                        unstable {
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'build/' + env.STAGE_NAME,
                                         status: 'FAILURE'
                        }
                        failure {
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'build/' + env.STAGE_NAME,
                                         status: 'ERROR'
                        }
                        */
                    }
                }
                stage('Build on Leap 15') {
                    when { branch 'master' }
                    agent {
                        dockerfile {
                            filename 'Dockerfile.leap:15'
                            dir 'utils/docker'
                            label 'docker_runner'
                            additionalBuildArgs '$BUILDARGS'
                        }
                    }
                    steps {
                        sconsBuild clean: "_build.external${arch}"
                    }
                    post {
                        /* when JENKINS-39203 is resolved, can probably use stepResult
                           here and remove the remaining post conditions
                        always {
                               stepResult name: env.STAGE_NAME,
                                          context: 'build/' + env.STAGE_NAME,
                                          result: ${currentBuild.currentResult}
                        }
                        */
                        success {
                            /* JENKINS-55417
                            recordIssues enabledForFailure: true,
                                         aggregatingResults: true,
                                         id: "analysis-leap15",
                                         tools: [ gnuMakeGcc(), cppCheck() ],
                                         filters: [excludeFile('.*\\/_build\\.external\\/.*'),
                                                   excludeFile('_build\\.external\\/.*')]
                            */
                            sh 'echo "success"'
                        /* temporarily moved into stepResult due to JENKINS-39203
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'build/' + env.STAGE_NAME,
                                         status: 'SUCCESS'
                            */
                        }
                        /* temporarily moved into stepResult due to JENKINS-39203
                        unstable {
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'build/' + env.STAGE_NAME,
                                         status: 'FAILURE'
                        }
                        failure {
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'build/' + env.STAGE_NAME,
                                         status: 'ERROR'
                        }
                        */
                    }
                }
                stage('Build on Leap 15 with Clang') {
                    when { branch 'master' }
                    agent {
                        dockerfile {
                            filename 'Dockerfile.leap:15'
                            dir 'utils/docker'
                            label 'docker_runner'
                            additionalBuildArgs '$BUILDARGS'
                        }
                    }
                    steps {
                        sconsBuild clean: "_build.external${arch}", COMPILER: "clang"
                    }
                    post {
                        /* when JENKINS-39203 is resolved, can probably use stepResult
                           here and remove the remaining post conditions
                        always {
                               stepResult name: env.STAGE_NAME,
                                          context: 'build/' + env.STAGE_NAME,
                                          result: ${currentBuild.currentResult}
                        }
                        */
                        success {
                            /* JENKINS-55417
                            recordIssues enabledForFailure: true,
                                         aggregatingResults: true,
                                         id: "analysis-leap15-clang",
                                         tools: [ clang(), cppCheck() ],
                                         filters: [excludeFile('.*\\/_build\\.external\\/.*'),
                                                   excludeFile('_build\\.external\\/.*')]
                            */
                            sh 'echo "success"'
                        /* temporarily moved into stepResult due to JENKINS-39203
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'build/' + env.STAGE_NAME,
                                         status: 'SUCCESS'
                            */
                        }
                        /* temporarily moved into stepResult due to JENKINS-39203
                        unstable {
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'build/' + env.STAGE_NAME,
                                         status: 'FAILURE'
                        }
                        failure {
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'build/' + env.STAGE_NAME,
                                         status: 'ERROR'
                        }
                        */
                    }
                }
                stage('Build on Leap 15 with Intel-C') {
                    when { branch 'foobar' }
                    agent {
                        dockerfile {
                            filename 'Dockerfile.leap:15'
                            dir 'utils/docker'
                            label 'docker_runner'
                            additionalBuildArgs '$BUILDARGS'
                            args '-v /opt:/opt'
                        }
                    }
                    steps {
                        sconsBuild clean: "_build.external${arch}", COMPILER: "icc"
                    }
                    post {
                        /* when JENKINS-39203 is resolved, can probably use stepResult
                           here and remove the remaining post conditions
                        always {
                               stepResult name: env.STAGE_NAME,
                                          context: 'build/' + env.STAGE_NAME,
                                          result: ${currentBuild.currentResult}
                        }
                        */
                        success {
                            /* JENKINS-55417
                            recordIssues enabledForFailure: true,
                                         aggregatingResults: true,
                                         id: "analysis-leap15-intelc",
                                         tools: [ intel(), cppCheck() ],
                                         filters: [excludeFile('.*\\/_build\\.external\\/.*'),
                                                   excludeFile('_build\\.external\\/.*')]
                            */
                            sh 'echo "success"'
                        /* temporarily moved into stepResult due to JENKINS-39203
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'build/' + env.STAGE_NAME,
                                         status: 'SUCCESS'
                            */
                        }
                        /* temporarily moved into stepResult due to JENKINS-39203
                        unstable {
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'build/' + env.STAGE_NAME,
                                         status: 'FAILURE'
                        }
                        failure {
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'build/' + env.STAGE_NAME,
                                         status: 'ERROR'
                        }
                        */
                    }
                }
            }
        }
        stage('Test') {
            parallel {
                stage('Single-node') {
                    agent {
                        label 'ci_vm1'
                    }
                    environment {
                        CART_TEST_MODE = 'native'
                    }
                    steps {
                        singleNodeTest()
                    }
                    post {
                        always {
                             archiveArtifacts artifacts: '''install/Linux/TESTING/testLogs/**,
                                                            build/Linux/src/utest/utest.log,
                                                            build/Linux/src/utest/test_output'''
                            /* when JENKINS-39203 is resolved, can probably use stepResult
                               here and remove the remaining post conditions
                               stepResult name: env.STAGE_NAME,
                                          context: 'build/' + env.STAGE_NAME,
                                          result: ${currentBuild.currentResult}
                            */
                        }
                        /* temporarily moved into runTest->stepResult due to JENKINS-39203
                        success {
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'test/' + env.STAGE_NAME,
                                         status: 'SUCCESS'
                        }
                        unstable {
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'test/' + env.STAGE_NAME,
                                         status: 'FAILURE'
                        }
                        failure {
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'test/' + env.STAGE_NAME,
                                         status: 'ERROR'
                        }
                        */
                    }
                }
                stage('Single-node-valgrind') {
                    agent {
                        label 'ci_vm1'
                    }
                    environment {
                        CART_TEST_MODE = 'memcheck'
                    }
                    steps {
                        singleNodeTest()
                    }
                    post {
                        always {
                            sh ''' mv  install/Linux/TESTING/testLogs{,_valgrind}
                                  mv build/Linux/src/utest{,_valgrind}'''
                             archiveArtifacts artifacts: '''install/Linux/TESTING/testLogs_valgrind/**,
                                                            build/Linux/src/utest_valgrind/utest.log,
                                                            build/Linux/src/utest_valgrind/test_output'''
                        /* when JENKINS-39203 is resolved, can probably use stepResult
                           here and remove the remaining post conditions
                           stepResult name: env.STAGE_NAME,
                                   context: 'build/' + env.STAGE_NAME,
                                    result: ${currentBuild.currentResult}
                        */
                        }
                        /* temporarily moved into runTest->stepResult due to JENKINS-39203
                        success {
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'test/' + env.STAGE_NAME,
                                         status: 'SUCCESS'
                        }
                        unstable {
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'test/' + env.STAGE_NAME,
                                         status: 'FAILURE'
                        }
                        failure {
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'test/' + env.STAGE_NAME,
                                         status: 'ERROR'
                        }
                        */
                    }
                }
                stage('Two-node') {
                    agent {
                        label 'cluster_provisioner-2_nodes'
                    }
                    steps {
                        echo "Starting Two-node"
                        checkoutScm url: 'ssh://review.hpdd.intel.com:29418/exascale/jenkins',
                                    checkoutDir: 'jenkins',
                                    credentialsId: 'bf21c68b-9107-4a38-8077-e929e644996a'

                        checkoutScm url: 'ssh://review.hpdd.intel.com:29418/coral/scony_python-junit',
                                    checkoutDir: 'scony_python-junit',
                                    credentialsId: 'bf21c68b-9107-4a38-8077-e929e644996a'

                        echo "Starting Two-node runTest"
                        runTest stashes: [ 'CentOS-install', 'CentOS-build-vars' ],
                                script: 'bash -x ./multi-node-test.sh 2; echo "rc: $?"',
                                junit_files: "CART_2-node_junit.xml"
                    }
                    post {
                        always {
                            junit 'CART_2-node_junit.xml'
                            archiveArtifacts artifacts: 'install/Linux/TESTING/testLogs-2_node/**'
                            /* when JENKINS-39203 is resolved, can probably use stepResult
                               here and remove the remaining post conditions
                               stepResult name: env.STAGE_NAME,
                                          context: 'build/' + env.STAGE_NAME,
                                          result: ${currentBuild.currentResult}
                            */
                        }
                        /* temporarily moved into runTest->stepResult due to JENKINS-39203
                        success {
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'test/' + env.STAGE_NAME,
                                         status: 'SUCCESS'
                        }
                        unstable {
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'test/' + env.STAGE_NAME,
                                         status: 'FAILURE'
                        }
                        failure {
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'test/' + env.STAGE_NAME,
                                         status: 'ERROR'
                        }
                        */
                    }
                }
                stage('Three-node') {
                    agent {
                        label 'cluster_provisioner-5_nodes'
                    }
                    steps {
                        echo "Starting Three-node"
                        checkoutScm url: 'ssh://review.hpdd.intel.com:29418/exascale/jenkins',
                                    checkoutDir: 'jenkins',
                                    credentialsId: 'bf21c68b-9107-4a38-8077-e929e644996a'

                        checkoutScm url: 'ssh://review.hpdd.intel.com:29418/coral/scony_python-junit',
                                    checkoutDir: 'scony_python-junit',
                                    credentialsId: 'bf21c68b-9107-4a38-8077-e929e644996a'

                        echo "Starting Three-node runTest"
                        runTest stashes: [ 'CentOS-install', 'CentOS-build-vars' ],
                                script: 'bash -x ./multi-node-test.sh 3; echo "rc: $?"',
                                junit_files: "CART_3-node_junit.xml"
                    }
                    post {
                        always {
                            junit 'CART_3-node_junit.xml'
                            archiveArtifacts artifacts: 'install/Linux/TESTING/testLogs-3_node/**'
                            /* when JENKINS-39203 is resolved, can probably use stepResult
                               here and remove the remaining post conditions
                               stepResult name: env.STAGE_NAME,
                                          context: 'build/' + env.STAGE_NAME,
                                          result: ${currentBuild.currentResult}
                            */
                        }
                        /* temporarily moved into runTest->stepResult due to JENKINS-39203
                        success {
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'test/' + env.STAGE_NAME,
                                         status: 'SUCCESS'
                        }
                        unstable {
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'test/' + env.STAGE_NAME,
                                         status: 'FAILURE'
                        }
                        failure {
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'test/' + env.STAGE_NAME,
                                         status: 'ERROR'
                        }
                        */
                    }
                }
                stage('Five-node') {
                    agent {
                        label 'cluster_provisioner-5_nodes'
                    }
                    steps {
                        echo "Starting Five-node"
                        checkoutScm url: 'ssh://review.hpdd.intel.com:29418/exascale/jenkins',
                                    checkoutDir: 'jenkins',
                                    credentialsId: 'bf21c68b-9107-4a38-8077-e929e644996a'

                        checkoutScm url: 'ssh://review.hpdd.intel.com:29418/coral/scony_python-junit',
                                    checkoutDir: 'scony_python-junit',
                                    credentialsId: 'bf21c68b-9107-4a38-8077-e929e644996a'

                        echo "Starting Five-node runTest"
                        runTest stashes: [ 'CentOS-install', 'CentOS-build-vars' ],
                                script: 'bash -x ./multi-node-test.sh 5; echo "rc: $?"',
                                junit_files: "CART_5-node_junit.xml"
                    }
                    post {
                        always {
                            junit 'CART_5-node_junit.xml'
                            archiveArtifacts artifacts: 'install/Linux/TESTING/testLogs-5_node/**'
                            /* when JENKINS-39203 is resolved, can probably use stepResult
                               here and remove the remaining post conditions
                               stepResult name: env.STAGE_NAME,
                                          context: 'build/' + env.STAGE_NAME,
                                          result: ${currentBuild.currentResult}
                            */
                        }
                        /* temporarily moved into runTest->stepResult due to JENKINS-39203
                        success {
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'test/' + env.STAGE_NAME,
                                         status: 'SUCCESS'
                        }
                        unstable {
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'test/' + env.STAGE_NAME,
                                         status: 'FAILURE'
                        }
                        failure {
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'test/' + env.STAGE_NAME,
                                         status: 'ERROR'
                        }
                        */
                    }
                }
            }
        }
    }
}
