/*
 * Created by Hxk
 * 2022/10/18
 */

#include "dma_communication.h"
#include <errno.h>
DOCA_LOG_REGISTER(DMA_COMMUNICATION);

// Convert port to string 
// size_t num : the port being converted
// char *str : converted string
void port_2_str(size_t num, char *str) {
	char tmp_s[6] = {0}; 
	int i = 0;
	int tmp = num / 10;
	while(tmp > 0){
		tmp_s[i++] = num % 10 + 48;
		num = num / 10;
		tmp = num / 10;
	}
	tmp_s[i] = num + 48;

	for(int j = 0; j <= i; j++) {
		str[j] = tmp_s[i - j];
	}
	str[i + 1] = '\0';
}

bool bind_thread_to_core(int core_id) {
	cpu_set_t cpuset;
    pthread_t current_thread;

	// Get the number of the cores
	int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
	int core = core_id % num_cores;

	current_thread = pthread_self();
    CPU_ZERO(&cpuset);
	CPU_SET(core, &cpuset);

	int res = pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
	if(res != 0) {
		DOCA_LOG_ERR("Failed to bind to core");	
		return false;
	}
	return true;
}

bool client_connect(int *fd, char *ip, uint16_t port) {
    struct sockaddr_in addr;
	struct timeval timeout = {
		.tv_sec = 5,
	};
    int ret;

    *fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(*fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

	addr.sin_addr.s_addr = inet_addr(ip);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	if (connect(*fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		DOCA_LOG_ERR("Couldn't establish a connection to remote node");
		close(*fd);
		return false;
	}
	DOCA_LOG_INFO("client_connect is successfully ");

	return true;
}

bool server_accept(int *fd, const size_t port) {
	struct addrinfo *res, *it;
	struct addrinfo hints = {
		.ai_flags = AI_PASSIVE,
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM
	};    
	int receiver_fd = -1;
	int queue_size = 1;
	int optval = 1;

	char p[6] = {0};
	port_2_str(port, p);
	if (getaddrinfo(NULL, p, &hints, &res)) {
		DOCA_LOG_ERR("Failed to retrieve network information");
		return false;
	}

	for (it = res; it; it = it->ai_next) {
		receiver_fd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
		if (receiver_fd >= 0) {
			setsockopt(receiver_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
			if (!bind(receiver_fd, it->ai_addr, it->ai_addrlen))
				break;
			close(receiver_fd);
			receiver_fd = -1;
		}
	}

	freeaddrinfo(res);

	if (receiver_fd < 0) {
		DOCA_LOG_ERR("Port listening failed");
		return false;
	}

	listen(receiver_fd, queue_size);

	DOCA_LOG_INFO("Waiting for remote node to connect");

    *fd = accept(receiver_fd, NULL, 0);

    close(receiver_fd);

    if (*fd < 0) {
        DOCA_LOG_ERR("Connection acceptance failed");
        return false;
    }

    return true;
}

bool send_dma_info_to_remote(int fd, 
                             char *export_str, 
                             size_t export_str_len){
	int ret;								
    ret = write(fd, export_str, export_str_len);

	printf("ret is %d\n", ret);
	printf("export_str is %s\n", export_str);

	if (ret != (int)export_str_len) {
		DOCA_LOG_ERR("Failed to send data to remote node");
		close(fd);
		return false;
	}

    DOCA_LOG_INFO("Send dma info successfully");
	// DOCA_LOG_INFO("Message is %s", export_str);
    return true;
}

bool receive_dma_info_from_remote(int fd, 
                                  char *export_buffer, 
                                  size_t export_buffer_len,
                                  char **remote_addr,
			                      size_t *remote_addr_len){                          

	struct json_object *from_export_json;
	struct json_object *addr;
	struct json_object *len;
	int bytes_ret;

    bytes_ret = recv(fd, export_buffer, export_buffer_len, 0);
	printf("bytes_ret is %d\n", bytes_ret);
	printf("bytes_ret is %s\n", export_buffer);

    if (bytes_ret == -1) {
        DOCA_LOG_ERR("Couldn't receive data from remote node");
        close(fd);
        return false;
    } else if (bytes_ret == export_buffer_len) {
        if (export_buffer[export_buffer_len - 1] != '\0') {
            DOCA_LOG_ERR("Exported data buffer size is not sufficient");
            return false;
        }
    }

    DOCA_LOG_INFO("Exported data was received");
	// DOCA_LOG_INFO("Message is %s", export_buffer);

    /* Parse the export json */
    from_export_json = json_tokener_parse(export_buffer);
    json_object_object_get_ex(from_export_json, "addr", &addr);
    json_object_object_get_ex(from_export_json, "len", &len);
    *remote_addr = (char *) json_object_get_int64(addr);
    *remote_addr_len = (size_t) json_object_get_int64(len);
    json_object_put(from_export_json);

    return true;
}

void send_end_ack_to_remote(struct dma_parameters *dma_params) {
    int ret;
	char ack_buffer[] = "DMA operation was completed";
	int length = strlen(ack_buffer) + 1;

	ret = write(dma_params->tcp_fd, ack_buffer, length);
	if (ret != length)
		DOCA_LOG_ERR("Failed to send ack message to sender node");

	close(dma_params->tcp_fd);
}

void wait_remote_end_ack(struct dma_parameters *dma_params) {
    char ack_buffer[1024] = {0};
	char exp_ack[] = "DMA operation was completed";

    /* Waiting for DMA completion signal from receiver */
	DOCA_LOG_INFO("Waiting for remote node to acknowledge DMA operation was ended");

	int res;
	while (1) {
		res = recv(dma_params->tcp_fd, ack_buffer, sizeof(ack_buffer), 0);
		if(res > 0){
			break;
		}
		else {
			if(res < 0 && (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN)) {
				continue;
			}
			DOCA_LOG_ERR("Failed to receive ack message");
			close(dma_params->tcp_fd);
			return false;
		}
	}
	
	if (strcmp(exp_ack, ack_buffer)) {
		DOCA_LOG_ERR("Ack message is not correct");
		close(dma_params->tcp_fd);
		return false;
	}

	DOCA_LOG_INFO("Ack message was received, closing memory mapping");

	close(dma_params->tcp_fd);
}

doca_error_t poll_wq(struct doca_workq *workq, struct doca_event *event){
	doca_error_t res;
	/* Wait for job completion */
	while ((res = doca_workq_progress_retrieve(workq, event, DOCA_WORKQ_RETRIEVE_FLAGS_NONE)) ==
			DOCA_ERROR_AGAIN) {
		/* Do nothing */
	}

	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to submit DMA job: %s", doca_get_error_string(res));
		return res;
	}

	/* On DOCA_SUCCESS, Verify DMA job result */
	if (event->result.u64 != DOCA_SUCCESS) {
		DOCA_LOG_ERR("DMA job returned unsuccessfully");
		return DOCA_ERROR_UNKNOWN;
	} 
	return DOCA_SUCCESS;
}

doca_error_t run_remote_dma_read_lat(struct dma_parameters *dma_params) {
	if (dma_params->is_server) {
		if(!server_accept(&(dma_params->tcp_fd), dma_params->port)) {
			DOCA_LOG_ERR("Can not accept to client");
			return DOCA_ERROR_NOT_CONNECTED;
		}
	}
	else {
		if(!(client_connect(&(dma_params->tcp_fd), dma_params->ip, dma_params->port))) {
			DOCA_LOG_ERR("Can not connoct to server");
			return DOCA_ERROR_NOT_CONNECTED;
		}
	}

	doca_error_t res;
	size_t export_str_len;

	res = open_local_device(&(dma_params->pcie_addr), &(dma_params->state));
	if (res != DOCA_SUCCESS){
		DOCA_LOG_ERR("open_local_device false");
		return res;
	}

	size_t length = dma_params->size;
	void *src_buf = malloc(length);

	if(dma_params->is_server) {
		if (!receive_dma_info_from_remote(dma_params->tcp_fd, dma_params->export_json, 1024, 
						&(dma_params->remote_addr), &(dma_params->remote_addr_len))) {
			DOCA_LOG_ERR("receive_dma_info_from_remote false");
			res = DOCA_ERROR_NOT_CONNECTED;
			goto end;
		}

		struct doca_event event = {0};
		struct doca_job doca_job = {0};
		struct doca_dma_job_memcpy dma_job = {0};
		struct doca_buf *src_doca_buf;
		struct doca_buf *dst_doca_buf;

		res = create_core_objects(&(dma_params->state), dma_params->num_elements, dma_params->wq_depth);
		if (res != DOCA_SUCCESS) {
			DOCA_LOG_ERR("create_core_objects false");
			send_end_ack_to_remote(dma_params);
			goto end;
		}

		res = init_core_objects(&(dma_params->state), dma_params->max_chunks);
		if (res != DOCA_SUCCESS) {
			DOCA_LOG_ERR("init_core_objects false");
			send_end_ack_to_remote(dma_params);
			goto end;
		}	

		res = populate_mmap(dma_params->state.local_mmap, src_buf, length, dma_params->pg_sz);
		if (res != DOCA_SUCCESS) {
			DOCA_LOG_ERR("populate_mmap false");
			send_end_ack_to_remote(dma_params);
			goto end;
		}

		/* Create a local DOCA mmap from exported data */
		res = doca_mmap_create_from_export("remote_mmap", (uint8_t *)(dma_params->export_json), strlen(dma_params->export_json) + 1, 
							dma_params->state.dev, &(dma_params->state.remote_mmap));			   
		if (res != DOCA_SUCCESS) {
			DOCA_LOG_ERR("doca_mmap_create_from_export false: %s", doca_get_error_string(res));
			send_end_ack_to_remote(dma_params);
			goto end;
		}

		/* Construct DOCA buffer for each address range */
		res = doca_buf_inventory_buf_by_addr(dma_params->state.buf_inv, dma_params->state.remote_mmap, 
											dma_params->remote_addr, dma_params->remote_addr_len, &dst_doca_buf);
		if (res != DOCA_SUCCESS) {
			DOCA_LOG_ERR("Unable to acquire DOCA buffer representing remote buffer: %s", doca_get_error_string(res));
			send_end_ack_to_remote(dma_params);
			goto end;
		}

		/* Construct DOCA buffer for each address range */
		res = doca_buf_inventory_buf_by_addr(dma_params->state.buf_inv, dma_params->state.local_mmap, src_buf, length, &src_doca_buf);
		if (res != DOCA_SUCCESS) {
			DOCA_LOG_ERR("Unable to acquire DOCA buffer representing destination buffer: %s", doca_get_error_string(res));
			doca_buf_refcount_rm(dst_doca_buf, NULL);
			send_end_ack_to_remote(dma_params);
			goto end;
		}

		/* Construct DMA job */
		doca_job.type = DOCA_DMA_JOB_MEMCPY;
		doca_job.flags = DOCA_JOB_FLAGS_NONE;
		doca_job.ctx = dma_params->state.ctx;

		dma_job.base = doca_job;
		dma_job.dst_buff = src_doca_buf;
		dma_job.src_buff = dst_doca_buf;
		dma_job.num_bytes_to_copy = length;

		size_t inters = dma_params->iters;
		size_t preheats = dma_params->preheats;

		// warm up
		for (size_t i = 0; i < preheats; i++) {
			/* Enqueue DMA job */
			res = doca_workq_submit(dma_params->state.wq, &dma_job.base);
			if (res != DOCA_SUCCESS) {
				doca_buf_refcount_rm(dst_doca_buf, NULL);
				doca_buf_refcount_rm(src_doca_buf, NULL);
				send_end_ack_to_remote(dma_params);
				goto end;
			}

			if ((res = poll_wq(dma_params->state.wq, &event) != DOCA_SUCCESS)) {
				doca_buf_refcount_rm(dst_doca_buf, NULL);
				doca_buf_refcount_rm(src_doca_buf, NULL);
				send_end_ack_to_remote(dma_params);
				goto end;
			}
		}

		// real dma opts
		for (size_t i = 0; i < inters; i++) {
			dma_params->tposted[i] = get_cycles();
			/* Enqueue DMA job */
			res = doca_workq_submit(dma_params->state.wq, &dma_job.base);
			if (res != DOCA_SUCCESS) {
				doca_buf_refcount_rm(dst_doca_buf, NULL);
				doca_buf_refcount_rm(src_doca_buf, NULL);
				send_end_ack_to_remote(dma_params);
				goto end;
			}

			if ((res = poll_wq(dma_params->state.wq, &event) != DOCA_SUCCESS)) {
				doca_buf_refcount_rm(dst_doca_buf, NULL);
				doca_buf_refcount_rm(src_doca_buf, NULL);
				send_end_ack_to_remote(dma_params);
				goto end;
			}
		}

		if (doca_buf_refcount_rm(src_doca_buf, NULL) | doca_buf_refcount_rm(dst_doca_buf, NULL))
			DOCA_LOG_ERR("Failed to decrease DOCA buffer reference count");
		send_end_ack_to_remote(dma_params);
	}
	else {
		res = init_remote_core_objects(&(dma_params->state));
		if (res != DOCA_SUCCESS) {
			DOCA_LOG_ERR("init_remote_core_objects false");
			goto end;
		}

		res = populate_mmap(dma_params->state.provide_for_remote_mmap, src_buf, length, dma_params->pg_sz);
		if (res != DOCA_SUCCESS) {
			DOCA_LOG_ERR("populate_mmap false");
			goto end;
		}	

		/* Export DOCA mmap to enable DMA */
		res = doca_mmap_export(dma_params->state.provide_for_remote_mmap, dma_params->state.dev, 
							   (uint8_t **)&(dma_params->export_str), &export_str_len);
		if (res != DOCA_SUCCESS) {
			DOCA_LOG_ERR("doca_mmap_export false");
			goto end;
		}

		if (!send_dma_info_to_remote(dma_params->tcp_fd, dma_params->export_str, export_str_len)) {
			DOCA_LOG_ERR("send_dma_info_to_remote false");
			res =  DOCA_ERROR_NOT_CONNECTED;
			goto end;
		}

		wait_remote_end_ack(dma_params);
	}

end:
	cleanup_objects(&(dma_params->state));
	destroy_objects(&(dma_params->state));
	free(src_buf);
	return res;
}

doca_error_t run_remote_dma_write_lat(struct dma_parameters *dma_params) {
	if(dma_params->is_server) {
		if(!server_accept(&(dma_params->tcp_fd), dma_params->port)) {
			DOCA_LOG_ERR("Can not accept to client");
			return DOCA_ERROR_NOT_CONNECTED;
		}
	}
	else {
		if(!(client_connect(&(dma_params->tcp_fd), dma_params->ip, dma_params->port))) {
			DOCA_LOG_ERR("Can not connoct to server");
			return DOCA_ERROR_NOT_CONNECTED;
		}
	}

	doca_error_t res;
	size_t export_str_len;

	res = open_local_device(&(dma_params->pcie_addr), &(dma_params->state));
	if (res != DOCA_SUCCESS){
		DOCA_LOG_ERR("open_local_device false");
		return res;
	}

	size_t length = dma_params->size;
	volatile char *send_buf = (char*)malloc(length);
	volatile char *recv_buf = (char*)malloc(length);

	res = init_remote_core_objects(&(dma_params->state));
	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("init_remote_core_objects false");
		goto end;
	}

	res = populate_mmap(dma_params->state.provide_for_remote_mmap, recv_buf, length, dma_params->pg_sz);
	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("populate_mmap false");
		goto end;
	}	

	/* Export DOCA mmap to enable DMA */
	res = doca_mmap_export(dma_params->state.provide_for_remote_mmap, dma_params->state.dev, 
						   (uint8_t **)&(dma_params->export_str), &export_str_len);
	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("doca_mmap_export false");
		goto end;
	}

    // Exchange of information
	if(dma_params->is_server) {
		if (!receive_dma_info_from_remote(dma_params->tcp_fd, dma_params->export_json, 1024, 
						&(dma_params->remote_addr), &(dma_params->remote_addr_len))) {
			DOCA_LOG_ERR("receive_dma_info_from_remote false");
			res = DOCA_ERROR_NOT_CONNECTED;
			goto end;
		}

		if (!send_dma_info_to_remote(dma_params->tcp_fd, dma_params->export_str, export_str_len)) {
			DOCA_LOG_ERR("send_dma_info_to_remote false");
			res =  DOCA_ERROR_NOT_CONNECTED;
			goto end;
		}
		// There is a temporary unknown problem, 
		// and the Server needs to sleep before performing the DMA operation, 
		// otherwise the Server may perform DMA operations on the wrong remote address.
		// Currently, it has been observed that the success or failure of running the remote_dma_write_lat program 
		// is related to the IP address used with the -a parameter 
		// (the same server can have multiple IP addresses).
		sleep(1);
	}
	else {
		if (!send_dma_info_to_remote(dma_params->tcp_fd, dma_params->export_str, export_str_len)) {
			DOCA_LOG_ERR("send_dma_info_to_remote false");
			res =  DOCA_ERROR_NOT_CONNECTED;
			goto end;
		}

		if (!receive_dma_info_from_remote(dma_params->tcp_fd, dma_params->export_json, 1024, 
						&(dma_params->remote_addr), &(dma_params->remote_addr_len))) {
			DOCA_LOG_ERR("receive_dma_info_from_remote false");
			res = DOCA_ERROR_NOT_CONNECTED;
			goto end;
		}
	}

	struct doca_event event = {0};
	struct doca_job doca_job = {0};
	struct doca_dma_job_memcpy dma_job = {0};
	struct doca_buf *src_doca_buf;
	struct doca_buf *dst_doca_buf;
	uint64_t                scnt = 0;
	uint64_t                rcnt = 0;

	res = create_core_objects(&(dma_params->state), dma_params->num_elements, dma_params->wq_depth);
	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("create_core_objects false");
		goto end;
	}

	res = init_core_objects(&(dma_params->state), dma_params->max_chunks);
	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("init_core_objects false");
		goto end;
	}	

	res = populate_mmap(dma_params->state.local_mmap, send_buf, length, dma_params->pg_sz);
	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("populate_mmap false");
		goto end;
	}

	/* Create a local DOCA mmap from exported data */
	res = doca_mmap_create_from_export("remote_mmap", (uint8_t *)(dma_params->export_json), strlen(dma_params->export_json) + 1, 
						dma_params->state.dev, &(dma_params->state.remote_mmap));			   
	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("doca_mmap_create_from_export false: %s", doca_get_error_string(res));
		goto end;
	}

	/* Construct DOCA buffer for each address range */
	res = doca_buf_inventory_buf_by_addr(dma_params->state.buf_inv, dma_params->state.remote_mmap, 
	  									 dma_params->remote_addr, dma_params->remote_addr_len, &dst_doca_buf);
	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to acquire DOCA buffer representing remote buffer: %s", doca_get_error_string(res));
		goto end;
	}

	/* Construct DOCA buffer for each address range */
	res = doca_buf_inventory_buf_by_addr(dma_params->state.buf_inv, dma_params->state.local_mmap, send_buf, length, &src_doca_buf);
	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to acquire DOCA buffer representing destination buffer: %s", doca_get_error_string(res));
		doca_buf_refcount_rm(dst_doca_buf, NULL);
		goto end;
	}

	/* Construct DMA job */
	doca_job.type = DOCA_DMA_JOB_MEMCPY;
	doca_job.flags = DOCA_JOB_FLAGS_NONE;
	doca_job.ctx = dma_params->state.ctx;

	dma_job.base = doca_job;
	dma_job.dst_buff = dst_doca_buf;
	dma_job.src_buff = src_doca_buf;
	dma_job.num_bytes_to_copy = length;

	size_t inters = dma_params->iters;
	size_t preheats = dma_params->preheats;

	recv_buf[0] = (char)rcnt; 
	send_buf[0] = (char)scnt; 

	// warm up
	DOCA_LOG_INFO("run warm up");
	while(rcnt < preheats) {
		if(!(scnt == 0 && dma_params->is_server == 1) && (rcnt < preheats)){
			rcnt++;
			while (recv_buf[0] != (char)rcnt){
			}
		}
		if(scnt < preheats) {
			send_buf[0] = (char)++scnt;
			/* Enqueue DMA job */

			res = doca_workq_submit(dma_params->state.wq, &dma_job.base);
			if (res != DOCA_SUCCESS) {
				doca_buf_refcount_rm(dst_doca_buf, NULL);
				doca_buf_refcount_rm(src_doca_buf, NULL);
				goto end;
			}

			if ((res = poll_wq(dma_params->state.wq, &event) != DOCA_SUCCESS)) {
				doca_buf_refcount_rm(dst_doca_buf, NULL);
				doca_buf_refcount_rm(src_doca_buf, NULL);
				goto end;
			}
		}
	}

	rcnt = 0;
	scnt = 0;
	recv_buf[0] = (char)rcnt;
	send_buf[0] = (char)scnt;

	// real dma opts
	DOCA_LOG_INFO("run real opt");
	while(rcnt < inters) {
		if(!(scnt == 0 && dma_params->is_server == 1) && (rcnt < inters)){
			rcnt++;
			while (recv_buf[0] != (char)rcnt);
		}
		if(scnt < inters) {
			dma_params->tposted[scnt] = get_cycles();
			send_buf[0] = (char)++scnt;
			/* Enqueue DMA job */
			res = doca_workq_submit(dma_params->state.wq, &dma_job.base);
			if (res != DOCA_SUCCESS) {
				doca_buf_refcount_rm(dst_doca_buf, NULL);
				doca_buf_refcount_rm(src_doca_buf, NULL);
				goto end;
			}

			if ((res = poll_wq(dma_params->state.wq, &event) != DOCA_SUCCESS)) {
				doca_buf_refcount_rm(dst_doca_buf, NULL);
				doca_buf_refcount_rm(src_doca_buf, NULL);
				goto end;
			}
		}
	}

	if (doca_buf_refcount_rm(src_doca_buf, NULL) | doca_buf_refcount_rm(dst_doca_buf, NULL))
		DOCA_LOG_ERR("Failed to decrease DOCA buffer reference count");

