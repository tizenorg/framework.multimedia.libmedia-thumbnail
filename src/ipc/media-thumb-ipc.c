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

#include "media-thumbnail.h"
#include "media-thumb-ipc.h"
#include "media-thumb-util.h"
#include "media-thumb-db.h"
#include <glib.h>
#include <string.h>
#include <errno.h>

static __thread GQueue *g_request_queue = NULL;
typedef struct {
	GIOChannel *channel;
	char *path;
	int source_id;
	thumbUserData *userData;
} thumbReq;

int _media_thumb_get_error()
{
	if (errno == EWOULDBLOCK) {
		thumb_err("Timeout. Can't try any more");
		return MEDIA_THUMB_ERROR_TIMEOUT;
	} else {
		thumb_err("recvfrom failed : %s", strerror(errno));
		return MEDIA_THUMB_ERROR_NETWORK;
	}
}

int __media_thumb_pop_req_queue(const char *path, bool shutdown_channel)
{
	int req_len = 0, i;

	if (g_request_queue == NULL) return -1;
	req_len = g_queue_get_length(g_request_queue);

//	thumb_dbg("Queue length : %d", req_len);
//	thumb_dbg("Queue path : %s", path);

	if (req_len <= 0) {
//		thumb_dbg("There is no request in the queue");
	} else {

		for (i = 0; i < req_len; i++) {
			thumbReq *req = NULL;
			req = (thumbReq *)g_queue_peek_nth(g_request_queue, i);
			if (req == NULL) continue;

			if (strncmp(path, req->path, strlen(path)) == 0) {
				//thumb_dbg("Popped %s", path);
				if (shutdown_channel) {
					GSource *source_id = g_main_context_find_source_by_id(g_main_context_get_thread_default(), req->source_id);
					if (source_id != NULL) {
						g_source_destroy(source_id);
					} else {
						thumb_err("G_SOURCE_ID is NULL");
					}

					g_io_channel_shutdown(req->channel, TRUE, NULL);
					g_io_channel_unref(req->channel);
				}
				g_queue_pop_nth(g_request_queue, i);

				SAFE_FREE(req->path);
				SAFE_FREE(req->userData);
				SAFE_FREE(req);

				break;
			}
		}
		if (i == req_len) {
//			thumb_dbg("There's no %s in the queue", path);
		}
	}

	return MEDIA_THUMB_ERROR_NONE;
}

int __media_thumb_check_req_queue(const char *path)
{
	int req_len = 0, i;

	req_len = g_queue_get_length(g_request_queue);

//	thumb_dbg("Queue length : %d", req_len);
//	thumb_dbg("Queue path : %s", path);

	if (req_len <= 0) {
//		thumb_dbg("There is no request in the queue");
	} else {

		for (i = 0; i < req_len; i++) {
			thumbReq *req = NULL;
			req = (thumbReq *)g_queue_peek_nth(g_request_queue, i);
			if (req == NULL) continue;

			if (strncmp(path, req->path, strlen(path)) == 0) {
				//thumb_dbg("Same Request - %s", path);
				return -1;

				break;
			}
		}
	}

	return MEDIA_THUMB_ERROR_NONE;
}

int
_media_thumb_recv_msg(int sock, int header_size, thumbMsg *msg)
{
	int recv_msg_len = 0;
	unsigned char *buf = NULL;

	buf = (unsigned char*)malloc(header_size);

	if ((recv_msg_len = recv(sock, buf, header_size, 0)) <= 0) {
		thumb_err("recv failed : %s", strerror(errno));
		SAFE_FREE(buf);
		return _media_thumb_get_error();
	}

	memcpy(msg, buf, header_size);
	//thumb_dbg("origin_path_size : %d, dest_path_size : %d", msg->origin_path_size, msg->dest_path_size);

	SAFE_FREE(buf);

	if (!(msg->origin_path_size > 0 && msg->origin_path_size < MAX_FILEPATH_LEN)) {
		thumb_err("origin path size is wrong");
		return MEDIA_THUMB_ERROR_INVALID_PARAMETER;
	}

	if (!(msg->dest_path_size > 0 && msg->dest_path_size < MAX_FILEPATH_LEN)) {
		thumb_err("destination path size is wrong");
		return MEDIA_THUMB_ERROR_INVALID_PARAMETER;
	}

	buf = (unsigned char*)malloc(msg->origin_path_size);

	if ((recv_msg_len = recv(sock, buf, msg->origin_path_size, 0)) < 0) {
		thumb_err("recv failed : %s", strerror(errno));
		SAFE_FREE(buf);
		return _media_thumb_get_error();
	}

	strncpy(msg->org_path, (char*)buf, msg->origin_path_size);
	//thumb_dbg("original path : %s", msg->org_path);

	SAFE_FREE(buf);
	buf = (unsigned char*)malloc(msg->dest_path_size);

	if ((recv_msg_len = recv(sock, buf, msg->dest_path_size, 0)) < 0) {
		thumb_err("recv failed : %s", strerror(errno));
		SAFE_FREE(buf);
		return _media_thumb_get_error();
	}

	strncpy(msg->dst_path, (char*)buf, msg->dest_path_size);
	//thumb_dbg("destination path : %s", msg->dst_path);

	SAFE_FREE(buf);
	return MEDIA_THUMB_ERROR_NONE;
}


