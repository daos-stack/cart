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
//NOTESTYET        stage('Unit Test') {
//NOTESTYET            parallel {
//NOTESTYET                stage('run_test.sh') {
//NOTESTYET                    agent {
//NOTESTYET                        label 'single'
//NOTESTYET                    }
//NOTESTYET                    steps {
//NOTESTYET                        runTest stashes: [ 'CentOS-tests', 'CentOS-install', 'CentOS-build-vars' ],
//NOTESTYET                                script: 'LD_LIBRARY_PATH=install/Linux/lib64:install/Linux/lib HOSTPREFIX=wolf-53 bash -x utils/run_test.sh --init && echo "run_test.sh exited successfully with ${PIPESTATUS[0]}" || echo "run_test.sh exited failure with ${PIPESTATUS[0]}"',
//NOTESTYET                              junit_files: null
//NOTESTYET                    }
//NOTESTYET                    post {
//NOTESTYET                        /* temporarily moved into runTest->stepResult due to JENKINS-39203
//NOTESTYET                        success {
//NOTESTYET                            githubNotify credentialsId: 'daos-jenkins-commit-status', description: 'run_test.sh',  context: 'test/run_test.sh', status: 'SUCCESS'
//NOTESTYET                        }
//NOTESTYET                        unstable {
//NOTESTYET                            githubNotify credentialsId: 'daos-jenkins-commit-status', description: 'run_test.sh',  context: 'test/run_test.sh', status: 'FAILURE'
//NOTESTYET                        }
//NOTESTYET                        failure {
//NOTESTYET                            githubNotify credentialsId: 'daos-jenkins-commit-status', description: 'run_test.sh',  context: 'test/run_test.sh', status: 'ERROR'
//NOTESTYET                        }
//NOTESTYET                        */
//NOTESTYET                        always {
//NOTESTYET                            sh '''rm -rf run_test.sh/
//NOTESTYET                                  mkdir run_test.sh/
//NOTESTYET                                  [ -f /tmp/daos.log ] && mv /tmp/daos.log run_test.sh/ || true'''
//NOTESTYET                            archiveArtifacts artifacts: 'run_test.sh/**'
//NOTESTYET                        }
//NOTESTYET                    }
//NOTESTYET                }
//NOTESTYET            }
//NOTESTYET        }
//NOTESTYET        stage('Test') {
//NOTESTYET            parallel {
//NOTESTYET                stage('Functional all') {
//NOTESTYET                    agent {
//NOTESTYET                        label 'cluster_provisioner'
//NOTESTYET                    }
//NOTESTYET                    steps {
//NOTESTYET                        runTest stashes: [ 'CentOS-install', 'CentOS-build-vars' ],
//NOTESTYET                                script: 'bash ftest.sh all; echo "rc: $?"',
//NOTESTYET                                junit_files: "src/tests/ftest/avocado/job-results/*/*.xml"
//NOTESTYET                    }
//NOTESTYET                    post {
//NOTESTYET                        /* temporarily moved into runTest->stepResult due to JENKINS-39203
//NOTESTYET                        success {
//NOTESTYET                            githubNotify credentialsId: 'daos-jenkins-commit-status', description: 'Functional all',  context: 'test/functional_quick', status: 'SUCCESS'
//NOTESTYET                        }
//NOTESTYET                        unstable {
//NOTESTYET                            githubNotify credentialsId: 'daos-jenkins-commit-status', description: 'Functional all',  context: 'test/functional_quick', status: 'FAILURE'
//NOTESTYET                        }
//NOTESTYET                        failure {
//NOTESTYET                            githubNotify credentialsId: 'daos-jenkins-commit-status', description: 'Functional all',  context: 'test/functional_quick', status: 'ERROR'
//NOTESTYET                        }
//NOTESTYET                        */
//NOTESTYET                        always {
//NOTESTYET                            sh '''rm -rf src/tests/ftest/avocado/job-results/*/html/ "Functional all"/
//NOTESTYET                                  mkdir "Functional all"/
//NOTESTYET                                  ls daos.log* && mv daos.log* "Functional all"/ || true
//NOTESTYET                                  mv src/tests/ftest/avocado/job-results/* "Functional all"/
//NOTESTYET                                  ls -l "Functional all"/ || true'''
//NOTESTYET                            junit 'Functional all/*/results.xml'
//NOTESTYET                            archiveArtifacts artifacts: 'Functional all/**'
//NOTESTYET                        }
//NOTESTYET                    }
//NOTESTYET                }
//NOTESTYET// we don't need to run this separately when running "all" above
//NOTESTYET//                stage('Functional daos_test') {
//NOTESTYET//                    agent {
//NOTESTYET//                        label 'cluster_provisioner'
//NOTESTYET//                    }
//NOTESTYET//                    steps {
//NOTESTYET//                        runTest stashes: [ 'CentOS-install', 'CentOS-build-vars' ],
//NOTESTYET//                                script: 'bash ftest.sh daos_test; echo "rc: $?"',
//NOTESTYET//                                // Hrm.  I wonder if there is any way to tell Avocado
//NOTESTYET//                                // to put daos_test's own JUnit files into a job-results
//NOTESTYET//                                // dir
//NOTESTYET//                                junit_files: "install/Linux/tmp/*results.xml"
//NOTESTYET//                    }
//NOTESTYET//                    post {
//NOTESTYET//                        /* temporarily moved into runTest->stepResult due to JENKINS-39203
//NOTESTYET//                        success {
//NOTESTYET//                            githubNotify credentialsId: 'daos-jenkins-commit-status', description: 'Functional daos_test',  context: 'test/functional_daos_test', status: 'SUCCESS'
//NOTESTYET//                        }
//NOTESTYET//                        unstable {
//NOTESTYET//                            githubNotify credentialsId: 'daos-jenkins-commit-status', description: 'Functional daos_test',  context: 'test/functional_daos_test', status: 'FAILURE'
//NOTESTYET//                        }
//NOTESTYET//                        failure {
//NOTESTYET//                            githubNotify credentialsId: 'daos-jenkins-commit-status', description: 'Functional daos_test',  context: 'test/functional_daos_test', status: 'ERROR'
//NOTESTYET//                        }
//NOTESTYET//                        */
//NOTESTYET//                        always {
//NOTESTYET//                            sh '''rm -rf src/tests/ftest/avocado/job-results/*/html/ "Functional daos_test"/
//NOTESTYET//                                  mkdir "Functional daos_test"/
//NOTESTYET//                                  mv src/tests/ftest/avocado/job-results/* "Functional daos_test"/
//NOTESTYET//                                  pwd
//NOTESTYET//                                  ls *daos.log* && mv -f *daos.log* "Functional daos_test"/ || true
//NOTESTYET//                                  find "Functional daos_test"/ -print || true'''
//NOTESTYET//                            junit 'install/Linux/tmp/*results.xml, Functional daos_test/*/results.xml'
//NOTESTYET//                            archiveArtifacts artifacts: 'Functional daos_test/**'
//NOTESTYET//                        }
//NOTESTYET//                    }
//NOTESTYET//                }
//NOTESTYET            }
//NOTESTYET        }
    }
}
