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

/**
* @file 	utc_thumbnail_request_save_to_file_func.c
* @brief 	This is a suit of unit test cases to test thumbnail_request_save_to_file API function
* @author
* @version 	Initial Creation Version 0.1
* @date 	2011-10-13
*/

#include "utc_thumbnail_request_save_to_file_func.h"


/**
* @brief	This tests int thumbnail_request_save_to_file() API with valid parameters
* @par ID	utc_thumbnail_request_save_to_file_func_01
* @param	[in] 
* @return	This function returns zero on success, or negative value with error code
*/
void utc_thumbnail_request_save_to_file_func_01()
{
	int ret = -1;
	 
	const char *origin_path = "/opt/media/Images/Wallpapers/Home_default.jpg";
	char thumb_path[255] = { 0, };
	snprintf(thumb_path, sizeof(thumb_path), "/tmp/test_thumb.jpg");

	media_thumb_type thumb_type = MEDIA_THUMB_LARGE;

	ret = thumbnail_request_save_to_file(origin_path, thumb_type, thumb_path);
	
	if (ret < MEDIA_THUMB_ERROR_NONE) {
		UTC_THUMB_LOG( "unable to save thumbnail file. error code->%d", ret);
		tet_result(TET_FAIL);
		return;
	} else {
		unlink(thumb_path);
		tet_result(TET_PASS);
	}
	
	return;
}


/**
* @brief 	This tests int thumbnail_request_save_to_file() API with invalid parameters
* @par ID	utc_thumbnail_request_save_to_file_func_02
* @param	[in] 
* @return	error code on success 
*/
void utc_thumbnail_request_save_to_file_func_02()
{	
	int ret = -1;

	const char *origin_path = NULL;
	char thumb_path[255] = { 0, };

	media_thumb_type thumb_type = MEDIA_THUMB_LARGE;

	ret = thumbnail_request_save_to_file(origin_path, thumb_type, thumb_path);
	if (ret < MEDIA_THUMB_ERROR_NONE) {
		UTC_THUMB_LOG("abnormal condition test for null, error code->%d", ret);
		tet_result(TET_PASS);
	} else {
		UTC_THUMB_LOG("Creating thumbnail file should be failed because the origin_path is NULL");
		tet_result(TET_FAIL);
	}

	return ;
}
