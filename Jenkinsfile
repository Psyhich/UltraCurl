pipeline {
	agent any
	environment {
		GIT_URL = 'https://github.com/Psyhich/UltraCurl'
	}
	stages {
		stage('SCM') {
			steps { 
				checkout scm
				sh 'git submodule update --init --recursive'
			}
		}
		
		stage("Build") {
			steps {
				dir("build") {
					sh 'cmake ../ -DCMAKE_BUILD_TYPE=Debug'
					sh 'make tests'
				}
			}
		}
		stage("Test") {
			steps {
				dir("build") {
					sh 'ctest'
				}
			}
		}
		stage("Check coverage"){
			steps{
				dir("build") {
					sh 'make tests_coverage'
					cobertura autoUpdateHealth: false, autoUpdateStability: false, coberturaReportFile: 'cobertura.xml', conditionalCoverageTargets: '70, 80, 80', lineCoverageTargets: '80, 80, 80', maxNumberOfBuilds: 0, methodCoverageTargets: '80, 80, 80', onlyStable: false, sourceEncoding: 'ASCII', zoomCoverageChart: true
				}
			}

		}
		stage("Build main"){
			steps {
				dir("build") {
					sh 'make main'
				}
				script {
					currentBuild.result = 'SUCCESS'
				}
			}
		}
	}
	post {
		always {
			dir("build"){
				publishCoverage adapters: [coberturaAdapter('cobertura.xml')], sourceFileResolver: sourceFiles('NEVER_STORE')
			}
			step([$class: 'MasterCoverageAction', scmVars: [GIT_URL: env.GIT_URL]])
			step([$class: 'CompareCoverageAction', publishResultAs: 'statusCheck', skipPublishingChecks: true, scmVars: [GIT_URL: env.GIT_URL]])
		}
	}
}
