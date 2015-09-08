/*
 * media-thumbnail-server
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

#include "thumb-server-internal.h"
#include "media-thumb-util.h"

#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <Ecore_Evas.h>
#include <dd-display.h>
#ifdef LOG_TAG
#undef LOG_TAG
#endif
#include <dbus/dbus-glib.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

#define LOG_TAG "MEDIA_THUMBNAIL_SERVER"

static __thread char **arr_path;
static __thread int g_idx = 0;
static __thread int g_cur_idx = 0;

GMainLoop *g_thumb_server_mainloop; // defined in thumb-server.c as extern

guint g_source_id;
bool g_extract_all_status;

static gboolean __thumb_server_send_msg_to_agent(int msg_type);
static void __thumb_daemon_stop_job(void);
static int __thumb_daemon_all_extract(void);
int _thumb_daemon_process_queue_jobs(gpointer data);

gboolean _thumb_daemon_start_jobs(gpointer data)
{
	thumb_dbg("");
	/* Initialize ecore-evas to use evas library */
	ecore_evas_init();

	__thumb_server_send_msg_to_agent(MS_MSG_THUMB_SERVER_READY);

	return FALSE;
}

void _thumb_daemon_finish_jobs(void)
{
	sqlite3 *sqlite_db_handle = _media_thumb_db_get_handle();

	if (sqlite_db_handle != NULL) {
		_media_thumb_db_disconnect();
		thumb_dbg("sqlite3 handle is alive. So disconnect to sqlite3");
	}

	/* Shutdown ecore-evas */
	ecore_evas_shutdown();

	return;
}

static int __thumb_daemon_mmc_status(void)
{
	int err = -1;
	int status = -1;

	err = vconf_get_int(VCONFKEY_SYSMAN_MMC_STATUS, &status);
	if (err == 0) {
		return status;
	} else if (err == -1) {
		thumb_err("vconf_get_int failed : %d", err);
	} else {
		thumb_err("vconf_get_int Unexpected error code: %d", err);
	}

	return status;
}

void _thumb_daemon_power_off_cb(void* data)
{
	int err = -1;
	int status = 0;

	thumb_warn("_thumb_daemon_power_off_cb called");

	err = vconf_get_int(VCONFKEY_SYSMAN_POWER_OFF_STATUS, &status);
	if (err == 0)
	{
		if (status == VCONFKEY_SYSMAN_POWER_OFF_DIRECT || status == VCONFKEY_SYSMAN_POWER_OFF_RESTART)
		{
			if (g_thumb_server_mainloop)
				g_main_loop_quit(g_thumb_server_mainloop);
			else
				return;
		}
	}
	else
	{
		thumb_err("vconf_get_int failed : %d", err);
	}

	return;
}

void _thumb_daemon_mmc_eject_vconf_cb(keynode_t *key, void* data)
{
	int err = -1;
	int status = 0;

	thumb_warn("_thumb_daemon_mmc_eject_vconf_cb called");

	err = vconf_get_int(VCONFKEY_SYSMAN_MMC_STATUS, &status);
	if (err == 0) {
		if (status == VCONFKEY_SYSMAN_MMC_REMOVED || status == VCONFKEY_SYSMAN_MMC_INSERTED_NOT_MOUNTED) {
			thumb_warn("SD card is ejected or not mounted. So media-thumbnail-server stops jobs to extract all thumbnails");

			__thumb_daemon_stop_job();
		}
	} else if (err == -1) {
		thumb_err("vconf_get_int failed : %d", err);
	} else {
		thumb_err("vconf_get_int Unexpected error code: %d", err);
	}

	return;
}

void _thumb_daemon_vconf_cb(keynode_t *key, void* data)
{
	int err = -1;
	int status = 0;

	thumb_warn("_thumb_daemon_vconf_cb called");

	err = vconf_get_int(VCONFKEY_SYSMAN_MMC_FORMAT, &status);
	if (err == 0) {
		if (status == VCONFKEY_SYSMAN_MMC_FORMAT_COMPLETED) {
			thumb_warn("SD card format is completed. So media-thumbnail-server stops jobs to extract all thumbnails");

			__thumb_daemon_stop_job();
		} else {
			thumb_dbg("not completed");
		}
	} else if (err == -1) {
		thumb_err("vconf_get_int failed : %d", err);
	} else {
		thumb_err("vconf_get_int Unexpected error code: %d", err);
	}

	return;
}

