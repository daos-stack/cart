// To use a test branch (i.e. PR) until it lands to master
// I.e. for testing library changes
@Library(value="pipeline-lib@debug") _

def singleNodeTest(nodelist) {
    echo "Running singleNodeTest on " + nodelist
    provisionNodes NODELIST: nodelist,
                   node_count: 1,
                   snapshot: true
    runTest script: """pwd
                       ls -l
                       """,
          junit_files: null
}

def arch="-Linux"

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
        stage('Test') {
            parallel {
                stage('Single-node') {
                    agent {
                        label 'ci_vm1'
                    }
                    environment {
                        CART_TEST_MODE = 'native'
                    }
                    steps {
                        sh 'env'
                        singleNodeTest(env.NODELIST)
                    }
                }
                stage('Five-node') {
                    agent {
                        label 'ci_vm5'
                    }
                    steps {
                        sh 'env'
                        echo "Starting Five-node"
                        provisionNodes NODELIST: env.NODELIST,
                                       node_count: 5,
                                       snapshot: true
                        checkoutScm url: 'ssh://review.hpdd.intel.com:29418/exascale/jenkins',
                                    checkoutDir: 'jenkins',
                                    credentialsId: 'bf21c68b-9107-4a38-8077-e929e644996a'

                        checkoutScm url: 'ssh://review.hpdd.intel.com:29418/coral/scony_python-junit',
                                    checkoutDir: 'scony_python-junit',
                                    credentialsId: 'bf21c68b-9107-4a38-8077-e929e644996a'

                        echo "Starting Five-node runTest"
                        sh 'pdsh -R ssh -S -w ' + env.NODELIST +
                           '"set -x; pdsh -R ssh -S -w ' + env.NODELIST +
                           'id | dshbak -c" | dshbak -c'
                    }
                }
            }
        }
    }
}
