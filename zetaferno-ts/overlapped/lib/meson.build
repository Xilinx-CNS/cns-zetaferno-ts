# SPDX-License-Identifier: Apache-2.0
# (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved.
overlapped_lib_sources = [
    'overlapped_lib.c',
]

overlapped_lib = static_library('overlapped_lib', overlapped_lib_sources,
                                  include_directories: [lib_dir, overlapped_lib_dir],
                                  dependencies: dep_tirpc)