int
_media_thumb_recv_udp_msg(int sock, int header_size, thumbMsg *msg, struct sockaddr_un *from_addr, unsigned int *from_size)
{
	int recv_msg_len = 0;
	unsigned int from_addr_size = sizeof(struct sockaddr_un);
	unsigned char *buf = NULL;

	buf = (unsigned char*)malloc(sizeof(thumbMsg));
	//thumb_dbg("header size : %d", header_size);

	if ((recv_msg_len = recvfrom(sock, buf, sizeof(thumbMsg), 0, (struct sockaddr *)from_addr, &from_addr_size)) < 0) {
		thumb_err("recvfrom failed : %s", strerror(errno));
		SAFE_FREE(buf);
		return _media_thumb_get_error();
	}

	memcpy(msg, buf, header_size);

	if (msg->origin_path_size <= 0  || msg->origin_path_size > MAX_PATH_SIZE) {
		SAFE_FREE(buf);
		thumb_err("msg->origin_path_size is invalid %d", msg->origin_path_size );
		return MEDIA_THUMB_ERROR_INVALID_PARAMETER;
	}

	strncpy(msg->org_path, (char*)buf + header_size, msg->origin_path_size);
	//thumb_dbg("original path : %s", msg->org_path);

	if (msg->dest_path_size <= 0  || msg->dest_path_size > MAX_PATH_SIZE) {
		SAFE_FREE(buf);
		thumb_err("msg->origin_path_size is invalid %d", msg->dest_path_size );
		return MEDIA_THUMB_ERROR_INVALID_PARAMETER;
	}

	strncpy(msg->dst_path, (char*)buf + header_size + msg->origin_path_size, msg->dest_path_size);
	//thumb_dbg("destination path : %s", msg->dst_path);

	SAFE_FREE(buf);
	*from_size = from_addr_size;

	return MEDIA_THUMB_ERROR_NONE;
}

int
_media_thumb_set_buffer(thumbMsg *req_msg, unsigned char **buf, int *buf_size)
{
	if (req_msg == NULL || buf == NULL) {
		return -1;
	}

	int org_path_len = 0;
	int dst_path_len = 0;
	int size = 0;
	int header_size = 0;

	header_size = sizeof(thumbMsg) - MAX_PATH_SIZE*2;
	org_path_len = strlen(req_msg->org_path) + 1;
	dst_path_len = strlen(req_msg->dst_path) + 1;

	//thumb_dbg("Basic Size : %d, org_path : %s[%d], dst_path : %s[%d]", header_size, req_msg->org_path, org_path_len, req_msg->dst_path, dst_path_len);

	size = header_size + org_path_len + dst_path_len;
	*buf = malloc(size);
	memcpy(*buf, req_msg, header_size);
	memcpy((*buf)+header_size, req_msg->org_path, org_path_len);
	memcpy((*buf)+header_size + org_path_len, req_msg->dst_path, dst_path_len);

	*buf_size = size;

	return 0;
}

