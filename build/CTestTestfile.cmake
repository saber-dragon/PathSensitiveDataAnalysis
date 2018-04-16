# CMake generated Testfile for 
# Source directory: /home/saber/GitHub/PathSensitiveAnalysis
# Build directory: /home/saber/GitHub/PathSensitiveAnalysis/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(pathSensitiveAnalysisTestOn/home/saber/GitHub/PathSensitiveAnalysis/build/test/cprop.ll "bash" "-c" "opt -load /home/saber/GitHub/PathSensitiveAnalysis/build/libpathSensitiveAnalysis.so -pathSensitiveAnalysis -analyze /home/saber/GitHub/PathSensitiveAnalysis/build/test/cprop.ll")
set_tests_properties(pathSensitiveAnalysisTestOn/home/saber/GitHub/PathSensitiveAnalysis/build/test/cprop.ll PROPERTIES  WORKING_DIRECTORY "/home/saber/GitHub/PathSensitiveAnalysis/build")
add_test(pathSensitiveAnalysisTestOn/home/saber/GitHub/PathSensitiveAnalysis/build/test/simplest.ll "bash" "-c" "opt -load /home/saber/GitHub/PathSensitiveAnalysis/build/libpathSensitiveAnalysis.so -pathSensitiveAnalysis -analyze /home/saber/GitHub/PathSensitiveAnalysis/build/test/simplest.ll")
set_tests_properties(pathSensitiveAnalysisTestOn/home/saber/GitHub/PathSensitiveAnalysis/build/test/simplest.ll PROPERTIES  WORKING_DIRECTORY "/home/saber/GitHub/PathSensitiveAnalysis/build")