end:
	cleanup_objects(&(dma_params->state));
	destroy_objects(&(dma_params->state));
	free(recv_buf);
	free(send_buf);
	return res;
}

doca_error_t run_local_dma_read_lat(struct dma_parameters *dma_params) {
	
	struct doca_event event = {0};
	struct doca_job doca_job = {0};
	struct doca_dma_job_memcpy dma_job = {0};
	struct doca_buf *src_doca_buf;
	struct doca_buf *dst_doca_buf;
	doca_error_t res;

	res = open_local_device(&(dma_params->pcie_addr), &(dma_params->state));
	if (res != DOCA_SUCCESS){
		DOCA_LOG_ERR("open_local_device false");
		return res;
	}

	size_t length = dma_params->size;
	void *src_buf = malloc(length);
	void *dst_buf = malloc(length);

	res = create_core_objects(&(dma_params->state), dma_params->num_elements, dma_params->wq_depth);
	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("create_core_objects false");
		goto end;
	}

	res = init_core_objects(&(dma_params->state), dma_params->max_chunks);
	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("init_core_objects false");
		goto end;
	}	

	res = populate_mmap(dma_params->state.local_mmap, dst_buf, length, dma_params->pg_sz) |
	      populate_mmap(dma_params->state.local_mmap, src_buf, length, dma_params->pg_sz);

	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("populate_mmap false");
		goto end;
	}

	/* Construct DOCA buffer for each address range */
	res = doca_buf_inventory_buf_by_addr(dma_params->state.buf_inv, dma_params->state.local_mmap, src_buf, length, &src_doca_buf);
	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to acquire DOCA buffer representing source buffer: %s", doca_get_error_string(res));
		goto end;
	}

	/* Construct DOCA buffer for each address range */
	res = doca_buf_inventory_buf_by_addr(dma_params->state.buf_inv, dma_params->state.local_mmap, dst_buf, length, &dst_doca_buf);
	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to acquire DOCA buffer representing destination buffer: %s", doca_get_error_string(res));
		doca_buf_refcount_rm(src_doca_buf, NULL);
		goto end;
	}	

	/* Construct DMA job */
	doca_job.type = DOCA_DMA_JOB_MEMCPY;
	doca_job.flags = DOCA_JOB_FLAGS_NONE;
	doca_job.ctx = dma_params->state.ctx;
	dma_job.base = doca_job;
	dma_job.dst_buff = src_doca_buf;
 	dma_job.src_buff = dst_doca_buf;
	dma_job.num_bytes_to_copy = length;

	size_t inters = dma_params->iters;
	size_t preheats = dma_params->preheats;

	// warm up
	for (size_t i = 0; i < preheats; i++) {
		/* Enqueue DMA job */
		res = doca_workq_submit(dma_params->state.wq, &dma_job.base);
		if (res != DOCA_SUCCESS) {
			doca_buf_refcount_rm(dst_doca_buf, NULL);
			doca_buf_refcount_rm(src_doca_buf, NULL);
			goto end;
		}

		if ((res = poll_wq(dma_params->state.wq, &event) != DOCA_SUCCESS)) {
				doca_buf_refcount_rm(dst_doca_buf, NULL);
				doca_buf_refcount_rm(src_doca_buf, NULL);
				goto end;
		}
	}
	// real dma opts
	for (size_t i = 0; i < inters; i++) {
		dma_params->tposted[i] = get_cycles();
		/* Enqueue DMA job */
		res = doca_workq_submit(dma_params->state.wq, &dma_job.base);
		if (res != DOCA_SUCCESS) {
			doca_buf_refcount_rm(dst_doca_buf, NULL);
			doca_buf_refcount_rm(src_doca_buf, NULL);
			goto end;
		}

		if ((res = poll_wq(dma_params->state.wq, &event) != DOCA_SUCCESS)) {
				doca_buf_refcount_rm(dst_doca_buf, NULL);
				doca_buf_refcount_rm(src_doca_buf, NULL);
				goto end;
		}
	}
	if (doca_buf_refcount_rm(src_doca_buf, NULL) | doca_buf_refcount_rm(dst_doca_buf, NULL))
		DOCA_LOG_ERR("Failed to decrease DOCA buffer reference count");

