# SPDX-License-Identifier: Apache-2.0
# (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved.
overlapped_lib_dir = include_directories('lib')

subdir('lib')

overlapped_test_deps = test_deps
overlapped_test_deps += declare_dependency(include_directories: overlapped_lib_dir,
                                            link_with: overlapped_lib)

tests = [
    'recv_pftf',
    'recv_pftf_bad_csum',
    'recv_pftf_two_muxer',
]

foreach test : tests
    test_exe = test
    test_c = test + '.c'
    package_tests_c += [ test_c ]
    executable(test_exe, test_c, install: true, install_dir: package_dir,
               dependencies: [test_deps,  overlapped_test_deps])
endforeach

tests_info_xml = custom_target(package_dir.underscorify() + 'tests-info-xml',
                               install: true, install_dir: package_dir,
                               input: package_tests_c,
                               output: 'tests-info.xml', capture: true,
                               command: [ te_tests_info_sh,
                               meson.current_source_dir() ])

install_data([ 'package.dox', 'package.xml' ],
             install_dir: package_dir)

