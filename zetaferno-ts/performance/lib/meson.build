# SPDX-License-Identifier: Apache-2.0
# (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved.
performance_lib_sources = [
    'performance_lib.c',
]

performance_lib = static_library('performance_lib', performance_lib_sources,
                                  include_directories: [lib_dir, performance_lib_dir],
                                  dependencies: dep_tirpc)
