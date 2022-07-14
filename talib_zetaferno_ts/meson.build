# SPDX-License-Identifier: Apache-2.0
# (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved.
t = 'struct mmsghdr'
if cc.has_type(t, prefix: '#include <sys/socket.h>')
    conf.set('HAVE_' + t.to_upper().underscorify(), 1)
endif

check_headers = [
    'asm-generic/errno.h',
]
foreach h : check_headers
    if cc.has_header(h)
        c_args += [ '-DHAVE_' + h.to_upper().underscorify() ]
    endif
endforeach

includes += include_directories('include')

sources += files(
    'rpc.c',
    'rpc_alts.c',
    'rpc_ds.c',
    'rpc_muxer.c',
    'rpc_tcp.c',
    'rpc_udp_rx.c',
    'rpc_udp_tx.c',
)

build_lib = 'static'

te_libs += [
    'loggerta',
    'rpcserver',
]
