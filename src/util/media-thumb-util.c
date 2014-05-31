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

#include "media-thumb-util.h"
#include "media-thumb-internal.h"

#include <glib.h>
#include <aul.h>
#include <string.h>
#include <drm_client.h>

int
_media_thumb_get_file_type(const char *file_full_path, int *is_drm)
{
	int ret = 0;
	drm_bool_type_e drm_type = DRM_FALSE;
	drm_file_type_e drm_file_type = DRM_TYPE_UNDEFINED;
	char mimetype[255] = {0,};

	if (file_full_path == NULL)
		return MEDIA_THUMB_ERROR_INVALID_PARAMETER;

	ret = drm_is_drm_file(file_full_path, &drm_type);
	if (ret != DRM_RETURN_SUCCESS) {
		thumb_err("drm_is_drm_file falied : %d", ret);
		drm_type = DRM_FALSE;
	}

	*is_drm = drm_type;

	if (drm_type == DRM_TRUE) {
		thumb_dbg_slog("DRM file : %s", file_full_path);

		ret = drm_get_file_type(file_full_path, &drm_file_type);
		if (ret != DRM_RETURN_SUCCESS) {
			thumb_err("drm_get_file_type falied : %d", ret);
			return THUMB_NONE_TYPE;
		}

		if (drm_file_type == DRM_TYPE_UNDEFINED) {
			return THUMB_NONE_TYPE;
		} else {
			if (drm_file_type == DRM_TYPE_OMA_V1
			|| drm_file_type == DRM_TYPE_OMA_V2
			|| drm_file_type == DRM_TYPE_OMA_PD) {
				drm_content_info_s contentInfo;
				memset(&contentInfo, 0x00, sizeof(drm_content_info_s));
	
				ret = drm_get_content_info(file_full_path, &contentInfo);
				if (ret != DRM_RETURN_SUCCESS) {
					thumb_err("drm_get_content_info() fails. : %d", ret);
					return THUMB_NONE_TYPE;
				}
				thumb_dbg("DRM mime type: %s", contentInfo.mime_type);
	
				strncpy(mimetype, contentInfo.mime_type, sizeof(mimetype) - 1);
				mimetype[sizeof(mimetype) - 1] = '\0';

				goto findtype;
			} else {
				thumb_dbg("Use aul to get mime type");
			}
		}
	}

	{
		/* get content type and mime type from file. */
		ret = aul_get_mime_from_file(file_full_path, mimetype, sizeof(mimetype));
		if (ret < 0) {
			thumb_warn
				("aul_get_mime_from_file fail.. Now trying to get type by extension");
	
			char ext[255] = { 0 };
			int ret = _media_thumb_get_file_ext(file_full_path, ext, sizeof(ext));
			if (ret < 0) {
				thumb_err("_media_thumb_get_file_ext failed");
				return THUMB_NONE_TYPE;
			}
	
			if (strcasecmp(ext, "JPG") == 0 ||
				strcasecmp(ext, "JPEG") == 0 ||
				strcasecmp(ext, "PNG") == 0 ||
				strcasecmp(ext, "GIF") == 0 ||
				strcasecmp(ext, "AGIF") == 0 ||
				strcasecmp(ext, "XWD") == 0 ||
				strcasecmp(ext, "BMP") == 0 ||
				strcasecmp(ext, "WBMP") == 0) {
				return THUMB_IMAGE_TYPE;
			} else if (strcasecmp(ext, "AVI") == 0 ||
				strcasecmp(ext, "MPEG") == 0 ||
				strcasecmp(ext, "MP4") == 0 ||
				strcasecmp(ext, "DCF") == 0 ||
				strcasecmp(ext, "WMV") == 0 ||
				strcasecmp(ext, "3GPP") == 0 ||
				strcasecmp(ext, "3GP") == 0) {
				return THUMB_VIDEO_TYPE;
			} else {
				return THUMB_NONE_TYPE;
			}
		}
	}

findtype:
	thumb_dbg("mime type : %s", mimetype);

	/* categorize from mimetype */
	if (strstr(mimetype, "image") != NULL) {
		return THUMB_IMAGE_TYPE;
	} else if (strstr(mimetype, "video") != NULL) {
		return THUMB_VIDEO_TYPE;
	}

	return THUMB_NONE_TYPE;
}

int _media_thumb_get_store_type_by_path(const char *full_path)
{
	if (full_path != NULL) {
		if (strncmp(full_path, THUMB_PATH_PHONE, strlen(THUMB_PATH_PHONE)) == 0) {
			return THUMB_PHONE;
		} else if (strncmp(full_path, THUMB_PATH_MMC, strlen(THUMB_PATH_MMC)) == 0) {
			return THUMB_MMC;
		}
	}

	return -1;
}

int _media_thumb_get_width(media_thumb_type thumb_type)
{
	if (thumb_type == MEDIA_THUMB_LARGE) {
		return THUMB_LARGE_WIDTH;
	} else if (thumb_type == MEDIA_THUMB_SMALL) {
		return  THUMB_SMALL_WIDTH;
	} else {
		return -1;
	}
}

int _media_thumb_get_height(media_thumb_type thumb_type)
{
	if (thumb_type == MEDIA_THUMB_LARGE) {
		return THUMB_LARGE_HEIGHT;
	} else if (thumb_type == MEDIA_THUMB_SMALL) {
		return  THUMB_SMALL_HEIGHT;
	} else {
		return -1;
	}
}

int _media_thumb_remove_file(const char *path)
{
	int result = -1;

	result = remove(path);
	if (result == 0) {
		thumb_dbg("success to remove file");
		return TRUE;
	} else {
		thumb_err("fail to remove file[%s] result errno = %s", path, strerror(errno));
		return FALSE;
	}
}

int _media_thumb_get_file_ext(const char *file_path, char *file_ext, int max_len)
{
	int i = 0;

	for (i = strlen(file_path); i >= 0; i--) {
		if ((file_path[i] == '.') && (i < strlen(file_path))) {
			strncpy(file_ext, &file_path[i + 1], max_len);
			return 0;
		}

		/* meet the dir. no ext */
		if (file_path[i] == '/') {
			return -1;
		}
	}

	return -1;
}

