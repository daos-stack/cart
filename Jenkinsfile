// To use a test branch (i.e. PR) until it lands to master
// I.e. for testing library changes
@Library(value="pipeline-lib@debug") _

def singleNodeTest(nodelist) {
    echo "Running singleNodeTest on " + nodelist
    provisionNodes NODELIST: nodelist,
                   node_count: 1,
                   snapshot: true
    runTest stashes: [ 'CentOS-install', 'CentOS-build-vars' ],
            script: """pwd
                       ls -l
                       . ./.build_vars-Linux.sh
                       CART_BASE=\${SL_PREFIX%/install*}
                       NODELIST=$nodelist
                       NODE=\${NODELIST%%,*}
                       trap 'set +e; set -x; ssh \$NODE "set -ex; sudo umount \$CART_BASE"' EXIT
                       ssh \$NODE "set -x
                       set -e
                       sudo mkdir -p \$CART_BASE
                       sudo mount -t nfs \$HOSTNAME:\$PWD \$CART_BASE
                       cd \$CART_BASE
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
                        sh 'env'
                        singleNodeTest(env.NODELIST)
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
                        sh 'env'
                        singleNodeTest(env.NODELIST)
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
                        label 'ci_vm2'
                    }
                    steps {
                        sh 'env'
                        echo "Starting Two-node"
                        provisionNodes NODELIST: env.NODELIST,
                                       node_count: 2,
                                       snapshot: true
                        checkoutScm url: 'ssh://review.hpdd.intel.com:29418/exascale/jenkins',
                                    checkoutDir: 'jenkins',
                                    credentialsId: 'bf21c68b-9107-4a38-8077-e929e644996a'

                        checkoutScm url: 'ssh://review.hpdd.intel.com:29418/coral/scony_python-junit',
                                    checkoutDir: 'scony_python-junit',
                                    credentialsId: 'bf21c68b-9107-4a38-8077-e929e644996a'

                        echo "Starting Two-node runTest"
                        runTest stashes: [ 'CentOS-install', 'CentOS-build-vars' ],
                                script: "bash -x ./multi-node-test.sh 2 " + 
                                        env.NODELIST + "; echo \"rc: \$?\"",
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
                        label 'ci_vm3'
                    }
                    steps {
                        sh 'env'
                        echo "Starting Three-node"
                        provisionNodes NODELIST: env.NODELIST,
                                       node_count: 3,
                                       snapshot: true
                        checkoutScm url: 'ssh://review.hpdd.intel.com:29418/exascale/jenkins',
                                    checkoutDir: 'jenkins',
                                    credentialsId: 'bf21c68b-9107-4a38-8077-e929e644996a'

                        checkoutScm url: 'ssh://review.hpdd.intel.com:29418/coral/scony_python-junit',
                                    checkoutDir: 'scony_python-junit',
                                    credentialsId: 'bf21c68b-9107-4a38-8077-e929e644996a'

                        echo "Starting Three-node runTest"
                        runTest stashes: [ 'CentOS-install', 'CentOS-build-vars' ],
                                script: "bash -x ./multi-node-test.sh 3 " +
                                        env.NODELIST + "; echo \"rc: \$?\"",
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
                        label 'ci_vm5'
                    }
                    steps {
                        sh 'env'
                        echo "Starting Five-node"
                        provisionNodes NODELIST: env.NODELIST,
                                       node_count: 5,
                                       snapshot: true
                        checkoutScm url: 'ssh://review.hpdd.intel.com:29418/exascale/jenkins',
                                    checkoutDir: 'jenkins',
                                    credentialsId: 'bf21c68b-9107-4a38-8077-e929e644996a'

                        checkoutScm url: 'ssh://review.hpdd.intel.com:29418/coral/scony_python-junit',
                                    checkoutDir: 'scony_python-junit',
                                    credentialsId: 'bf21c68b-9107-4a38-8077-e929e644996a'

                        echo "Starting Five-node runTest"
                        runTest stashes: [ 'CentOS-install', 'CentOS-build-vars' ],
                                script: "bash -x ./multi-node-test.sh 5 " +
                                        env.NODELIST + "; echo \"rc: \$?\"",
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
=======
>>>>>>> bf31105... Provisioner testing
                }
            }
        }
    }
}
