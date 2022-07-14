# SPDX-License-Identifier: Apache-2.0
# (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved.
sources = [
    'zf_ds_test.c',
]

zf_ds_lib = static_library('zf_ds_test', sources,
                           include_directories: [lib_dir, zf_ds_lib_dir],
                           dependencies: dep_tirpc)
