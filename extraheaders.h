/***************************************************************************
 * Interface declarations for the extra header routines in extraheaders.c
 *
 * This file is part of the miniSEED Library.
 *
 * Copyright (c) 2024 Chad Trabant, EarthScope Data Services
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ***************************************************************************/

#ifndef EXTRAHEADERS_H
#define EXTRAHEADERS_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include "libmseed.h"
#include "yyjson.h"

/* Avoid unused parameter warnings in known cases */
#define UNUSED(x) (void)(x)

/* Private allocation wrappers for yyjson's allocator definition */
void *_priv_malloc(void *ctx, size_t size);
void *_priv_realloc(void *ctx, void *ptr, size_t oldsize, size_t size);
void _priv_free(void *ctx, void *ptr);

/* Internal structure for holding parsed JSON extra headers */
struct LM_PARSED_JSON_s
{
  yyjson_doc *doc;
  yyjson_mut_doc *mut_doc;
};

#ifdef __cplusplus
}
#endif

#endif
