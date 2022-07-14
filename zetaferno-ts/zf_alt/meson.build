# SPDX-License-Identifier: Apache-2.0
# (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved.
tests = [
    'alt_overfill',
    'alt_overfill_param',
    'alt_release',
    'alt_reuse',
    'fill_bufs',
    'invalid_alt_attr_bufs',
    'many_zft',
    'no_ack',
    'no_extra_data',
    'peer_close',
    'prologue',
    'queue_mult',
    'zft_alt_sample',
    'zft_alt_send',
    'zft_shutdown',
    'zft_tst_send',
    'zft_mixed_send',
]

foreach test : tests
    test_exe = test
    test_c = test + '.c'
    package_tests_c += [ test_c ]
    executable(test_exe, test_c, install: true, install_dir: package_dir,
               dependencies: test_deps)
endforeach

tests_info_xml = custom_target(package_dir.underscorify() + 'tests-info-xml',
                               install: true, install_dir: package_dir,
                               input: package_tests_c,
                               output: 'tests-info.xml', capture: true,
                               command: [ te_tests_info_sh,
                               meson.current_source_dir() ])

install_data([ 'package.dox', 'package.xml' ],
             install_dir: package_dir)

