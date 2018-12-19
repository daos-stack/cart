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
                stage('Single-node tests') {
                    agent {
                        dockerfile {
                            filename 'Dockerfile.centos:7'
                            dir 'utils/docker'
                            label 'docker_runner'
                            additionalBuildArgs '$BUILDARGS'
                        }
                    }
                    stages {
                        stage('Single-node') {
                            environment {
                                CART_TEST_MODE = 'native'
                            }
                            steps {
                                runTest stashes: [ 'CentOS-install', 'CentOS-build-vars' ],
                                        script: '''pwd
                                                ls -l
                                                . ./.build_vars-Linux.sh
                                                if [ ! -d $SL_PREFIX ]; then
                                                    mkdir -p ${SL_PREFIX%/Linux} || {
                                                    ls -l /var/lib/ /var/lib/jenkins || true
                                                    exit 1
                                                    }
                                                    ln -s $SL_PREFIX/install
                                                fi
                                                if bash -x utils/run_test.sh; then
                                                    echo "run_test.sh exited successfully with ${PIPESTATUS[0]}"
                                                else
                                                    echo "run_test.sh exited failure with ${PIPESTATUS[0]}"
                                                fi''',
                                        junit_files: null
                            }
                            post {
                                /* temporarily moved into runTest->stepResult due to JENKINS-39203
                                success {
                                    githubNotify credentialsId: 'daos-jenkins-commit-status', description: env.STAGE_NAME,  context: 'test/functional_quick', status: 'SUCCESS'
                                }
                                unstable {
                                    githubNotify credentialsId: 'daos-jenkins-commit-status', description: env.STAGE_NAME,  context: 'test/functional_quick', status: 'FAILURE'
                                }
                                failure {
                                    githubNotify credentialsId: 'daos-jenkins-commit-status', description: env.STAGE_NAME,  context: 'test/functional_quick', status: 'ERROR'
                                }
                                */
                                always {
                                    sh '''mkdir -p install/Linux/TESTING/testLogs_non_valgrind
                                          mv install/Linux/TESTING/testLogs/** install/Linux/TESTING/testLogs_non_valgrind
                                          mkdir -p build/Linux/src/utest_non_valgrind
                                          mv build/Linux/src/utest/utest.log build/Linux/src/utest_non_valgrind
                                          mv build/Linux/src/utest/test_output build/Linux/src/utest_non_valgrind'''
                                    archiveArtifacts artifacts: 'install/Linux/TESTING/testLogs_non_valgrind/**,build/Linux/src/utest_non_valgrind/**'
                                }
                            }
                        }
                        stage('Single-node-valgrind') {
                            environment {
                                CART_TEST_MODE = 'memcheck'
                            }
                            steps {
                                runTest stashes: [ 'CentOS-install', 'CentOS-build-vars' ],
                                        script: '''pwd
                                             ls -l
                                             . ./.build_vars-Linux.sh
                                             if [ ! -d $SL_PREFIX ]; then
                                                 mkdir -p ${SL_PREFIX%/Linux} || {
                                                   ls -l /var/lib/ /var/lib/jenkins || true
                                                   exit 1
                                                 }
                                                 ln -s $SL_PREFIX/install
                                             fi
                                             if bash -x utils/run_test.sh; then
                                                 echo "run_test.sh exited successfully with ${PIPESTATUS[0]}"
                                             else
                                                 echo "run_test.sh exited failure with ${PIPESTATUS[0]}"
                                             fi''',
                                    junit_files: null
                            }
                            post {
                                always {
                                    sh '''mkdir -p install/Linux/TESTING/testLogs_valgrind
                                          mv  install/Linux/TESTING/testLogs/** install/Linux/TESTING/testLogs_valgrind
                                          mkdir -p build/Linux/src/utest_valgrind
                                          mv build/Linux/src/utest/utest.log build/Linux/src/utest_valgrind
                                          mv build/Linux/src/utest/test_output build/Linux/src/utest_valgrind'''
                                    archiveArtifacts artifacts: 'install/Linux/TESTING/testLogs_valgrind/**,build/Linux/src/utest_valgrind/**'
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
    }
}