void _thumb_daemon_camera_vconf_cb(keynode_t *key, void* data)
{
	int err = -1;
	int camera_state = -1;

	err = vconf_get_int(VCONFKEY_CAMERA_STATE, &camera_state);
	if (err == 0) {
		thumb_dbg("camera state: %d", camera_state);
	} else {
		thumb_err("vconf_get_int failed: %d", err);
		return;
	}

	if (camera_state >= VCONFKEY_CAMERA_STATE_PREVIEW) {
		thumb_warn("CAMERA is running. Do not create thumbnail");

		if (g_source_id > 0) {
			__thumb_daemon_stop_job();
			g_source_remove(g_source_id);
			g_source_id = 0;
		}
		thumb_warn("q_source_id = %d", g_source_id);
	} else {
		if (g_extract_all_status == true) {
			/*add queue job again*/
			thumb_warn("q_source_id = %d", g_source_id);
			if (g_source_id == 0) {
				__thumb_daemon_all_extract();
				g_source_id = g_idle_add(_thumb_daemon_process_queue_jobs, NULL);
			}
		}
	}

	return;
}

static void __thumb_daemon_stop_job()
{
	int i = 0;
	char *path = NULL;

	thumb_warn("There are %d jobs in the queue. But all jobs will be stopped", g_idx - g_cur_idx);

	for (i = g_cur_idx; i < g_idx; i++) {
		path = arr_path[g_cur_idx++];
		SAFE_FREE(path);
	}

	return;
}

static int __thumb_daemon_process_job(thumbMsg *req_msg, thumbMsg *res_msg)
{
	int err = -1;

	err = _media_thumb_process(req_msg, res_msg);
	if (err < 0) {
		if (req_msg->msg_type == THUMB_REQUEST_SAVE_FILE) {
			thumb_err("_media_thumb_process is failed: %d", err);
			res_msg->status = THUMB_FAIL;
		} else {
			if (err != MEDIA_THUMB_ERROR_FILE_NOT_EXIST) {
				thumb_warn("_media_thumb_process is failed: %d, So use default thumb", err);
				res_msg->status = THUMB_SUCCESS;
			} else {
				thumb_warn("_media_thumb_process is failed: %d, (file not exist) ", err);
				res_msg->status = THUMB_FAIL;
			}
		}
	} else {
		res_msg->status = THUMB_SUCCESS;
	}

	return err;
}

static int __thumb_daemon_all_extract(void)
{
	int err = -1;
	char query_string[MAX_PATH_SIZE + 1] = { 0, };
	char path[MAX_PATH_SIZE + 1] = { 0, };
	sqlite3 *sqlite_db_handle = NULL;
	sqlite3_stmt *sqlite_stmt = NULL;

	err = _media_thumb_db_connect();
	if (err < 0) {
		thumb_err("_media_thumb_db_connect failed: %d", err);
		return MEDIA_THUMB_ERROR_DB;
	}

	sqlite_db_handle = _media_thumb_db_get_handle();
	if (sqlite_db_handle == NULL) {
		thumb_err("sqlite handle is NULL");
		return MEDIA_THUMB_ERROR_DB;
	}

	if (__thumb_daemon_mmc_status() == VCONFKEY_SYSMAN_MMC_MOUNTED) {
		snprintf(query_string, sizeof(query_string), SELECT_PATH_FROM_UNEXTRACTED_THUMB_MEDIA);
	} else {
		snprintf(query_string, sizeof(query_string), SELECT_PATH_FROM_UNEXTRACTED_THUMB_INTERNAL_MEDIA);
	}

	thumb_warn("Query: %s", query_string);

	err = sqlite3_prepare_v2(sqlite_db_handle, query_string, strlen(query_string), &sqlite_stmt, NULL);
	if (SQLITE_OK != err) {
		thumb_err("prepare error [%s]", sqlite3_errmsg(sqlite_db_handle));
		_media_thumb_db_disconnect();
		return MEDIA_THUMB_ERROR_DB;
	}

	while(1) {
		err = sqlite3_step(sqlite_stmt);
		if (err != SQLITE_ROW) {
			thumb_dbg("end of row [%s]", sqlite3_errmsg(sqlite_db_handle));
			break;
		}

		strncpy(path, (const char *)sqlite3_column_text(sqlite_stmt, 0), sizeof(path));
		path[sizeof(path) - 1] = '\0';

		thumb_dbg_slog("Path : %s", path);

		if (g_idx == 0) {
			arr_path = (char**)malloc(sizeof(char*));
		} else {
			arr_path = (char**)realloc(arr_path, (g_idx + 1) * sizeof(char*));
		}

		arr_path[g_idx++] = strdup(path);
	}

	sqlite3_finalize(sqlite_stmt);
	_media_thumb_db_disconnect();

	return MEDIA_THUMB_ERROR_NONE;
}

 static int __thumb_daemon_get_camera_state()
{
	int err = -1;
	int camera_state = -1;

	err = vconf_get_int(VCONFKEY_CAMERA_STATE, &camera_state);
	if (err == 0) {
		thumb_dbg("camera state: %d", camera_state);
		return camera_state;
	} else {
		thumb_err("vconf_get_int failed: %d", err);
	}

	return camera_state;
}

