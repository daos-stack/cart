// To use a test branch (i.e. PR) until it lands to master
// I.e. for testing library changes
@Library(value="pipeline-lib@sconsBuild-clean") _

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
                    }
                }
            }
        }
        stage('Unit Test') {
            parallel {
                stage('Single Node') {
                    agent {
                        dockerfile {
                            filename 'Dockerfile.centos:7'
                            dir 'utils/docker'
                            label 'docker_runner'
                            additionalBuildArgs '$BUILDARGS'
                        }
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
                }
                stage('Two Node') {
                    agent {
                        label 'cluster_provisioner-2_nodes'
                    }
                    steps {
                        echo "Starting Two-node runTest"
                        runTest stashes: [ 'CentOS-install', 'CentOS-build-vars' ],
                                script: 'bash -x ./multi-node-test.sh 2; echo "rc: $?"',
                                junit_files: null
                    }
                    post {
                        always {
                             archiveArtifacts artifacts: 'install/Linux/TESTING/testLogs-2_node/**', allowEmptyArchive: true
                        }
                    }
                }
            }
        }
    }
}
