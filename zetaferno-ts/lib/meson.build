# SPDX-License-Identifier: Apache-2.0
# (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved.
sources = [
    'rpc_zf.c',
    'rpc_zf_alts.c',
    'rpc_zf_ds.c',
    'rpc_zf_internal.c',
    'rpc_zf_muxer.c',
    'rpc_zf_tcp.c',
    'rpc_zf_udp_rx.c',
    'rpc_zf_udp_tx.c',
    'zetaferno_ts.c',
    'zfts_muxer.c',
    'zfts_tcp.c',
    'zfts_zfur.c',
    'zfts_zfut.c',
    join_paths(onload_tslib_dir, 'lib-ts.c'),
    join_paths(onload_tslib_dir, 'lib-ts_netns.c'),
    join_paths(onload_tslib_dir, 'lib-ts_timestamps.c'),
]

ts_zetaferno_lib = static_library('ts_zetaferno', sources,
                                  include_directories: lib_dir,
                                  dependencies: dep_tirpc)
