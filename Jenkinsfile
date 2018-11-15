pipeline {
    agent none

    environment {
        SHELL = '/bin/bash'
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
                            additionalBuildArgs  '--build-arg NOBUILD=1 --build-arg UID=$(id -u) --build-arg DONT_USE_RPMS=false'
                        }
                    }
                    steps {
                        sh '''git submodule update --init --recursive
                              utils/check_modules.sh'''
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
                        //githubNotify description: 'CentOS 7 Build',  context: 'build/centos7', status: 'PENDING'
                        checkout scm
                        sh '''git submodule update --init --recursive
                              scons -c
                              # scons -c is not perfect so get out the big hammer
                              rm -rf _build.external-Linux install build
                              SCONS_ARGS="--build-deps=yes --config=force install"
                              if ! scons $SCONS_ARGS; then
                                  echo "$SCONS_ARGS failed"
                                  rc=\${PIPESTATUS[0]}
                                  cat config.log || true 
                                  exit \$rc
                              fi'''
                        stash name: 'CentOS-install', includes: 'install/**'
                        stash name: 'CentOS-build-files', includes: '.build_vars-Linux.*, cart-linux.conf, .sconsign-Linux.dblite, .sconf-temp-Linux/**'
                    }
                    /*post {
                        success {
                            //githubNotify description: 'CentOS 7 Build',  context: 'build/centos7', status: 'SUCCESS'
                            sh '''echo "Success" '''
                        }
                        unstable {
                            //githubNotify description: 'CentOS 7 Build',  context: 'build/centos7', status: 'FAILURE'
                            sh '''echo "Failure" '''
                        }
                    }*/
                }
            }
        }
        stage('Test') {
            parallel {
                stage('run_test.sh') {
                    agent {
                        dockerfile {
                            filename 'Dockerfile.centos:7'
                            dir 'utils/docker'
                            label 'docker_runner'
                            additionalBuildArgs  '--build-arg NOBUILD=1 --build-arg UID=$(id -u) --build-arg DONT_USE_RPMS=false'
                        }
                    }
                    steps {
                        //githubNotify description: 'run_test.sh',  context: 'test/run_test.sh', status: 'PENDING'
                        dir('install') {
                            deleteDir()
                        }
                        unstash 'CentOS-install'
                        unstash 'CentOS-build-files'
                        sh '''bash utils/run_test.sh'''
                    }
                    /*post {
                        always {
                            archiveArtifacts artifacts: 'install/Linux/TESTING/testLogs/**,build/Linux/src/utest/utest.log,build/Linux/src/utest/test_output', allowEmptyArchive: true
                        }
                        success {
                            //githubNotify description: 'run_test.sh',  context: 'test/run_test.sh', status: 'SUCCESS'
                            sh '''echo "Success" '''
                        }
                        unstable {
                            //githubNotify description: 'run_test.sh',  context: 'test/run_test.sh', status: 'FAILURE'
                            sh '''echo "Failure" '''
                        }
                    }*/
                }
            } 
        }    

    }
}
