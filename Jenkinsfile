// Fake comment
pipeline {
    agent none

    environment {
        SHELL = '/bin/bash'
        TR_REDIRECT_OUTPUT = 'yes'
    }
    options {
        // preserve stashes so that jobs can be started at the test stage
        preserveStashes(buildCount: 5)
    }

    stages {
        stage('Pre-build') {
            parallel {
                stage('check_modules.sh') {
                    agent {
                        dockerfile {
                            filename 'Dockerfile.centos:7'
                            dir 'utils/docker'
                            label 'docker_runner'
                            additionalBuildArgs  '--build-arg NOBUILD=1 --build-arg UID=$(id -u)'
                        }
                    }
                    steps {
                        sh '''rm -rf *
                              ls -l'''
                        checkout scm
                        sh '''pwd
                              ls -l
                              git submodule status
                              git submodule update --init --recursive
                              git submodule status
                              ls -l
                              ls -l utils
                              utils/check_modules.sh'''
                    }
                    post {
                        always {
                            archiveArtifacts artifacts: 'pylint.log', allowEmptyArchive: true
                        }
                    }
                }
            }
        }
        stage('Build') {
            parallel {
                stage('Build on CentOS 7') {
                    agent {
                        dockerfile {
                            filename 'Dockerfile.centos:7'
                            dir 'utils/docker'
                            label 'docker_runner'
                            additionalBuildArgs  '--build-arg NOBUILD=1 --build-arg UID=$(id -u)'
                        }
                    }
                    steps {
                        checkout scm
                        sh '''git submodule update --init --recursive
                              scons -c
                              # scons -c is not perfect so get out the big hammer
                              rm -rf _build.external install build
                              SCONS_ARGS="--config=force --build-deps=yes USE_INSTALLED=all install"
                              if ! scons $SCONS_ARGS; then
                                  echo "$SCONS_ARGS failed"
                                  rc=\${PIPESTATUS[0]}
                                  cat config.log || true 
                                  exit \$rc
                              fi'''
                        stash name: 'CentOS-install', includes: 'install/**'
                        stash name: 'CentOS-build-files', includes: '.build_vars-Linux.*, cart-linux.conf, .sconsign-Linux.dblite, .sconf-temp-Linux/**'
                    }
                }
            }
        }
        stage('Test') {
            parallel {
                stage('Centos Test') {
                    agent {
                        dockerfile {
                            filename 'Dockerfile.centos:7'
                            dir 'utils/docker'
                            label 'docker_runner'
                            additionalBuildArgs  '--build-arg NOBUILD=1 --build-arg UID=$(id -u) --build-arg DONT_USE_RPMS=false'
                        }
                    }
                    steps {
                        dir('install') {
                            deleteDir()
                        }
                        unstash 'CentOS-install'
                        unstash 'CentOS-build-files'
                        sh '''export TR_REDIRECT_OUTPUT='yes'
                        bash utils/run_test.sh'''
                    }
                    post {
                        always {
                            sh '''mv build/Linux/src/utest/utest.log build/Linux/src/utest/utest.centos.7.log 
                                  mv install/Linux/TESTING/testLogs install/Linux/TESTING/testLogs.centos.7
                                  //cd build/Linux/src/utest
                                  //for file in *.xml; do
                                      //mv "$file" "$(basename "$file" .xml).centos.7.xml"
                                  //done
                                  //cd -
                                  '''
                            archiveArtifacts artifacts: 'build/Linux/src/utest/utest.centos.7.log', allowEmptyArchive: true
                            archiveArtifacts artifacts: 'install/Linux/TESTING/testLogs.centos.7/**', allowEmptyArchive: true
                            //archiveArtifacts artifacts: 'build/Linux/src/utest/*.xml', allowEmptyArchive: true
                        }
                    }
                }
            }
        }
    }
}
