
/*
 * Copyright (c) 2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */


#ifndef __SIN_D_AVX2_128_H__
#define __SIN_D_AVX2_128_H__

#include <immintrin.h>
#include "common_sin.h"
#define CONFIG 1
#include "helperavx2_128.h"
#include "sin_d_vec.h"

extern "C" vdouble __attribute__ ((noinline)) __fd_sin_2_avx2(vdouble const a);

vdouble __attribute__ ((noinline))
__fd_sin_2_avx2(vdouble const a)
{
	return __sin_d_vec(a);
}

#endif // __SIN_D_AVX2_128_H__

