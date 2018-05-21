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

#ifndef SEMUTIL0_H_
#define SEMUTIL0_H_

#include "gbldefs.h"

/**
   \brief ...
 */
bool sem_eq_str(int con, char *pattern);

/**
   \brief ...
 */
INT cngcon(INT oldval, int oldtyp, int newtyp);

/**
   \brief ...
 */
int getrval(int ilmptr);

/**
   \brief ...
 */
void semant_init(void);

/**
   \brief ...
 */
void semant_reinit(void);

#endif // SEMUTIL0_H_
