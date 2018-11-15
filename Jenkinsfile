pipeline {
    agent none

    environment {
        SHELL = '/bin/bash'
        TR_REDIRECT_OUTPUT = 'yes'
        GITHUB_USER = credentials('aa4ae90b-b992-4fb6-b33b-236a53a26f77')
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
                            filename 'Dockerfile.ubuntu:18.04'
                            dir 'utils/docker'
                            label 'docker_runner'
                            additionalBuildArgs  '--build-arg NOBUILD=1 --build-arg UID=$(id -u)'
                        }
                    }
                    steps {
                        checkPatch user: GITHUB_USER_USR,
                                   password: GITHUB_USER_PSW
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
            failFast true
            parallel {
                stage('Build on Centos 7') {
                    agent {
                        dockerfile {
                            filename 'Dockerfile.centos:7'
                            dir 'utils/docker'
                            label 'docker_runner'
                            additionalBuildArgs  '--build-arg NOBUILD=1 --build-arg UID=$(id -u)'
                        }
                    }
                    steps {
                        //sconsBuild()
                        sh '''rm -rf *'''
                        checkout scm
                        sh '''git submodule update --init --recursive
                              scons -c
                              # scons -c is not perfect so get out the big hammer
                              rm -rf _build.external-Linux install build
                              SCONS_ARGS="--build-deps=yes --config=force USE_INSTALLED=all install"
                              if ! scons $SCONS_ARGS; then
                                  echo "$SCONS_ARGS failed"
                                  rc=\${PIPESTATUS[0]}
                                  cat config.log || true 
                                  exit \$rc
                              fi'''
                        stash name: 'Centos-install', includes: 'install/**'
                        stash name: 'Centos-build-files', includes: '.build_vars-Linux.*, cart-linux.conf, .sconsign-Linux.dblite, .sconf-temp-Linux/**'
                    }
                }
            }
        }
        /*stage('Test') {
            parallel {
                stage('Ubuntu Test') {
                    agent {
                        dockerfile {
                            filename 'Dockerfile.ubuntu:18.04'
                            dir 'utils/docker'
                            label 'docker_runner'
                            additionalBuildArgs  '--build-arg NOBUILD=1 --build-arg UID=$(id -u)'
                        }
                    }
                    steps {
                        runTest stashes: [ 'Ubuntu-install', 'Ubuntu-build-files' ],
                                script: '''bash utils/run_test.sh'''
                    }
                    post {
                        always {
                            sh '''mv build/Linux/src/utest/utest.log build/Linux/src/utest/utest.ubuntu.18.04.log
                                  mv install/Linux/TESTING/testLogs install/Linux/TESTING/testLogs.ubuntu.18.04'''
                            archiveArtifacts artifacts: 'build/Linux/src/utest/utest.ubuntu.18.04.log', allowEmptyArchive: true
                            archiveArtifacts artifacts: 'install/Linux/TESTING/testLogs.ubuntu.18.04/**', allowEmptyArchive: true
                        }
                    }
                }
            }
        }*/
    }
}
