#!/usr/bin/groovy
/* Copyright (C) 2019 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted for any purpose (including commercial purposes)
 * provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the
 *    documentation and/or materials provided with the distribution.
 *
 * 3. In addition, redistributions of modified forms of the source or binary
 *    code must carry prominent notices stating that the original code was
 *    changed and the date of the change.
 *
 *  4. All publications or advertising materials mentioning features or use of
 *     this software are asked, but not required, to acknowledge that it was
 *     developed by Intel Corporation and credit the contributors.
 *
 * 5. Neither the name of Intel Corporation, nor the name of any Contributor
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
// To use a test branch (i.e. PR) until it lands to master
// I.e. for testing library changes
//@Library(value="pipeline-lib@your_branch") _

def arch="-Linux"
def sanitized_JOB_NAME = JOB_NAME.toLowerCase().replaceAll('/', '-').replaceAll('%2f', '-')

pipeline {
    agent { label 'lightweight' }

    triggers {
        cron(env.BRANCH_NAME == 'master' ? '0 0 * * *' : '')
    }

    environment {
        GITHUB_USER = credentials('daos-jenkins-review-posting')
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
        stage('Cancel Previous Builds') {
            when { changeRequest() }
            steps {
                cancelPreviousBuilds()
            }
        }
        stage('Pre-build') {
            parallel {
                stage('checkpatch') {
                    agent {
                        dockerfile {
                            filename 'Dockerfile.centos.7'
                            dir 'utils/docker'
                            label 'docker_runner'
                            additionalBuildArgs "-t ${sanitized_JOB_NAME}-centos7 " + '$BUILDARGS'
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
            /* Don't use failFast here as whilst it avoids using extra resources
             * and gives faster results for PRs it's also on for master where we
	     * do want complete results in the case of partial failure
	     */
            //failFast true
            parallel {
                stage('Build on CentOS 7') {
                    agent {
                        dockerfile {
                            filename 'Dockerfile.centos.7'
                            dir 'utils/docker'
                            label 'docker_runner'
                            additionalBuildArgs "-t ${sanitized_JOB_NAME}-centos7 " + '$BUILDARGS'
                        }
                    }
                    steps {
                        sconsBuild clean: "_build.external${arch}"
                        // this really belongs in the test stage CORCI-530
                        sh '''scons utest --utest-mode=memcheck
                              mv build/Linux/src/utest{,_valgrind}
                              scons utest'''
                        stash name: 'CentOS-install', includes: 'install/**'
                        stash name: 'CentOS-build-vars', includes: ".build_vars${arch}.*"
                    }
                    post {
                        always {
                            node('lightweight') {
                                recordIssues enabledForFailure: true,
                                             aggregatingResults: true,
                                             id: "analysis-centos7",
                                             tools: [ gcc4(), cppCheck() ],
                                             filters: [excludeFile('.*\\/_build\\.external-Linux\\/.*'),
                                                       excludeFile('_build\\.external-Linux\\/.*')]
                            }
                            archiveArtifacts artifacts: '''build/Linux/src/utest_valgrind/utest.log,
                                                           build/Linux/src/utest_valgrind/test_output,
                                                           build/Linux/src/utest/utest.log,
                                                           build/Linux/src/utest/test_output'''
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
                                         context: 'build/' + env.STAGE_NAME,
                                         status: 'SUCCESS'
                        }
                        */
                        unstable {
                            sh "mv config${arch}.log config.log-centos7-gcc"
                            archiveArtifacts artifacts: 'config.log-centos7-gcc'
                            /* temporarily moved into stepResult due to JENKINS-39203
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'build/' + env.STAGE_NAME,
                                         status: 'FAILURE'
                            */
                        }
                        failure {
                            sh "mv config${arch}.log config.log-centos7-gcc"
                            archiveArtifacts artifacts: 'config.log-centos7-gcc'
                            /* temporarily moved into stepResult due to JENKINS-39203
                            githubNotify credentialsId: 'daos-jenkins-commit-status',
                                         description: env.STAGE_NAME,
                                         context: 'build/' + env.STAGE_NAME,
                                         status: 'ERROR'
                            */
                        }
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
                    steps {
                        provisionNodes NODELIST: env.NODELIST,
                                       node_count: 1,
                                       snapshot: true
                        runTest stashes: [ 'CentOS-install', 'CentOS-build-vars' ],
                                script: '''export PDSH_SSH_ARGS_APPEND="-i ci_key"
                                           export CART_TEST_MODE=native
                                           bash -x ./multi-node-test.sh 1 ''' +
                                           env.NODELIST + ''' one_node''',
                                junit_files: "install/Linux/TESTING/avocado/job-results/CART_1node/*/*.xml"
                    }
                    post {
                        always {
                            sh '''rm -rf install/Linux/TESTING/avocado/job-results/CART_1node/*/html/
                                  if [ -n "$STAGE_NAME" ]; then
                                      rm -rf "$STAGE_NAME/"
                                      mkdir "$STAGE_NAME/"
                                      mv install/Linux/TESTING/avocado/job-results/CART_1node/* \
                                         "$STAGE_NAME/"
                                  else
                                      echo "The STAGE_NAME environment variable is missing!"
                                      false
                                  fi'''
                            junit env.STAGE_NAME + '/*/results.xml'
                            archiveArtifacts artifacts: [env.STAGE_NAME + '/**',
                                                         'install/Linux/TESTING/testLogs-1_node/**']
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
                        provisionNodes NODELIST: env.NODELIST,
                                       node_count: 2,
                                       snapshot: true
                        runTest stashes: [ 'CentOS-install', 'CentOS-build-vars' ],
                                script: '''export PDSH_SSH_ARGS_APPEND="-i ci_key"
                                           export CART_TEST_MODE=none
                                           bash -x ./multi-node-test.sh 2 ''' +
                                           env.NODELIST + ''' two_node''',
                                junit_files: "install/Linux/TESTING/avocado/job-results/CART_2node/*/*.xml"
                    }
                    post {
                        always {
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
            }
        }
    }
}
