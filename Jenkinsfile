pipeline {
    agent any

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
                        stash name: 'Centos-install', includes: 'install/**'
                        stash name: 'Centos-build-files', includes: '.build_vars-Linux.*, cart-linux.conf, .sconsign-Linux.dblite, .sconf-temp-Linux/**'
                    }
                }
            }
        }
    }
}
