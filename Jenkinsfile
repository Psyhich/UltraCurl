pipeline {
	agent any
	environment{
		GIT_URL = 'https://github.com/Psyhich/My_String'
	}
	stages {
		stage('SCM') {
			steps { 
				checkout scm
			}
		}
		
		stage("Build") {
			steps {
				dir("Exercise/build") {
					sh 'cmake ../ -DCMAKE_BUILD_TYPE=Debug'
					sh 'make tests'
				}
			}
		}
		stage("Test") {
			steps {
				dir("Exercise/build") {
					sh 'ctest'
				}
			}
		}
		stage("Check coverage"){
			steps{
				dir("Exercise/build") {
					sh 'make tests_coverage'
					cobertura autoUpdateHealth: false, autoUpdateStability: false, coberturaReportFile: 'cobertura.xml', conditionalCoverageTargets: '70, 80, 80', lineCoverageTargets: '80, 80, 80', maxNumberOfBuilds: 0, methodCoverageTargets: '80, 80, 80', onlyStable: false, sourceEncoding: 'ASCII', zoomCoverageChart: false
				}
			}

		}
		stage("Build main"){
			steps {
				dir("Exercise/build") {
					sh 'make main'
				}
				script {
					currentBuild.result = 'SUCCESS'
				}
			}
		}
		stage("Publish to GitHub") {
			steps{
				dir("Exercise/build"){
					publishCoverage adapters: [coberturaAdapter('cobertura.xml')], sourceFileResolver: sourceFiles('NEVER_STORE')
				}
				step([$class: 'MasterCoverageAction', scmVars: [GIT_URL: env.GIT_URL]])
				step([$class: 'CompareCoverageAction', publishResultAs: 'statusCheck', skipPublishingChecks: true, scmVars: [GIT_URL: env.GIT_URL]])
			}
		}
	}
}
