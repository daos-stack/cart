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
        /*stage('Pre-build') {
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
                        }
                    }
                }
            }
        }*/
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
                        //stash name: 'CentOS-tests', includes: 'build/src/rdb/raft/src/tests_main, build/src/common/tests/btree_direct, build/src/common/tests/btree, src/common/tests/btree.sh, build/src/common/tests/sched, build/src/client/api/tests/eq_tests, src/vos/tests/evt_ctl.sh, build/src/vos/vea/tests/vea_ut, src/rdb/raft_tests/raft_tests.py'
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
            }
        }
        stage('Unit Test') {
            parallel {
                /*stage('Single Node') {
                    agent {
                        /* See if adding dockerfile to test lets it see scons */ 
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
                }*/
                stage('Two Node') {
                    agent {
                        /* See if adding dockerfile to test lets it see scons */ 
                        dockerfile {
                            filename 'Dockerfile.centos:7'
                            dir 'utils/docker'
                            label 'docker_runner'
                            additionalBuildArgs '$BUILDARGS'
                        }
                    }
                    steps {
                        runTest stashes: [ 'CentOS-install', 'CentOS-build-vars' ],
                                script: 'bash -x utils/run_test.sh --config utils/config.json && echo "run_test.sh exited successfully with ${PIPESTATUS[0]}" || echo "run_test.sh exited failure with ${PIPESTATUS[0]}"',
                              junit_files: null
                    }
                    post {
                        always {
                             archiveArtifacts artifacts: 'install/Linux/TESTING/testLogs/**,build/Linux/src/utest/utest.log,build/Linux/src/utest/test_output', allowEmptyArchive: true
                        }
                    }
                }
            }
        }
    }
}