int
_media_thumb_request(int msg_type, media_thumb_type thumb_type, const char *origin_path, char *thumb_path, int max_length, media_thumb_info *thumb_info)
{
	int sock;
	struct sockaddr_un serv_addr;
	ms_sock_info_s sock_info;
	int recv_str_len = 0;
	int err;
	int pid;

	if (ms_ipc_create_client_socket(MS_PROTOCOL_TCP, MS_TIMEOUT_SEC_10, &sock_info) < 0) {
		thumb_err("ms_ipc_create_client_socket failed");
		return MEDIA_THUMB_ERROR_NETWORK;
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
	sock = sock_info.sock_fd;
	serv_addr.sun_family = AF_UNIX;
	strcpy(serv_addr.sun_path, "/tmp/.media_ipc_thumbcreator");

	/* Connecting to the thumbnail server */
	if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		thumb_err("connect error : %s", strerror(errno));
		ms_ipc_delete_client_socket(&sock_info);
		return MEDIA_THUMB_ERROR_NETWORK;
	}

	thumbMsg req_msg;
	thumbMsg recv_msg;

	memset((void *)&req_msg, 0, sizeof(thumbMsg));
	memset((void *)&recv_msg, 0, sizeof(thumbMsg));

	/* Get PID of client*/
	pid = getpid();
	req_msg.pid = pid;

	/* Set requset message */
	req_msg.msg_type = msg_type;
	req_msg.thumb_type = thumb_type;
	strncpy(req_msg.org_path, origin_path, sizeof(req_msg.org_path));
	req_msg.org_path[strlen(req_msg.org_path)] = '\0';

	if (msg_type == THUMB_REQUEST_SAVE_FILE) {
		strncpy(req_msg.dst_path, thumb_path, sizeof(req_msg.dst_path));
		req_msg.dst_path[strlen(req_msg.dst_path)] = '\0';
	}

	req_msg.origin_path_size = strlen(req_msg.org_path) + 1;
	req_msg.dest_path_size = strlen(req_msg.dst_path) + 1;

	if (req_msg.origin_path_size > MAX_PATH_SIZE || req_msg.dest_path_size > MAX_PATH_SIZE) {
		thumb_err("path's length exceeds %d", MAX_PATH_SIZE);
		ms_ipc_delete_client_socket(&sock_info);
		return MEDIA_THUMB_ERROR_INVALID_PARAMETER;
	}

	unsigned char *buf = NULL;
	int buf_size = 0;
	int header_size = 0;

	header_size = sizeof(thumbMsg) - MAX_PATH_SIZE*2;
	_media_thumb_set_buffer(&req_msg, &buf, &buf_size);

	if (send(sock, buf, buf_size, 0) != buf_size) {
		thumb_err("sendto failed: %d", errno);
		SAFE_FREE(buf);
		ms_ipc_delete_client_socket(&sock_info);
		return MEDIA_THUMB_ERROR_NETWORK;
	}

	thumb_dbg("Sending msg to thumbnail daemon is successful");

	SAFE_FREE(buf);

	if ((err = _media_thumb_recv_msg(sock, header_size, &recv_msg)) < 0) {
		thumb_err("_media_thumb_recv_msg failed ");
		ms_ipc_delete_client_socket(&sock_info);
		return err;
	}

	recv_str_len = strlen(recv_msg.org_path);
	thumb_dbg_slog("recv %s(%d) from thumb daemon is successful", recv_msg.org_path, recv_str_len);

	ms_ipc_delete_client_socket(&sock_info);

	if (recv_str_len > max_length) {
		thumb_err("user buffer is too small. Output's length is %d", recv_str_len);
		return MEDIA_THUMB_ERROR_INVALID_PARAMETER;
	}

	if (recv_msg.status == THUMB_FAIL) {
		thumb_err("Failed to make thumbnail");
		return -1;
	}

	if (msg_type != THUMB_REQUEST_SAVE_FILE) {
		strncpy(thumb_path, recv_msg.dst_path, max_length);
	}

	thumb_info->origin_width = recv_msg.origin_width;
	thumb_info->origin_height = recv_msg.origin_height;

	return 0;
}

gboolean _media_thumb_write_socket(GIOChannel *src, GIOCondition condition, gpointer data)
{
	thumbMsg recv_msg;
	int header_size = 0;
	int sock = 0;
	int err = MEDIA_THUMB_ERROR_NONE;

	memset((void *)&recv_msg, 0, sizeof(thumbMsg));
	sock = g_io_channel_unix_get_fd(src);

	header_size = sizeof(thumbMsg) - MAX_PATH_SIZE*2;

	thumb_err("_media_thumb_write_socket socket : %d", sock);

	if ((err = _media_thumb_recv_msg(sock, header_size, &recv_msg)) < 0) {
		thumb_err("_media_thumb_recv_msg failed ");
//		g_io_channel_shutdown(src, TRUE, NULL);
//		g_io_channel_unref(src);
		if (recv_msg.origin_path_size > 0) {
			__media_thumb_pop_req_queue(recv_msg.org_path, TRUE);
		} else {
			thumb_err("origin path size is wrong.");
		}

		return FALSE;
	}

	g_io_channel_shutdown(src, TRUE, NULL);
	g_io_channel_unref(src);
	//thumb_dbg("Completed..%s", recv_msg.org_path);

	if (recv_msg.status == THUMB_FAIL) {
		thumb_err("Failed to make thumbnail");
		err = MEDIA_THUMB_ERROR_UNSUPPORTED;
	}

	if (data) {
		thumbUserData* cb = (thumbUserData*)data;
		if (cb->func != NULL)
			cb->func(err, recv_msg.dst_path, cb->user_data);
	}

	__media_thumb_pop_req_queue(recv_msg.org_path, FALSE);

	thumb_dbg("Done");
	return FALSE;
}

