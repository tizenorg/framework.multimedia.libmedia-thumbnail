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


#ifndef _MEDIA_THUMBNAIL_PRIVATE_H_
#define _MEDIA_THUMBNAIL_PRIVATE_H_

#include "media-thumb-error.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 *	thumbnail_generate_hash_code :
 * 	This function generates a hash code using its original path by md5 algorithm. 
 * 	If done, the code will be returned.
 *
 *	@return		This function returns zero(MEDIA_THUMB_ERROR_NONE) on success, or negative value with error code.
 *				Please refer 'media-thumb-error.h' to know the exact meaning of the error.
 *  @param[in]				origin_path      The path of the original media
 *  @param[out]				hash_code       The hash code generated by md5 algorithm.
 *  @param[in]				max_length       The max length of the returned hash code.
 *	@see		None.
 *	@pre		None.
 *	@post		None.
 *	@remark		None.
 *	@par example
 *	@code

#include <media-thumbnail.h>

void gen_thumb_hash_code()
{
	int ret = MEDIA_THUMB_ERROR_NONE;
	const char *origin_path = "/opt/usr/media/test.jpg";
	char hashcode[255];

	ret = thumbnail_generate_hash_code(origin_path, hashcode, 255);

	if (ret < 0) {
		printf( "thumbnail_generate_hash_code fails. error code->%d", ret);
	}

	return;
}

 * 	@endcode
 */
int thumbnail_generate_hash_code(const char *origin_path, char *hash_code, int max_length);


#ifdef __cplusplus
}
#endif

#endif /*_MEDIA_THUMBNAIL_PRIVATE_H_*/

