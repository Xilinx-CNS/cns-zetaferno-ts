# SPDX-License-Identifier: Apache-2.0
# (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved.
performance_lib_dir = include_directories('lib')

subdir('lib')

performance_test_deps = test_deps
performance_test_deps += declare_dependency(
                              include_directories: performance_lib_dir,
                              link_with: performance_lib)

tests = [
    'altpingpong',
    'prologue',
    'tcppingpong',
    'udppingpong',
]

foreach test : tests
    test_exe = test
    test_c = test + '.c'
    package_tests_c += [ test_c ]
    executable(test_exe, test_c, install: true, install_dir: package_dir,
               dependencies: [performance_test_deps])
endforeach

tests_info_xml = custom_target(package_dir.underscorify() + 'tests-info-xml',
                               install: true, install_dir: package_dir,
                               input: package_tests_c,
                               output: 'tests-info.xml', capture: true,
                               command: [ te_tests_info_sh,
                                          meson.current_source_dir() ])

install_data([ 'package.dox', 'package.xml' ],
             install_dir: package_dir)
