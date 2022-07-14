# SPDX-License-Identifier: Apache-2.0
# (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved.
tests = [
    'double_event',
    'events_on_freed_muxer',
    'gone_event',
    'iomux_stack_activity',
    'mod_event_recharge',
    'mod_events',
    'muxer_free_zocket',
    'muxer_in',
    'muxer_inout',
    'muxer_out',
    'muxer_set_release',
    'tcp_connect',
    'tcp_shutdown',
    'two_muxers',
    'two_zockets_del',
    'udp_out',
    'zf_waitable_event',
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