int
_media_thumb_request_async(int msg_type, media_thumb_type thumb_type, const char *origin_path, thumbUserData *userData)
{
	int sock;
	struct sockaddr_un serv_addr;
	ms_sock_info_s sock_info;
	int pid;

	if ((msg_type == THUMB_REQUEST_DB_INSERT) && (__media_thumb_check_req_queue(origin_path) < 0)) {
		return MEDIA_THUMB_ERROR_DUPLICATED_REQUEST;
	}

	if (ms_ipc_create_client_socket(MS_PROTOCOL_TCP, MS_TIMEOUT_SEC_10, &sock_info) < 0) {
		thumb_err("ms_ipc_create_client_socket failed");
		return MEDIA_THUMB_ERROR_NETWORK;
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
	sock = sock_info.sock_fd;
	serv_addr.sun_family = AF_UNIX;
	strcpy(serv_addr.sun_path, "/tmp/.media_ipc_thumbcreator");

	GIOChannel *channel = NULL;
	channel = g_io_channel_unix_new(sock);
	int source_id = -1;

	/* Connecting to the thumbnail server */
	if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		thumb_err("connect error : %s", strerror(errno));
		g_io_channel_shutdown(channel, TRUE, NULL);
		g_io_channel_unref(channel);
		return MEDIA_THUMB_ERROR_NETWORK;
	}

	if (msg_type != THUMB_REQUEST_CANCEL_MEDIA) {
		//source_id = g_io_add_watch(channel, G_IO_IN, _media_thumb_write_socket, userData );

		/* Create new channel to watch udp socket */
		GSource *source = NULL;
		source = g_io_create_watch(channel, G_IO_IN);

		/* Set callback to be called when socket is readable */
		/*NEED UPDATE SOCKET FILE DELETE*/
		g_source_set_callback(source, (GSourceFunc)_media_thumb_write_socket, userData, NULL);
		source_id = g_source_attach(source, g_main_context_get_thread_default());
	}

	thumbMsg req_msg;
	memset((void *)&req_msg, 0, sizeof(thumbMsg));

	pid = getpid();
	req_msg.pid = pid;
	req_msg.msg_type = msg_type;
	req_msg.thumb_type = thumb_type;
	strncpy(req_msg.org_path, origin_path, sizeof(req_msg.org_path));
	req_msg.org_path[strlen(req_msg.org_path)] = '\0';

	req_msg.origin_path_size = strlen(req_msg.org_path) + 1;
	req_msg.dest_path_size = strlen(req_msg.dst_path) + 1;

	if (req_msg.origin_path_size > MAX_PATH_SIZE || req_msg.dest_path_size > MAX_PATH_SIZE) {
		thumb_err("path's length exceeds %d", MAX_PATH_SIZE);
		g_io_channel_shutdown(channel, TRUE, NULL);
		g_io_channel_unref(channel);
		return MEDIA_THUMB_ERROR_INVALID_PARAMETER;
	}

	unsigned char *buf = NULL;
	int buf_size = 0;
	_media_thumb_set_buffer(&req_msg, &buf, &buf_size);

	//thumb_dbg("buffer size : %d", buf_size);

	if (send(sock, buf, buf_size, 0) != buf_size) {
		thumb_err("sendto failed: %d", errno);
		SAFE_FREE(buf);
		g_source_destroy(g_main_context_find_source_by_id(g_main_context_get_thread_default(), source_id));
		g_io_channel_shutdown(channel, TRUE, NULL);
		g_io_channel_unref(channel);
		return MEDIA_THUMB_ERROR_NETWORK;
	}

	SAFE_FREE(buf);
	thumb_dbg("Sending msg to thumbnail daemon is successful");

#if 0
	if (msg_type == THUMB_REQUEST_CANCEL_MEDIA) {
		g_io_channel_shutdown(channel, TRUE, NULL);
	}
#else
	if (msg_type == THUMB_REQUEST_CANCEL_MEDIA) {
		g_io_channel_shutdown(channel, TRUE, NULL);

		//thumb_dbg("Cancel : %s[%d]", origin_path, sock);
		__media_thumb_pop_req_queue(origin_path, TRUE);
	} else if (msg_type == THUMB_REQUEST_DB_INSERT) {
		if (g_request_queue == NULL) {
		 	g_request_queue = g_queue_new();
		}

		thumbReq *thumb_req = NULL;
		thumb_req = calloc(1, sizeof(thumbReq));
		if (thumb_req == NULL) {
			thumb_err("Failed to create request element");
			return 0;
		}

		thumb_req->channel = channel;
		thumb_req->path = strdup(origin_path);
		thumb_req->source_id = source_id;
		thumb_req->userData = userData;

		//thumb_dbg("Push : %s [%d]", origin_path, sock);
		g_queue_push_tail(g_request_queue, (gpointer)thumb_req);
	}
#endif

	return 0;
}

