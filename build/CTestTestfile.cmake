# CMake generated Testfile for 
# Source directory: E:/project/oh-my-skills
# Build directory: E:/project/oh-my-skills/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
if(CTEST_CONFIGURATION_TYPE MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
  add_test([=[skillctl_tests]=] "E:/project/oh-my-skills/build/Debug/skillctl_tests.exe")
  set_tests_properties([=[skillctl_tests]=] PROPERTIES  _BACKTRACE_TRIPLES "E:/project/oh-my-skills/CMakeLists.txt;63;add_test;E:/project/oh-my-skills/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
  add_test([=[skillctl_tests]=] "E:/project/oh-my-skills/build/Release/skillctl_tests.exe")
  set_tests_properties([=[skillctl_tests]=] PROPERTIES  _BACKTRACE_TRIPLES "E:/project/oh-my-skills/CMakeLists.txt;63;add_test;E:/project/oh-my-skills/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
  add_test([=[skillctl_tests]=] "E:/project/oh-my-skills/build/MinSizeRel/skillctl_tests.exe")
  set_tests_properties([=[skillctl_tests]=] PROPERTIES  _BACKTRACE_TRIPLES "E:/project/oh-my-skills/CMakeLists.txt;63;add_test;E:/project/oh-my-skills/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
  add_test([=[skillctl_tests]=] "E:/project/oh-my-skills/build/RelWithDebInfo/skillctl_tests.exe")
  set_tests_properties([=[skillctl_tests]=] PROPERTIES  _BACKTRACE_TRIPLES "E:/project/oh-my-skills/CMakeLists.txt;63;add_test;E:/project/oh-my-skills/CMakeLists.txt;0;")
else()
  add_test([=[skillctl_tests]=] NOT_AVAILABLE)
endif()
