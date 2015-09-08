/*
 * libmedia-thumbnail
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Hyunjun Ko <zzoon.ko@samsung.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include "media-thumb-error.h"
#include "media-thumb-types.h"
#include "media-thumb-debug.h"

#ifndef _MEDIA_THUMB_UTIL_H_
#define _MEDIA_THUMB_UTIL_H_

#define SAFE_FREE(src)      { if(src) {free(src); src = NULL;}}

int
_media_thumb_get_store_type_by_path(const char *full_path);

int
_media_thumb_get_file_ext(const char *file_path, char *file_ext, int max_len);

int
_media_thumb_get_file_type(const char *file_full_path, int *is_drm);

char*
_media_thumb_generate_hash_name(const char *file);

int
_media_thumb_get_hash_name(const char *file_full_path,
				 char *thumb_hash_path, size_t max_thumb_path);


int
_media_thumb_get_width(media_thumb_type thumb_type);

int
_media_thumb_get_height(media_thumb_type thumb_type);

int
_media_thumb_remove_file(const char *path);

#endif /*_MEDIA_THUMB_UTIL_H_*/

