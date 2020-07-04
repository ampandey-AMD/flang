#
# Copyright (c) 2019, Advanced Micro Devices, Inc. All rights reserved.
#
# Support for assumed size array as parameter
#

# Shared lit script for each tests. Run bash commands that run tests with make.

# XFAIL: *
# RUN: KEEP_FILES=%keep FLAGS=%flags TEST_SRC=%s MAKE_FILE_DIR=%S/.. bash %S/runmake | tee %t
# RUN: cat %t | FileCheck %S/runmake
