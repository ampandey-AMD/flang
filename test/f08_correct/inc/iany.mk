#
# Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

#
# Copyright (c) 2019, Advanced Micro Devices, Inc. All rights reserved.
#
# Support for iany intrinsic.
#

########## Make rule for test iany  ########


iany: .run

iany.$(OBJX):  $(SRC)/iany.f08
	-$(RM) iany.$(EXESUFFIX) core *.d *.mod FOR*.DAT FTN* ftn* fort.*
	@echo ------------------------------------ building test $@
	-$(CC) -c $(CFLAGS) $(SRC)/check.c -o check.$(OBJX)
	-$(FC) -c $(FFLAGS) $(LDFLAGS) $(SRC)/iany.f08 -o iany.$(OBJX)
	-$(FC) $(FFLAGS) $(LDFLAGS) iany.$(OBJX) check.$(OBJX) $(LIBS) -o iany.$(EXESUFFIX)


iany.run: iany.$(OBJX)
	@echo ------------------------------------ executing test iany
	iany.$(EXESUFFIX)

build:	iany.$(OBJX)

verify:	;

run:	 iany.$(OBJX)
	@echo ------------------------------------ executing test iany
	iany.$(EXESUFFIX)

