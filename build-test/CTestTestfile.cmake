# CMake generated Testfile for 
# Source directory: /Users/sa/projects/personal/mx-kernel
# Build directory: /Users/sa/projects/personal/mx-kernel/build-test
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[kernel_tests]=] "/Users/sa/projects/personal/mx-kernel/build-test/source/projects/kernel/tests/kernel_tests")
set_tests_properties([=[kernel_tests]=] PROPERTIES  _BACKTRACE_TRIPLES "/Users/sa/projects/personal/mx-kernel/CMakeLists.txt;97;add_test;/Users/sa/projects/personal/mx-kernel/CMakeLists.txt;0;")
subdirs("source/projects/kernel")
subdirs("source/projects/kernel/tests")