int _thumb_daemon_process_queue_jobs(gpointer data)
{
	int err = -1;
	char *path = NULL;

	if (g_cur_idx < g_idx) {
		thumb_warn("There are %d jobs in the queue", g_idx - g_cur_idx);
		thumb_dbg("Current idx : [%d]", g_cur_idx);
		path = arr_path[g_cur_idx++];

		thumbMsg recv_msg, res_msg;
		memset(&recv_msg, 0x00, sizeof(thumbMsg));
		memset(&res_msg, 0x00, sizeof(thumbMsg));

		recv_msg.msg_type = THUMB_REQUEST_DB_INSERT;
		recv_msg.thumb_type = MEDIA_THUMB_LARGE;
		strncpy(recv_msg.org_path, path, sizeof(recv_msg.org_path));
		recv_msg.org_path[sizeof(recv_msg.org_path) - 1] = '\0';

		err = __thumb_daemon_process_job(&recv_msg, &res_msg);
		if (err == MEDIA_THUMB_ERROR_FILE_NOT_EXIST) {
			thumb_err("Thumbnail processing is failed : %d", err);
		} else {
			if (res_msg.status == THUMB_SUCCESS) {

				err = _media_thumb_db_connect();
				if (err < 0) {
					thumb_err("_media_thumb_mb_svc_connect failed: %d", err);
					return TRUE;
				}

				/* Need to update DB once generating thumb is done */
				err = _media_thumb_update_db(recv_msg.org_path,
											res_msg.dst_path,
											res_msg.origin_width,
											res_msg.origin_height);
				if (err < 0) {
					thumb_err("_media_thumb_update_db failed : %d", err);
				}

				_media_thumb_db_disconnect();
			}
		}

		SAFE_FREE(path);
	} else {
		g_cur_idx = 0;
		g_idx = 0;
		thumb_warn("Deleting array");
		SAFE_FREE(arr_path);
		//_media_thumb_db_disconnect();

		__thumb_server_send_msg_to_agent(MS_MSG_THUMB_EXTRACT_ALL_DONE); // MS_MSG_THUMB_EXTRACT_ALL_DONE
		g_source_id = 0;

		return FALSE;
	}

	return TRUE;
}

