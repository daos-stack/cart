// To use a test branch (i.e. PR) until it lands to master
// I.e. for testing library changes
@Library(value="pipeline-lib@debug") _

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
                        /* temporarily moved into stepResult due to JENKINS-39203
                        success {
                            githubNotify credentialsId: 'daos-jenkins-commit-status', description: 'checkpatch',  context: 'pre-build/checkpatch', status: 'SUCCESS'
                        }
                        unstable {
                            githubNotify credentialsId: 'daos-jenkins-commit-status', description: 'checkpatch',  context: 'pre-build/checkpatch', status: 'FAILURE'
                        }
                        failure {
                            githubNotify credentialsId: 'daos-jenkins-commit-status', description: 'checkpatch',  context: 'pre-build/checkpatch', status: 'ERROR'
                        }
                        */
                        always {
                            archiveArtifacts artifacts: 'pylint.log', allowEmptyArchive: true
                        }
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
                        sconsBuild clean: "_build.external-Linux"
                        stash name: 'CentOS-install', includes: 'install/**'
                        stash name: 'CentOS-build-vars', includes: '.build_vars-Linux.*'
                    }
                    post {
                        always {
                            recordIssues enabledForFailure: true,
                                         aggregatingResults: true,
                                         id: "analysis-centos7",
                                         tools: [
                                             [tool: [$class: 'GnuMakeGcc']],
                                             [tool: [$class: 'CppCheck']],
                                         ],
                                         filters: [excludeFile('.*\\/_build\\.external\\/.*'),
                                                   excludeFile('_build\\.external\\/.*')]
                        }
                        /* temporarily moved into stepResult due to JENKINS-39203
                        success {
                            githubNotify credentialsId: 'daos-jenkins-commit-status', description: 'CentOS 7 Build',  context: 'build/centos7', status: 'SUCCESS'
                        }
                        unstable {
                            githubNotify credentialsId: 'daos-jenkins-commit-status', description: 'CentOS 7 Build',  context: 'build/centos7', status: 'FAILURE'
                        }
                        failure {
                            githubNotify credentialsId: 'daos-jenkins-commit-status', description: 'CentOS 7 Build',  context: 'build/centos7', status: 'ERROR'
                        }
                        */
                    }
                }
                stage('Build on Ubuntu 18.04') {
                    agent {
                        dockerfile {
                            filename 'Dockerfile.ubuntu:18.04'
                            dir 'utils/docker'
                            label 'docker_runner'
                            additionalBuildArgs '$BUILDARGS'
                        }
                    }
                    steps {
                        sh '''echo "Skipping Ubuntu 18 build due to https://jira.hpdd.intel.com/browse/CART-548"
                              exit 0'''
                        //sconsBuild clean: "_build.external-Linux"
                    }
                    post {
                        always {
                            recordIssues enabledForFailure: true,
                                         aggregatingResults: true,
                                         id: "analysis-ubuntu18",
                                         tools: [
                                             [tool: [$class: 'GnuMakeGcc']],
                                             [tool: [$class: 'CppCheck']],
                                         ],
                                         filters: [excludeFile('.*\\/_build\\.external\\/.*'),
                                                   excludeFile('_build\\.external\\/.*')]
                        }
                        /* temporarily moved into stepResult due to JENKINS-39203
                        success {
                            githubNotify credentialsId: 'daos-jenkins-commit-status', description: 'Ubuntu 18 Build',  context: 'build/ubuntu18', status: 'SUCCESS'
                        }
                        unstable {
                            githubNotify credentialsId: 'daos-jenkins-commit-status', description: 'Ubuntu 18 Build',  context: 'build/ubuntu18', status: 'FAILURE'
                        }
                        failure {
                            githubNotify credentialsId: 'daos-jenkins-commit-status', description: 'Ubuntu 18 Build',  context: 'build/ubuntu18', status: 'ERROR'
                        }
                        */
                    }
                }
            }
        }
        stage('Test') {
            parallel {
                /* stage('Single node') {
                    agent {
                        label 'single'
                        // can this really run in docker?
                        // dockerfile {
                        //     filename 'Dockerfile.centos:7'
                        //     dir 'utils/docker'
                        //     label 'docker_runner'
                        //     additionalBuildArgs '$BUILDARGS'
                        // }
                    }
                    steps {
                        runTest stashes: [ 'CentOS-install', 'CentOS-build-vars' ],
                                script: 'bash -x utils/run_test.sh && echo "run_test.sh exited successfully with ${PIPESTATUS[0]}" || echo "run_test.sh exited failure with ${PIPESTATUS[0]}"',
                              junit_files: null
                    }
                    post {
                        always {
                             archiveArtifacts artifacts: 'install/Linux/TESTING/testLogs/**,build/Linux/src/utest/utest.log,build/Linux/src/utest/test_output', allowEmptyArchive: true
                        }
                    }
                }*/
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
                        /* temporarily moved into runTest->stepResult due to JENKINS-39203
                        success {
                            githubNotify credentialsId: 'daos-jenkins-commit-status', description: 'Functional daos_test',  context: 'test/functional_daos_test', status: 'SUCCESS'
                        }
                        unstable {
                            githubNotify credentialsId: 'daos-jenkins-commit-status', description: 'Functional daos_test',  context: 'test/functional_daos_test', status: 'FAILURE'
                        }
                        failure {
                            githubNotify credentialsId: 'daos-jenkins-commit-status', description: 'Functional daos_test',  context: 'test/functional_daos_test', status: 'ERROR'
                        }
                        */
                        always {
                            junit 'CART_2-node_junit.xml'
                            archiveArtifacts artifacts: 'install/Linux/TESTING/testLogs-2_node/**'
                        }
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
                        /* temporarily moved into runTest->stepResult due to JENKINS-39203
                        success {
                            githubNotify credentialsId: 'daos-jenkins-commit-status', description: 'Functional daos_test',  context: 'test/functional_daos_test', status: 'SUCCESS'
                        }
                        unstable {
                            githubNotify credentialsId: 'daos-jenkins-commit-status', description: 'Functional daos_test',  context: 'test/functional_daos_test', status: 'FAILURE'
                        }
                        failure {
                            githubNotify credentialsId: 'daos-jenkins-commit-status', description: 'Functional daos_test',  context: 'test/functional_daos_test', status: 'ERROR'
                        }
                        */
                        always {
                            junit 'CART_5-node_junit.xml'
                            archiveArtifacts artifacts: 'install/Linux/TESTING/testLogs-5_node/**'
                        }
                    }
                }
            }
        }
    }
}