end:
	cleanup_objects(&(dma_params->state));
	destroy_objects(&(dma_params->state));
	free(src_buf);
	free(dst_buf);
	return res;
}

void *run_local_dma_write_lat(struct local_write_lat_parameters *local_write_lat_params) {
	struct doca_event event = {0};
	struct doca_job doca_job = {0};
	struct doca_dma_job_memcpy dma_job = {0};
	struct doca_buf *src_doca_buf;
	struct doca_buf *dst_doca_buf;
	uint64_t                scnt = 0;
	uint64_t                rcnt = 0;
	doca_error_t res;
	size_t length = local_write_lat_params->dma_params.size;

	res = open_local_device(&(local_write_lat_params->dma_params.pcie_addr), &(local_write_lat_params->dma_params.state));
	if (res != DOCA_SUCCESS){
		DOCA_LOG_ERR("open_local_device false");
		local_write_lat_params->res = res;
		return 0;
	}

	res = create_core_objects(&(local_write_lat_params->dma_params.state), 
								local_write_lat_params->dma_params.num_elements, 
								local_write_lat_params->dma_params.wq_depth);
	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("create_core_objects false");
		goto end;
	}

	res = init_core_objects(&(local_write_lat_params->dma_params.state), local_write_lat_params->dma_params.max_chunks);
	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("init_core_objects false");
		goto end;
	}	

	res = populate_mmap(local_write_lat_params->dma_params.state.local_mmap, 
						local_write_lat_params->dst_buf, 
						length, 
						local_write_lat_params->dma_params.pg_sz) |
	      populate_mmap(local_write_lat_params->dma_params.state.local_mmap, 
		  				local_write_lat_params->send_buf, 
						length, 
						local_write_lat_params->dma_params.pg_sz);

	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("populate_mmap false");
		goto end;
	}

	/* Construct DOCA buffer for each address range */
	res = doca_buf_inventory_buf_by_addr(local_write_lat_params->dma_params.state.buf_inv, 
										 local_write_lat_params->dma_params.state.local_mmap, 
										 local_write_lat_params->send_buf, 
										 length, 
										 &src_doca_buf);
	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to acquire DOCA buffer representing source buffer: %s", doca_get_error_string(res));
		goto end;
	}

	/* Construct DOCA buffer for each address range */
	res = doca_buf_inventory_buf_by_addr(local_write_lat_params->dma_params.state.buf_inv, 
										 local_write_lat_params->dma_params.state.local_mmap, 
										 local_write_lat_params->dst_buf, 
										 length, 
										 &dst_doca_buf);
	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to acquire DOCA buffer representing destination buffer: %s", doca_get_error_string(res));
		doca_buf_refcount_rm(src_doca_buf, NULL);
		goto end;
	}	

	/* Construct DMA job */
	doca_job.type = DOCA_DMA_JOB_MEMCPY;
	doca_job.flags = DOCA_JOB_FLAGS_NONE;
	doca_job.ctx = local_write_lat_params->dma_params.state.ctx;

	dma_job.base = doca_job;
	dma_job.dst_buff = dst_doca_buf;
 	dma_job.src_buff = src_doca_buf;
	dma_job.num_bytes_to_copy = length;

	size_t inters =  local_write_lat_params->dma_params.iters;
	size_t preheats = local_write_lat_params->dma_params.preheats;

	local_write_lat_params->recv_buf[0] = (char)rcnt; 
	local_write_lat_params->send_buf[0] = (char)scnt; 

	// warm up
	DOCA_LOG_INFO("run warm up");
	while(rcnt < preheats) {
		if(!(scnt == 0 && local_write_lat_params->dma_params.is_server == 1) && (rcnt < preheats)){
			rcnt++;
			while (local_write_lat_params->recv_buf[0] != (char)rcnt){}
		}
		if(scnt < preheats) {
			local_write_lat_params->send_buf[0] = (char)++scnt;
			/* Enqueue DMA job */
			res = doca_workq_submit(local_write_lat_params->dma_params.state.wq, &dma_job.base);
			if (res != DOCA_SUCCESS) {
				doca_buf_refcount_rm(dst_doca_buf, NULL);
				doca_buf_refcount_rm(src_doca_buf, NULL);
				goto end;
			}

			if ((res = poll_wq(local_write_lat_params->dma_params.state.wq, &event) != DOCA_SUCCESS)) {
				doca_buf_refcount_rm(dst_doca_buf, NULL);
				doca_buf_refcount_rm(src_doca_buf, NULL);
				goto end;
			}
		}
	}

	rcnt = 0;
	scnt = 0;
	
	// real dma opts
	DOCA_LOG_INFO("run real opt");
	while(rcnt < inters) {
		if(!(scnt == 0 && local_write_lat_params->dma_params.is_server == 1) && (rcnt < inters)){
			rcnt++;
			while (local_write_lat_params->recv_buf[0] != (char)rcnt);
		}
		if(scnt < inters) {
			local_write_lat_params->dma_params.tposted[scnt] = get_cycles();
			local_write_lat_params->send_buf[0] = (char)++scnt;
			/* Enqueue DMA job */
			res = doca_workq_submit(local_write_lat_params->dma_params.state.wq, &dma_job.base);
			if (res != DOCA_SUCCESS) {
				doca_buf_refcount_rm(dst_doca_buf, NULL);
				doca_buf_refcount_rm(src_doca_buf, NULL);
				goto end;
			}

			if ((res = poll_wq(local_write_lat_params->dma_params.state.wq, &event) != DOCA_SUCCESS)) {
				doca_buf_refcount_rm(dst_doca_buf, NULL);
				doca_buf_refcount_rm(src_doca_buf, NULL);
				goto end;
			}
		}
	}

	if (doca_buf_refcount_rm(src_doca_buf, NULL) | doca_buf_refcount_rm(dst_doca_buf, NULL))
		DOCA_LOG_ERR("Failed to decrease DOCA buffer reference count");