gboolean _thumb_server_read_socket(GIOChannel *src,
									GIOCondition condition,
									gpointer data)
{
	struct sockaddr_un client_addr;
	unsigned int client_addr_len;
	thumbMsg recv_msg;
	thumbMsg res_msg;

	int sock = -1;
	int header_size = 0;

	memset((void *)&recv_msg, 0, sizeof(recv_msg));
	memset((void *)&res_msg, 0, sizeof(res_msg));

	sock = g_io_channel_unix_get_fd(src);
	if (sock < 0) {
		thumb_err("sock fd is invalid!");
		return TRUE;
	}

	header_size = sizeof(thumbMsg) - MAX_PATH_SIZE*2;

	if (_media_thumb_recv_udp_msg(sock, header_size, &recv_msg, &client_addr, &client_addr_len) < 0) {
		thumb_err("_media_thumb_recv_udp_msg failed");
		return TRUE;
	}

	thumb_warn_slog("Received [%d] %s(%d) from PID(%d)", recv_msg.msg_type, recv_msg.org_path, strlen(recv_msg.org_path), recv_msg.pid);

	if (recv_msg.msg_type == THUMB_REQUEST_ALL_MEDIA) {
		if (g_idx == 0) {
			if (__thumb_daemon_get_camera_state() < VCONFKEY_CAMERA_STATE_PREVIEW) {
				if (g_source_id == 0) {
					thumb_warn("All thumbnails are being extracted now");
					__thumb_daemon_all_extract();
					g_source_id = g_idle_add(_thumb_daemon_process_queue_jobs, NULL);
				}
			}
			g_extract_all_status = true;
		} else {
			thumb_warn("All thumbnails are already being extracted.");
		}
	} else if(recv_msg.msg_type == THUMB_REQUEST_KILL_SERVER) {
		thumb_warn("received KILL msg from thumbnail agent.");
	} else {
		__thumb_daemon_process_job(&recv_msg, &res_msg);
	}

	res_msg.msg_type = recv_msg.msg_type;
	strncpy(res_msg.org_path, recv_msg.org_path, recv_msg.origin_path_size);
	res_msg.origin_path_size = recv_msg.origin_path_size;
	res_msg.dest_path_size = strlen(res_msg.dst_path) + 1;

	int buf_size = 0;
	unsigned char *buf = NULL;
	_media_thumb_set_buffer(&res_msg, &buf, &buf_size);

	//thumb_dbg("buffer size : %d", buf_size);

	thumb_dbg_slog("%s", client_addr.sun_path);
	if (sendto(sock, buf, buf_size, 0, (struct sockaddr *)&client_addr, sizeof(client_addr)) != buf_size) {
		thumb_stderror("sendto failed");
		SAFE_FREE(buf);
		return TRUE;
	}

	thumb_warn_slog("Sent %s(%d)", res_msg.dst_path, strlen(res_msg.dst_path));

	SAFE_FREE(buf);

	if(recv_msg.msg_type == THUMB_REQUEST_KILL_SERVER) {
		thumb_warn("Shutting down...");
#if 1
		if (_thumb_sever_set_power_mode(THUMB_END) == FALSE)
			thumb_err("_thumb_sever_set_power_mode failed");
#endif
		g_main_loop_quit(g_thumb_server_mainloop);
	}

	return TRUE;
}

