// To use a test branch (i.e. PR) until it lands to master
// I.e. for testing library changes

pipeline {
    agent any

    environment {
        SHELL = '/bin/bash'
        TR_REDIRECT_OUTPUT = 'yes'
        GITHUB_USER = credentials('aa4ae90b-b992-4fb6-b33b-236a53a26f77')
        BAHTTPS_PROXY = "${env.HTTP_PROXY ? '--build-arg HTTP_PROXY="' + env.HTTP_PROXY + '" --build-arg http_proxy="' + env.HTTP_PROXY + '"' : ''}"
        BAHTTP_PROXY = "${env.HTTP_PROXY ? '--build-arg HTTPS_PROXY="' + env.HTTPS_PROXY + '" --build-arg https_proxy="' + env.HTTPS_PROXY + '"' : ''}"
        UID = sh(script: "id -u", returnStdout: true)
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
                            additionalBuildArgs  '--build-arg NOBUILD=1 --build-arg UID=$(id -u)'
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
            parallel {
                stage('Build on CentOS 7') {
                    agent {
                        dockerfile {
                            filename 'Dockerfile.centos:7'
                            dir 'utils/docker'
                            label 'docker_runner'
                            additionalBuildArgs  '--build-arg NOBUILD=1 --build-arg UID=$(id -u)'
                            // This caused a failure
                            //customWorkspace("/var/lib/jenkins/workspace/daos-stack-org_cart:PR-8-centos7")
                        }
                    }
                    steps {
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
                    post {
                        always {
                            recordIssues enabledForFailure: true,
                            aggregatingResults: true,
                            id: "analysis-centos7",
                            tools: [
                                [tool: [$class: 'GnuMakeGcc']],
                                [tool: [$class: 'CppCheck']],
                            ],
                            // Phyl -- I included -Linux
                            filters: [excludeFile('.*\\/_build\\.external-Linux\\/.*'),
                                     excludeFile('_build\\.external-Linux\\/.*')]
                        }
                    }
                }
            }
        }
        stage('Test') {
            parallel {
                stage('Centos Test') {
                    // Phyl -- Another difference is that Brian doesn't use a dockerfile
                    // here. He just goes native.
                    /*agent {
                        dockerfile {
                            filename 'Dockerfile.centos:7'
                            dir 'utils/docker'
                            label 'docker_runner'
                            // The last build-arg here may be a culprint
                            additionalBuildArgs  '--build-arg NOBUILD=1 --build-arg UID=$(id -u) --build-arg DONT_USE_RPMS=false'
                        }
                    }*/
                    steps {
                        runTest stashes: [ 'CentOS-install', 'CentOS-build-vars' ],
                                script: 'LD_LIBRARY_PATH=install/lib64:install/lib HOSTPREFIX=wolf-53 bash -x utils/run_test.sh --init && echo "run_test.sh exited successfully with ${PIPESTATUS[0]}" || echo "run_test.sh exited failure with ${PIPESTATUS[0]}"',
                                junit_files: null
                    }
                    post {
                        always {
                            sh '''mv build/Linux/src/utest/utest.log build/Linux/src/utest/utest.centos.7.log
                                  mv install/Linux/TESTING/testLogs install/Linux/TESTING/testLogs.centos.7'''
                            archiveArtifacts artifacts: 'build/Linux/src/utest/utest.centos.7.log', allowEmptyArchive: true
                            archiveArtifacts artifacts: 'install/Linux/TESTING/testLogs.centos.7/**', allowEmptyArchive: true
                        }
                    }
                }
            }
        }
    }
}