end:
	cleanup_objects(&(local_write_lat_params->dma_params.state));
	destroy_objects(&(local_write_lat_params->dma_params.state));
	local_write_lat_params->res = res;
	return 0;
}

void *run_local_dma_bw(struct bw_parameters *bw_params) {
	struct doca_event event = {0};
	struct doca_job doca_job = {0};
	struct doca_dma_job_memcpy dma_job = {0};
	struct doca_buf *src_doca_buf;
	struct doca_buf *dst_doca_buf;
	doca_error_t res;

	if (bw_params->dma_params.is_bind_core == 1) {
		if(!bind_thread_to_core(bw_params->id)) {
			bw_params->res = DOCA_ERROR_UNKNOWN;
			return 0;
		}
	}

	size_t length = bw_params->dma_params.size;
	void *src_buf = malloc(length);
	void *dst_buf = malloc(length);

	res = open_local_device(&(bw_params->dma_params.pcie_addr), &(bw_params->dma_params.state));
	if (res != DOCA_SUCCESS){
		DOCA_LOG_ERR("open_local_device false");
		bw_params->res = res;
		return 0;
	}

	res = create_core_objects(&(bw_params->dma_params.state), 
								bw_params->dma_params.num_elements, 
								bw_params->dma_params.wq_depth);
	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("create_core_objects false");
		goto end;
	}

	res = init_core_objects(&(bw_params->dma_params.state), bw_params->dma_params.max_chunks);
	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("init_core_objects false");
		goto end;
	}		

	res = populate_mmap(bw_params->dma_params.state.local_mmap, 
						dst_buf, 
						length, 
						bw_params->dma_params.pg_sz) |
	      populate_mmap(bw_params->dma_params.state.local_mmap, 
		  				src_buf, 
						length, 
						bw_params->dma_params.pg_sz);

	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("populate_mmap false");
		goto end;
	}

	/* Construct DOCA buffer for each address range */
	res = doca_buf_inventory_buf_by_addr(bw_params->dma_params.state.buf_inv, 
										 bw_params->dma_params.state.local_mmap, 
										 src_buf, 
										 length, 
										 &src_doca_buf);
	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to acquire DOCA buffer representing source buffer: %s", doca_get_error_string(res));
		goto end;
	}

	/* Construct DOCA buffer for each address range */
	res = doca_buf_inventory_buf_by_addr(bw_params->dma_params.state.buf_inv, 
										 bw_params->dma_params.state.local_mmap, 
										 dst_buf, 
										 length, 
										 &dst_doca_buf);
	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to acquire DOCA buffer representing destination buffer: %s", doca_get_error_string(res));
		doca_buf_refcount_rm(src_doca_buf, NULL);
		goto end;
	}	

	/* Construct DMA job */
	doca_job.type = DOCA_DMA_JOB_MEMCPY;
	doca_job.flags = DOCA_JOB_FLAGS_NONE;
	doca_job.ctx = bw_params->dma_params.state.ctx;
	dma_job.base = doca_job;

	if (bw_params->dma_params.dma_type == READ) {
		dma_job.dst_buff = src_doca_buf;
 		dma_job.src_buff = dst_doca_buf;
	}
	else {
		dma_job.dst_buff = dst_doca_buf;
		dma_job.src_buff = src_doca_buf;
	}

	dma_job.num_bytes_to_copy = length;

	size_t inters =  bw_params->dma_params.iters;
	size_t preheats = bw_params->dma_params.preheats;

	uint32_t batch_num = bw_params->dma_params.wq_depth;
	size_t i = 0;

	// warm up
	while(i < preheats) {
		for (size_t j = 0; j < batch_num; j++) {
			/* Enqueue DMA job */
			res = doca_workq_submit(bw_params->dma_params.state.wq, &dma_job.base);
			if (res != DOCA_SUCCESS) {
				doca_buf_refcount_rm(dst_doca_buf, NULL);
				doca_buf_refcount_rm(src_doca_buf, NULL);
				goto end;
			}
			i++;
		}
		for (size_t j = 0; j < batch_num; j++) {
			if ((res = poll_wq(bw_params->dma_params.state.wq, &event) != DOCA_SUCCESS)) {
					doca_buf_refcount_rm(dst_doca_buf, NULL);
					doca_buf_refcount_rm(src_doca_buf, NULL);
					goto end;
			}
		}
	}

	// real dma opts
	bw_params->dma_params.start_time = get_cycles();
	while(i < inters) {
		for (size_t j = 0; j < batch_num; j++) {
			/* Enqueue DMA job */
			res = doca_workq_submit(bw_params->dma_params.state.wq, &dma_job.base);
			if (res != DOCA_SUCCESS) {
				doca_buf_refcount_rm(dst_doca_buf, NULL);
				doca_buf_refcount_rm(src_doca_buf, NULL);
				goto end;
			}
			i++;
		}
		for (size_t j = 0; j < batch_num; j++) {
			if ((res = poll_wq(bw_params->dma_params.state.wq, &event) != DOCA_SUCCESS)) {
					doca_buf_refcount_rm(dst_doca_buf, NULL);
					doca_buf_refcount_rm(src_doca_buf, NULL);
					goto end;
			}
		}
	}
	bw_params->dma_params.end_time = get_cycles();
	if (doca_buf_refcount_rm(src_doca_buf, NULL) | doca_buf_refcount_rm(dst_doca_buf, NULL))
		DOCA_LOG_ERR("Failed to decrease DOCA buffer reference count");

