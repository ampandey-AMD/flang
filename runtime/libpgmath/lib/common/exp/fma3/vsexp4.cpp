/* 
 * Copyright (c) 2017-2018, NVIDIA CORPORATION.  All rights reserved.
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
#if defined(TARGET_LINUX_POWER)
#include "xmm2altivec.h"
#else
#include <immintrin.h>
#endif


#include "exp_defs.h"

extern "C" __m128 __fvs_exp_fma3(__m128);
__m128 __pgm_exp_vec128_slowpath(__m128, __m128i, __m128);

__m128 __fvs_exp_fma3(__m128 a)
{
    __m128 const EXP_HI_VEC = _mm_set1_ps(EXP_HI);
    __m128 const EXP_LO_VEC = _mm_set1_ps(EXP_LO);
    __m128 const EXP_PDN_VEC = _mm_set1_ps(EXP_PDN);
    __m128 const FLT2INT_CVT_VEC = _mm_set1_ps(FLT2INT_CVT);
    __m128 const L2E_VEC = _mm_set1_ps(L2E);
   
    __m128 const SGN_VEC = (__m128)_mm_set1_epi32(MASK);

    __m128 abs = _mm_and_ps(a, SGN_VEC);
    __m128i sp_mask = _mm_cmpgt_epi32(_mm_castps_si128(abs), _mm_castps_si128(EXP_PDN_VEC)); // zero dla dobrych
#if defined(TARGET_LINUX_POWER)
    int sp = _vec_any_nz(sp_mask);
#else
    int sp = _mm_movemask_epi8(sp_mask);
#endif
    __m128 t = _mm_fmadd_ps(a, L2E_VEC, FLT2INT_CVT_VEC);
    __m128 tt = _mm_sub_ps(t, FLT2INT_CVT_VEC);
    __m128 z = _mm_fnmadd_ps(tt, _mm_set1_ps(LN2_0), a);
           z = _mm_fnmadd_ps(tt, _mm_set1_ps(LN2_1), z);
         
    __m128i exp = _mm_castps_si128(t);
            exp = _mm_slli_epi32(exp, 23);

    __m128 zz =                 _mm_set1_ps(EXP_C7);
    zz = _mm_fmadd_ps(zz, z, _mm_set1_ps(EXP_C6));
    zz = _mm_fmadd_ps(zz, z, _mm_set1_ps(EXP_C5));
    zz = _mm_fmadd_ps(zz, z, _mm_set1_ps(EXP_C4));
    zz = _mm_fmadd_ps(zz, z, _mm_set1_ps(EXP_C3));
    zz = _mm_fmadd_ps(zz, z, _mm_set1_ps(EXP_C2));
    zz = _mm_fmadd_ps(zz, z, _mm_set1_ps(EXP_C1));
    zz = _mm_fmadd_ps(zz, z, _mm_set1_ps(EXP_C0));
    __m128 res = (__m128)_mm_add_epi32(exp, (__m128i)zz);
 
    if (sp)
    {
        res = __pgm_exp_vec128_slowpath(a, exp, zz);       
    }

    return res;
}


//__m128 __pgm_exp_vec128_slowpath(__m128 a, __m128i exp, __m128 zz) __attribute__((noinline));
__m128 __pgm_exp_vec128_slowpath(__m128 a, __m128i exp, __m128 zz) {
    __m128 const EXP_HI_VEC = _mm_set1_ps(EXP_HI);
    __m128 const EXP_LO_VEC = _mm_set1_ps(EXP_LO);
    __m128i const DNRM_THR_VEC = _mm_set1_epi32(DNRM_THR);
    __m128i const EXP_BIAS_VEC = _mm_set1_epi32(EXP_BIAS);
    __m128i const DNRM_SHFT_VEC = _mm_set1_epi32(DNRM_SHFT);   
    __m128 const INF_VEC = (__m128)_mm_set1_epi32(INF);
    
    __m128 inf_mask = _mm_cmp_ps(a, EXP_HI_VEC, _CMP_LT_OS);
    __m128 zero_mask = _mm_cmp_ps(a, EXP_LO_VEC, _CMP_GT_OS);
    __m128 nan_mask = _mm_cmp_ps(a, a, _CMP_NEQ_UQ);
    //ORIG __m128 nan_mask = _mm_cmp_ps(a, a, 4);
    __m128 inf_vec = _mm_andnot_ps(inf_mask, INF_VEC);
    __m128 nan_vec = _mm_and_ps(a, nan_mask); 
    __m128 fix_mask = _mm_xor_ps(zero_mask, inf_mask); 

    __m128i dnrm = _mm_min_epi32(exp, DNRM_THR_VEC);
            dnrm = _mm_add_epi32(dnrm, DNRM_SHFT_VEC);
            exp = _mm_max_epi32(exp, DNRM_THR_VEC);
    __m128 res = (__m128)_mm_add_epi32(exp, (__m128i)zz);
    res = _mm_fmadd_ps((__m128)dnrm, res, nan_vec);
    res = _mm_blendv_ps(res, inf_vec, fix_mask);

    return res;
}