static gboolean __thumb_server_send_msg_to_agent(int msg_type)
{
	int sock;
	ms_sock_info_s sock_info;
	struct sockaddr_un serv_addr;
	ms_thumb_server_msg send_msg;

	if (ms_ipc_create_client_socket(MS_PROTOCOL_UDP, MS_TIMEOUT_SEC_10, &sock_info) < 0) {
		thumb_err("ms_ipc_create_server_socket failed");
		return FALSE;
	}

	memset(&serv_addr, 0, sizeof(serv_addr));

	sock = sock_info.sock_fd;
	serv_addr.sun_family = AF_UNIX;
	strncpy(serv_addr.sun_path, "/tmp/.media_ipc_thumbcomm", sizeof(serv_addr.sun_path));

	send_msg.msg_type = msg_type;

	if (sendto(sock, &send_msg, sizeof(ms_thumb_server_msg), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != sizeof(ms_thumb_server_msg)) {
		thumb_stderror("sendto failed");
		ms_ipc_delete_client_socket(&sock_info);
		return FALSE;
	}

	thumb_dbg("Sending msg to thumbnail agent[%d] is successful", send_msg.msg_type);

	ms_ipc_delete_client_socket(&sock_info);

 	return TRUE;
}

gboolean _thumb_server_prepare_socket(int *sock_fd)
{
	int sock;
	unsigned short serv_port;

	thumbMsg recv_msg;
	thumbMsg res_msg;

	memset((void *)&recv_msg, 0, sizeof(recv_msg));
	memset((void *)&res_msg, 0, sizeof(res_msg));
	serv_port = MS_THUMB_DAEMON_PORT;

	if (ms_ipc_create_server_socket(MS_PROTOCOL_UDP, serv_port, &sock) < 0) {
		thumb_err("ms_ipc_create_server_socket failed");
		return FALSE;
	}

	*sock_fd = sock;

	return TRUE;
}

bool _thumb_sever_set_power_mode(_server_status_e status)
{
	int res = TRUE;
	int err;

	switch (status) {
	case THUMB_START:
		err = display_lock_state(LCD_OFF, STAY_CUR_STATE, 0);
		if (err != 0)
			res = FALSE;
		break;
	case THUMB_END:
		err = display_unlock_state(LCD_OFF, STAY_CUR_STATE);
		if (err != 0)
			res = FALSE;
		break;
	default:
		thumb_err("Unacceptable type : %d", status);
		res = FALSE;
		break;
	}

	return res;
}

#define SYS_DBUS_NAME "ChangeState"
#define SYS_DBUS_PATH "/Org/Tizen/System/DeviceD/PowerOff"
#define SYS_DBUS_INTERFACE "org.tizen.system.deviced.PowerOff"
#define SYS_DBUS_MATCH_RULE "type='signal',interface='org.tizen.system.deviced.PowerOff'"

typedef struct pwoff_callback_data{
	power_off_cb user_callback;
	void *user_data;
} pwoff_callback_data;

DBusHandlerResult
__get_dbus_message(DBusMessage *message, void *user_cb, void *userdata)
{
	thumb_dbg("");

	/* A Ping signal on the com.burtonini.dbus.Signal interface */
	if (dbus_message_is_signal (message, SYS_DBUS_INTERFACE, SYS_DBUS_NAME)) {
		int current_type = DBUS_TYPE_INVALID;
		DBusError error;
		DBusMessageIter read_iter;
		DBusBasicValue value;
		power_off_cb cb_func = (power_off_cb)user_cb;

		dbus_error_init (&error);

		/* get data from dbus message */
		dbus_message_iter_init (message, &read_iter);
		while ((current_type = dbus_message_iter_get_arg_type (&read_iter)) != DBUS_TYPE_INVALID){
	                dbus_message_iter_get_basic (&read_iter, &value);
			switch(current_type) {
				case DBUS_TYPE_INT32:
					thumb_warn("value[%d]", value.i32);
					break;
				default:
					thumb_err("current type : %d", current_type);
					break;
			}

			if (value.i32 == 2 || value.i32 == 3) {
				thumb_warn("PREPARE POWER OFF");
				break;
			}

			dbus_message_iter_next (&read_iter);
		}

		if (value.i32 == 2 || value.i32 == 3)
			cb_func(userdata);

		return DBUS_HANDLER_RESULT_HANDLED;
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static DBusHandlerResult
__sysman_message_filter (DBusConnection *connection, DBusMessage *message, void *user_data)
{
	DBusHandlerResult ret;

	pwoff_callback_data *cb_data = (pwoff_callback_data *)user_data;

	thumb_dbg("");

	ret = __get_dbus_message(message, cb_data->user_callback, cb_data->user_data);

	thumb_dbg("");

	return ret;
}

int _thumb_sever_poweoff_event_receiver(power_off_cb user_callback, void *user_data)
{
	DBusConnection *dbus;
	DBusError error;
	pwoff_callback_data *cb_data = NULL;

	/*add noti receiver for power off*/
	dbus_error_init (&error);

	dbus = dbus_bus_get (DBUS_BUS_SYSTEM, &error);
	if (!dbus) {
		thumb_err("Failed to connect to the D-BUS daemon: %s", error.message);
		return -1;
	}

	dbus_connection_setup_with_g_main (dbus, NULL);

	cb_data = malloc(sizeof(pwoff_callback_data));
	if (cb_data == NULL)
	{
		thumb_err("Failed to allocate memroy");
		return -1;
	}
	cb_data->user_callback = user_callback;
	cb_data->user_data = user_data;

	/* listening to messages from all objects as no path is specified */
	dbus_bus_add_match (dbus, SYS_DBUS_MATCH_RULE, &error);
	if( !dbus_connection_add_filter (dbus, __sysman_message_filter, cb_data, NULL)) {
		dbus_bus_remove_match (dbus, SYS_DBUS_MATCH_RULE, NULL);
		thumb_err("");
		return -1;
	}

	return MEDIA_THUMB_ERROR_NONE;
}

int _thumbnail_get_data(const char *origin_path,
						media_thumb_type thumb_type,
						media_thumb_format format,
						char *thumb_path,
						unsigned char **data,
						int *size,
						int *width,
						int *height,
						int *origin_width,
						int *origin_height,
						int *alpha,
						bool *is_saved)
{
	int err = -1;
	int thumb_width = -1;
	int thumb_height = -1;

	if (origin_path == NULL || size == NULL
			|| width == NULL || height == NULL) {
		thumb_err("Invalid parameter");
		return MEDIA_THUMB_ERROR_INVALID_PARAMETER;
	}

	if (format < MEDIA_THUMB_BGRA || format > MEDIA_THUMB_RGB888) {
		thumb_err("parameter format is invalid");
		return MEDIA_THUMB_ERROR_INVALID_PARAMETER;
	}

	if (!g_file_test
	    (origin_path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
			thumb_err("Original path (%s) does not exist", origin_path);
			return MEDIA_THUMB_ERROR_INVALID_PARAMETER;
	}

	thumb_width = _media_thumb_get_width(thumb_type);
	if (thumb_width < 0) {
		thumb_err("media_thumb_type is invalid");
		return MEDIA_THUMB_ERROR_INVALID_PARAMETER;
	}

	thumb_height = _media_thumb_get_height(thumb_type);
	if (thumb_height < 0) {
		thumb_err("media_thumb_type is invalid");
		return MEDIA_THUMB_ERROR_INVALID_PARAMETER;
	}

	thumb_dbg("Origin path : %s", origin_path);

	int file_type = THUMB_NONE_TYPE;
	int is_drm = FALSE;
	media_thumb_info thumb_info = {0,};
	file_type = _media_thumb_get_file_type(origin_path, &is_drm);

	if (file_type == THUMB_IMAGE_TYPE) {
		if (is_drm) {
			thumb_err("DRM IMAGE IS NOT SUPPORTED");
			return MEDIA_THUMB_ERROR_UNSUPPORTED;
		} else {
			err = _media_thumb_image(origin_path, thumb_path, thumb_width, thumb_height, format, &thumb_info);
			if (err < 0) {
				thumb_err("_media_thumb_image failed");
				return err;
			}
		}
	} else if (file_type == THUMB_VIDEO_TYPE) {
		err = _media_thumb_video(origin_path, thumb_width, thumb_height, format, &thumb_info);
		if (err < 0) {
			thumb_err("_media_thumb_image failed");
			return err;
		}
	} else {
		thumb_err("MEDIA_THUMB_ERROR_UNSUPPORTED");
		return MEDIA_THUMB_ERROR_UNSUPPORTED;
	}

	if (size) *size = thumb_info.size;
	if (width) *width = thumb_info.width;
	if (height) *height = thumb_info.height;
	*data = thumb_info.data;
	if (origin_width) *origin_width = thumb_info.origin_width;
	if (origin_height) *origin_height = thumb_info.origin_height;
	if (alpha) *alpha = thumb_info.alpha;
	if (is_saved) *is_saved= thumb_info.is_saved;

	thumb_dbg("Thumb data is generated successfully (Size:%d, W:%d, H:%d) 0x%x",
				*size, *width, *height, *data);

	return MEDIA_THUMB_ERROR_NONE;
}

int
_media_thumb_process(thumbMsg *req_msg, thumbMsg *res_msg)
{
	int err = -1;
	unsigned char *data = NULL;
	int thumb_size = 0;
	int thumb_w = 0;
	int thumb_h = 0;
	int origin_w = 0;
	int origin_h = 0;
	int max_length = 0;
	char *thumb_path = NULL;
	int need_update_db = 0;
	int alpha = 0;
	bool is_saved = FALSE;

	if (req_msg == NULL || res_msg == NULL) {
		thumb_err("Invalid msg!");
		return MEDIA_THUMB_ERROR_INVALID_PARAMETER;
	}

	int msg_type = req_msg->msg_type;
	media_thumb_type thumb_type = req_msg->thumb_type;
	const char *origin_path = req_msg->org_path;

	media_thumb_format thumb_format = MEDIA_THUMB_BGRA;

	thumb_path = res_msg->dst_path;
	thumb_path[0] = '\0';
	max_length = sizeof(res_msg->dst_path);

	if (!g_file_test(origin_path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
		thumb_err("origin_path does not exist in file system.");
		return MEDIA_THUMB_ERROR_FILE_NOT_EXIST;
	}

	err = _media_thumb_db_connect();
	if (err < 0) {
		thumb_err("_media_thumb_mb_svc_connect failed: %d", err);
		return MEDIA_THUMB_ERROR_DB;
	}

	if (msg_type == THUMB_REQUEST_DB_INSERT) {
		err = _media_thumb_get_thumb_from_db_with_size(origin_path, thumb_path, max_length, &need_update_db, &origin_w, &origin_h);
		if (err == 0) {
			res_msg->origin_width = origin_w;
			res_msg->origin_height = origin_h;
			_media_thumb_db_disconnect();
			return MEDIA_THUMB_ERROR_NONE;
		} else {
			if (strlen(thumb_path) == 0) {
				err = _media_thumb_get_hash_name(origin_path, thumb_path, max_length);
				if (err < 0) {
					thumb_err("_media_thumb_get_hash_name failed - %d", err);
					strncpy(thumb_path, THUMB_DEFAULT_PATH, max_length);
					_media_thumb_db_disconnect();
					return err;
				}

				thumb_path[strlen(thumb_path)] = '\0';
			}
		}

	} else if (msg_type == THUMB_REQUEST_SAVE_FILE) {
		strncpy(thumb_path, req_msg->dst_path, max_length);

	} else if (msg_type == THUMB_REQUEST_ALL_MEDIA) {
		err = _media_thumb_get_hash_name(origin_path, thumb_path, max_length);
		if (err < 0) {
			thumb_err("_media_thumb_get_hash_name failed - %d", err);
			strncpy(thumb_path, THUMB_DEFAULT_PATH, max_length);
			_media_thumb_db_disconnect();
			return err;
		}

		thumb_path[strlen(thumb_path)] = '\0';
	}

	thumb_dbg_slog("Thumb path : %s", thumb_path);

	if (g_file_test(thumb_path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
		thumb_warn("thumb path already exists in file system.. remove the existed file");
		_media_thumb_remove_file(thumb_path);
	}

	err = _thumbnail_get_data(origin_path, thumb_type, thumb_format, thumb_path, &data, &thumb_size, &thumb_w, &thumb_h, &origin_w, &origin_h, &alpha, &is_saved);
	if (err < 0) {
		thumb_err("_thumbnail_get_data failed - %d", err);
		SAFE_FREE(data);

		strncpy(thumb_path, THUMB_DEFAULT_PATH, max_length);
		goto DB_UPDATE;
//		_media_thumb_db_disconnect();
//		return err;
	}

	//thumb_dbg("Size : %d, W:%d, H:%d", thumb_size, thumb_w, thumb_h);
	//thumb_dbg("Origin W:%d, Origin H:%d\n", origin_w, origin_h);
	//thumb_dbg("Thumb : %s", thumb_path);

	res_msg->msg_type = THUMB_RESPONSE;
	res_msg->thumb_size = thumb_size;
	res_msg->thumb_width = thumb_w;
	res_msg->thumb_height = thumb_h;
	res_msg->origin_width = origin_w;
	res_msg->origin_height = origin_h;

	/* If the image is transparent PNG format, make png file as thumbnail of this image */
	if (alpha) {
		char file_ext[10];
		err = _media_thumb_get_file_ext(origin_path, file_ext, sizeof(file_ext));
		if (strncasecmp(file_ext, "png", 3) == 0) {
			int len = strlen(thumb_path);
			thumb_path[len - 3] = 'p';
			thumb_path[len - 2] = 'n';
			thumb_path[len - 1] = 'g';
		}
		thumb_dbg_slog("Thumb path is changed : %s", thumb_path);
	}

	if (is_saved == FALSE && data != NULL) {
		err = _media_thumb_save_to_file_with_evas(data, thumb_w, thumb_h, alpha, thumb_path);
		if (err < 0) {
			thumb_err("save_to_file_with_evas failed - %d", err);
			SAFE_FREE(data);

			if (msg_type == THUMB_REQUEST_DB_INSERT || msg_type == THUMB_REQUEST_ALL_MEDIA)
				strncpy(thumb_path, THUMB_DEFAULT_PATH, max_length);

			_media_thumb_db_disconnect();
			return err;
		} else {
			thumb_dbg("file save success");
		}
	} else {
		thumb_dbg("file is already saved");
	}

	/* fsync */
	int fd = 0;
	fd = open(thumb_path, O_WRONLY);
	if (fd < 0) {
		thumb_warn("open failed");
	} else {
		err = fsync(fd);
		if (err == -1) {
			thumb_warn("fsync failed");
		}

		close(fd);
	}
	/* End of fsync */

	SAFE_FREE(data);
DB_UPDATE:
	/* DB update if needed */
	if (need_update_db == 1) {
		err = _media_thumb_update_db(origin_path, thumb_path, res_msg->origin_width, res_msg->origin_height);
		if (err < 0) {
			thumb_err("_media_thumb_update_db failed : %d", err);
		}
	}

	_media_thumb_db_disconnect();

	return 0;
}