end:
	cleanup_objects(&(bw_params->dma_params.state));
	destroy_objects(&(bw_params->dma_params.state));
	free(src_buf);
	free(dst_buf);
	bw_params->res = res;
	return 0;
}


void *run_remote_dma_bw(struct bw_parameters *bw_params) {
	if (bw_params->dma_params.is_server) {
		if(!server_accept(&(bw_params->dma_params.tcp_fd), bw_params->dma_params.port)) {
			DOCA_LOG_ERR("Can not accept to client");
			return DOCA_ERROR_NOT_CONNECTED;
		}
	}
	else {
		if (!(client_connect(&(bw_params->dma_params.tcp_fd), bw_params->dma_params.ip, bw_params->dma_params.port))) {
			DOCA_LOG_ERR("Can not connoct to server");
			return DOCA_ERROR_NOT_CONNECTED;
		}
	}

	if (bw_params->dma_params.is_bind_core == 1) {
		if (!bind_thread_to_core(bw_params->id)) {
			bw_params->res = DOCA_ERROR_UNKNOWN;
			return 0;
		}
	}

	doca_error_t res;
	size_t export_str_len;

	res = open_local_device(&(bw_params->dma_params.pcie_addr), &(bw_params->dma_params.state));
	if (res != DOCA_SUCCESS){
		DOCA_LOG_ERR("open_local_device false");
		bw_params->res = res;
		return 0;
	}

	size_t length = bw_params->dma_params.size;
	void *src_buf = malloc(length);

	if (bw_params->dma_params.is_server) {
		if (!receive_dma_info_from_remote(bw_params->dma_params.tcp_fd, bw_params->dma_params.export_json, 1024, 
						&(bw_params->dma_params.remote_addr), &(bw_params->dma_params.remote_addr_len))) {
			DOCA_LOG_ERR("receive_dma_info_from_remote false");
			res = DOCA_ERROR_NOT_CONNECTED;
			goto end;
		}

		struct doca_event event = {0};
		struct doca_job doca_job = {0};
		struct doca_dma_job_memcpy dma_job = {0};
		struct doca_buf *src_doca_buf;
		struct doca_buf *dst_doca_buf;

		res = create_core_objects(&(bw_params->dma_params.state), 
									bw_params->dma_params.num_elements, 
									bw_params->dma_params.wq_depth);
		if (res != DOCA_SUCCESS) {
			DOCA_LOG_ERR("create_core_objects false");
			send_end_ack_to_remote(&(bw_params->dma_params));
			goto end;
		}

		res = init_core_objects(&(bw_params->dma_params.state), bw_params->dma_params.max_chunks);
		if (res != DOCA_SUCCESS) {
			DOCA_LOG_ERR("init_core_objects false");
			send_end_ack_to_remote(&(bw_params->dma_params));
			goto end;
		}	

		res = populate_mmap(bw_params->dma_params.state.local_mmap, 
							src_buf, 
							length, 
							bw_params->dma_params.pg_sz);	
		if (res != DOCA_SUCCESS) {
			DOCA_LOG_ERR("populate_mmap false");
			send_end_ack_to_remote(&(bw_params->dma_params));
			goto end;
		}

		/* Create a local DOCA mmap from exported data */
		res = doca_mmap_create_from_export("remote_mmap", 
										   (uint8_t *)(bw_params->dma_params.export_json), 
										   strlen(bw_params->dma_params.export_json) + 1, 
										   bw_params->dma_params.state.dev, 
										   &(bw_params->dma_params.state.remote_mmap));			   
		if (res != DOCA_SUCCESS) {
			DOCA_LOG_ERR("doca_mmap_create_from_export false: %s", doca_get_error_string(res));
			send_end_ack_to_remote(&(bw_params->dma_params));
			goto end;
		}

		/* Construct DOCA buffer for each address range */
		res = doca_buf_inventory_buf_by_addr(bw_params->dma_params.state.buf_inv, 
											 bw_params->dma_params.state.remote_mmap, 
											 bw_params->dma_params.remote_addr, 
											 bw_params->dma_params.remote_addr_len, 
											 &dst_doca_buf);
		if (res != DOCA_SUCCESS) {
			DOCA_LOG_ERR("Unable to acquire DOCA buffer representing remote buffer: %s", doca_get_error_string(res));
			send_end_ack_to_remote(&(bw_params->dma_params));
			goto end;
		}

		/* Construct DOCA buffer for each address range */
		res = doca_buf_inventory_buf_by_addr(bw_params->dma_params.state.buf_inv, 
											 bw_params->dma_params.state.local_mmap, 
											 src_buf, 
											 length, 
											 &src_doca_buf);
		if (res != DOCA_SUCCESS) {
			DOCA_LOG_ERR("Unable to acquire DOCA buffer representing source buffer: %s", doca_get_error_string(res));
			doca_buf_refcount_rm(dst_doca_buf, NULL);
			send_end_ack_to_remote(&(bw_params->dma_params));
			goto end;
		}

		/* Construct DMA job */
		doca_job.type = DOCA_DMA_JOB_MEMCPY;
		doca_job.flags = DOCA_JOB_FLAGS_NONE;
		doca_job.ctx = bw_params->dma_params.state.ctx;
		dma_job.base = doca_job;

		if (bw_params->dma_params.dma_type == READ) {
			dma_job.dst_buff = src_doca_buf;
			dma_job.src_buff = dst_doca_buf;
		}
		else {
			dma_job.dst_buff = dst_doca_buf;
			dma_job.src_buff = src_doca_buf;
		}
		dma_job.num_bytes_to_copy = length;

		size_t inters =  bw_params->dma_params.iters;
		size_t preheats = bw_params->dma_params.preheats;

		uint32_t batch_num = bw_params->dma_params.wq_depth;
		size_t i = 0;

		// warm up
		while(i < preheats) {
			for (size_t j = 0; j < batch_num; j++) {
				/* Enqueue DMA job */
				res = doca_workq_submit(bw_params->dma_params.state.wq, &dma_job.base);
				if (res != DOCA_SUCCESS) {
					doca_buf_refcount_rm(dst_doca_buf, NULL);
					doca_buf_refcount_rm(src_doca_buf, NULL);
					send_end_ack_to_remote(&(bw_params->dma_params));
					goto end;
				}
				i++;
			}
			for (size_t j = 0; j < batch_num; j++) {
				if ((res = poll_wq(bw_params->dma_params.state.wq, &event) != DOCA_SUCCESS)) {
						doca_buf_refcount_rm(dst_doca_buf, NULL);
						doca_buf_refcount_rm(src_doca_buf, NULL);
						send_end_ack_to_remote(&(bw_params->dma_params));
						goto end;
				}
			}
		}

		// real dma opts
		bw_params->dma_params.start_time = get_cycles();
		while(i < inters) {
			for (size_t j = 0; j < batch_num; j++) {
				/* Enqueue DMA job */
				res = doca_workq_submit(bw_params->dma_params.state.wq, &dma_job.base);
				if (res != DOCA_SUCCESS) {
					doca_buf_refcount_rm(dst_doca_buf, NULL);
					doca_buf_refcount_rm(src_doca_buf, NULL);
					send_end_ack_to_remote(&(bw_params->dma_params));
					goto end;
				}
				i++;
			}
			for (size_t j = 0; j < batch_num; j++) {
				if ((res = poll_wq(bw_params->dma_params.state.wq, &event) != DOCA_SUCCESS)) {
						doca_buf_refcount_rm(dst_doca_buf, NULL);
						doca_buf_refcount_rm(src_doca_buf, NULL);
						send_end_ack_to_remote(&(bw_params->dma_params));
						goto end;
				}
			}
		}
		bw_params->dma_params.end_time = get_cycles();
		if (doca_buf_refcount_rm(src_doca_buf, NULL) | doca_buf_refcount_rm(dst_doca_buf, NULL))
			DOCA_LOG_ERR("Failed to decrease DOCA buffer reference count");
		send_end_ack_to_remote(&(bw_params->dma_params));
	}
	else {
		res = init_remote_core_objects(&(bw_params->dma_params.state));
		if (res != DOCA_SUCCESS) {
			DOCA_LOG_ERR("init_remote_core_objects false");
			goto end;
		}

		res = populate_mmap(bw_params->dma_params.state.provide_for_remote_mmap, 
							src_buf, length, bw_params->dma_params.pg_sz);
		if (res != DOCA_SUCCESS) {
			DOCA_LOG_ERR("populate_mmap false");
			goto end;
		}	

		/* Export DOCA mmap to enable DMA */
		res = doca_mmap_export(bw_params->dma_params.state.provide_for_remote_mmap, 
							   bw_params->dma_params.state.dev, 
							   (uint8_t **)&(bw_params->dma_params.export_str), 
							   &export_str_len);
		if (res != DOCA_SUCCESS) {
			DOCA_LOG_ERR("doca_mmap_export false");
			goto end;
		}

		if (!send_dma_info_to_remote(bw_params->dma_params.tcp_fd, 
									 bw_params->dma_params.export_str, export_str_len)) {
			DOCA_LOG_ERR("send_dma_info_to_remote false");
			res =  DOCA_ERROR_NOT_CONNECTED;
			goto end;
		}
		wait_remote_end_ack(&(bw_params->dma_params));
	}
	
end:
	cleanup_objects(&(bw_params->dma_params.state));
	destroy_objects(&(bw_params->dma_params.state));
	free(src_buf);
	bw_params->res = res;
	return 0;
}
