// Fake comment
// To use a test branch (i.e. PR) until it lands to master
// I.e. for testing library changes
@Library(value="pipeline-lib@test-branch") _

pipeline {
    agent none

    environment {
        TR_REDIRECT_OUTPUT = 'yes'
        GITHUB_USER = credentials('aa4ae90b-b992-4fb6-b33b-236a53a26f77')
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
                        sconsBuild()
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
                        runTest stashes: [ 'CentOS-install', 'CentOS-build-files' ],
                                script: '''export TR_REDIRECT_OUTPUT='yes'
                                           bash utils/run_test.sh'''
                    }
                    post {
                        always {
                            sh '''mv build/Linux/src/utest/utest.log build/Linux/src/utest/utest.centos.7.log
                                  mv install/Linux/TESTING/testLogs install/Linux/TESTING/testLogs.centos.7
                                  //cd build/Linux/src/utest
                                  //for file in *.xml; do
                                  //    mv "$file" "${file%.xml}.centos.7.xml"
                                  //done'''
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
