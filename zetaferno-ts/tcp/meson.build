# SPDX-License-Identifier: Apache-2.0
# (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved.
tests = [
    'bind_address_types',
    'bind_twice',
    'connect_after_fault',
    'delayed_ack',
    'endpoints_limit',
    'incomplete_connect',
    'listen_address_types',
    'listen_backlog',
    'listen_reuse_laddr',
    'listen_share_stack',
    'listeners_limit',
    'locked_rx_buffers',
    'msg_more',
    'reactor_recv_event',
    'recv_buffer_overfilling',
    'send_buffer_overfilling',
    'share_events_queue',
    'sleep_after_stack_alloc',
    'tcp_active',
    'tcp_passive',
    'two_active_zockets',
    'unaccepted_endpoints_limit',
    'wait_for_time_wait',
    'zc_read_buffers',
    'zc_recv_done_some',
    'zft_handle',
    'zft_send_buffers',
    'zft_send_space',
    'zockets_release',
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

