/*
 * Created by Hxk
 * 2022/10/18
 */

#include "dma_communication.h"
#include <errno.h>

// Convert port to string 
// size_t num : the port being converted
// char *str : converted string
void port_2_str(size_t num, char *str) {

	char tmp_s[6] = {0}; 
	// printf("num is %ld\n", num);
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

	// printf("str is %s\n", str);
}

bool send_dma_info_to_remote(char *ip, 
                             uint16_t port, 
                             char *export_str, 
                             size_t export_str_len){
    struct sockaddr_in addr;
	struct timeval timeout = {
		.tv_sec = 5,
	};
    int ret;

    tcp_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(tcp_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

	addr.sin_addr.s_addr = inet_addr(ip);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	if (connect(tcp_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		DOCA_LOG_ERR("Couldn't establish a connection to remote node");
		close(tcp_fd);
		return false;
	}

    ret = write(tcp_fd, export_str, export_str_len);
	if (ret != (int)export_str_len) {
		DOCA_LOG_ERR("Failed to send data to remote node");
		close(tcp_fd);
		return false;
	}

    DOCA_LOG_INFO("Send dma info successfully");
	// DOCA_LOG_INFO("Message is %s", export_str);
    return true;
}

bool receive_dma_info_from_remote(const size_t port, 
                                  char *export_buffer, 
                                  size_t export_buffer_len,
                                  char **remote_addr,
			                      size_t *remote_addr_len){                          

	struct json_object *from_export_json;
	struct json_object *addr;
	struct json_object *len;
	struct addrinfo *res, *it;
	struct addrinfo hints = {
		.ai_flags = AI_PASSIVE,
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM
	};    
	int receiver_fd = -1;
	int bytes_ret;
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

	DOCA_LOG_INFO("Waiting for remote node to send exported data");

    tcp_fd = accept(receiver_fd, NULL, 0);

    close(receiver_fd);

    if (tcp_fd < 0) {
        DOCA_LOG_ERR("Connection acceptance failed");
        return false;
    }

    bytes_ret = recv(tcp_fd, export_buffer, export_buffer_len, 0);

    if (bytes_ret == -1) {
        DOCA_LOG_ERR("Couldn't receive data from remote node");
        close(tcp_fd);
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

void send_end_ack_to_remote(){
    int ret;
	char ack_buffer[] = "DMA operation was completed";
	int length = strlen(ack_buffer) + 1;

	ret = write(tcp_fd, ack_buffer, length);
	if (ret != length)
		DOCA_LOG_ERR("Failed to send ack message to sender node");

	close(tcp_fd);
}

void wait_remote_end_ack(){
    char ack_buffer[1024] = {0};
	char exp_ack[] = "DMA operation was completed";

    /* Waiting for DMA completion signal from receiver */
	DOCA_LOG_INFO("Waiting for remote node to acknowledge DMA operation was ended");

	int res;
	while (1) {
		res = recv(tcp_fd, ack_buffer, sizeof(ack_buffer), 0);
		if(res > 0){
			break;
		}
		else {
			if(res < 0 && (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN)) {
				continue;
			}
			DOCA_LOG_ERR("Failed to receive ack message");
			close(tcp_fd);
			return false;
		}
	}
	
	if (strcmp(exp_ack, ack_buffer)) {
		DOCA_LOG_ERR("Ack message is not correct");
		close(tcp_fd);
		return false;
	}

	DOCA_LOG_INFO("Ack message was received, closing memory mapping");

	close(tcp_fd);
}

// When do dma operations false, call this function to release related resources
void wq_submit_false(struct doca_buf *src_doca_buf, 
					 struct doca_buf *dst_doca_buf,
					 struct doca_mmap *remote_mmap,
					 struct dma_state *state,
					 bool is_local,
					 doca_error_t res) {
	DOCA_LOG_ERR("Failed to do DMA job: %s", doca_get_error_string(res));
	if (doca_buf_refcount_rm(src_doca_buf, NULL) | doca_buf_refcount_rm(dst_doca_buf, NULL))
		DOCA_LOG_ERR("Failed to decrease DOCA buffer reference count");
	if(!is_local) {
		/* Destroy remote memory map */
		if (doca_mmap_destroy(remote_mmap))
			DOCA_LOG_ERR("Failed to destroy remote memory map");
		send_end_ack_to_remote();
	}
	cleanup_core_objects(state);
	destroy_core_objects(state);
}

// Do dma operations.
// In order to reduce the number of "if" statements to ensure the accuracy of the test as much as possible,
// let's make the code a little redundant.
doca_error_t do_dma_operations(struct dma_parameters *dma_params,
					   		   struct dma_state *state,
					           struct doca_dma_job_memcpy *dma_job,
					           struct doca_buf *src_doca_buf, 
					           struct doca_buf *dst_doca_buf,
					           struct doca_mmap *remote_mmap,
					   		   struct doca_event *event) {
	doca_error_t res;
	size_t inters = dma_params->iters;
	size_t preheats = dma_params->preheats;
	bool is_local = (dma_params->is_local == 1);

	// warm up
	for (size_t i = 0; i < preheats; i++) {
		/* Enqueue DMA job */
		res = doca_workq_submit(state->wq, &(dma_job->base));
		if (res != DOCA_SUCCESS) {
			wq_submit_false(dst_doca_buf, src_doca_buf, remote_mmap, state, is_local, res);
			return res;
		}

		/* Wait for job completion */
		while ((res = doca_workq_progress_retrieve(state->wq, event, DOCA_WORKQ_RETRIEVE_FLAGS_NONE)) ==
			DOCA_ERROR_AGAIN) {
				/* Do nothing */
		}

		if (res != DOCA_SUCCESS)
			DOCA_LOG_ERR("Failed to submit DMA job: %s", doca_get_error_string(res));

		/* On DOCA_SUCCESS, Verify DMA job result */
		if (event->result.u64 != DOCA_SUCCESS) {
			DOCA_LOG_ERR("DMA job returned unsuccessfully");
			wq_submit_false(dst_doca_buf, src_doca_buf, remote_mmap, state, is_local, res);
			res = DOCA_ERROR_UNKNOWN;
			return res;
		} 
	}

	if(dma_params->test_type == LAT) {
		for (size_t i = 0; i < inters; i++) {
			
			dma_params->tposted[i] = get_cycles();
			/* Enqueue DMA job */
			res = doca_workq_submit(state->wq, &(dma_job->base));
			if (res != DOCA_SUCCESS) {
				wq_submit_false(dst_doca_buf, src_doca_buf, remote_mmap, state, is_local, res);
				return res;
			}

			/* Wait for job completion */
			while ((res = doca_workq_progress_retrieve(state->wq, event, DOCA_WORKQ_RETRIEVE_FLAGS_NONE)) ==
				DOCA_ERROR_AGAIN) {
					/* Do nothing */
			}

			if (res != DOCA_SUCCESS)
				DOCA_LOG_ERR("Failed to submit DMA job: %s", doca_get_error_string(res));

			/* On DOCA_SUCCESS, Verify DMA job result */
			if (event->result.u64 != DOCA_SUCCESS) {
				DOCA_LOG_ERR("DMA job returned unsuccessfully");
				wq_submit_false(dst_doca_buf, src_doca_buf, remote_mmap, state, is_local, res);
				res = DOCA_ERROR_UNKNOWN;
				return res;
			} 

		}
	}
	else {
		dma_params->start_time = get_cycles();
		for (size_t i = 0; i < inters; i++) {
			/* Enqueue DMA job */
			res = doca_workq_submit(state->wq, &(dma_job->base));
			if (res != DOCA_SUCCESS) {
				wq_submit_false(dst_doca_buf, src_doca_buf, remote_mmap, state, is_local, res);
				return res;
			}

			/* Wait for job completion */
			while ((res = doca_workq_progress_retrieve(state->wq, event, DOCA_WORKQ_RETRIEVE_FLAGS_NONE)) ==
				DOCA_ERROR_AGAIN) {
					/* Do nothing */
			}

			if (res != DOCA_SUCCESS)
				DOCA_LOG_ERR("Failed to submit DMA job: %s", doca_get_error_string(res));

			/* On DOCA_SUCCESS, Verify DMA job result */
			if (event->result.u64 != DOCA_SUCCESS) {
				DOCA_LOG_ERR("DMA job returned unsuccessfully");
				wq_submit_false(dst_doca_buf, src_doca_buf, remote_mmap, state, is_local, res);
				res = DOCA_ERROR_UNKNOWN;
				return res;
			} 
		}
		dma_params->end_time = get_cycles();
	}
	return res;
}

// Because this is only a performance test for latency, 
// the buffer do not memset 0 every time
doca_error_t dma_local_copy(struct dma_parameters *dma_params, 
                            char *dst_buffer, 
                            char *src_buffer, 
                            size_t length){

	struct dma_state state = {0};
	struct doca_event event = {0};
	struct doca_job doca_job = {0};
	struct doca_dma_job_memcpy dma_job = {0};
	struct doca_buf *src_doca_buf;
	struct doca_buf *dst_doca_buf;

	doca_error_t res;

	if (dst_buffer == NULL || src_buffer == NULL || length == 0) {
		DOCA_LOG_ERR("Invalid input values, addresses and sizes must not be 0");
		return DOCA_ERROR_INVALID_VALUE;
	}

	res = open_local_device(&(dma_params->pcie_addr), &state);
	if (res != DOCA_SUCCESS){
		DOCA_LOG_ERR("open_local_device false");
		return res;
	}

	res = create_core_objects(&state, dma_params->num_elements, dma_params->wq_num);;
	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("create_core_objects false");
		destroy_core_objects(&state);
		return res;
	}

	res = init_core_objects(&state, dma_params->max_chunks);
	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("init_core_objects false");
		cleanup_core_objects(&state);
		destroy_core_objects(&state);
		return res;
	}

	res = populate_mmap(state.mmap, dst_buffer, length, dma_params->pg_sz) |
	      populate_mmap(state.mmap, src_buffer, length, dma_params->pg_sz);

	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("populate_mmap false");
		cleanup_core_objects(&state);
		destroy_core_objects(&state);
		return res;
	}

	/* Clear destination memory buffer */
	// memset(dst_buffer, 0, length);

	/* Construct DOCA buffer for each address range */
	res = doca_buf_inventory_buf_by_addr(state.buf_inv, state.mmap, src_buffer, length, &src_doca_buf);
	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to acquire DOCA buffer representing source buffer: %s", doca_get_error_string(res));
		cleanup_core_objects(&state);
		destroy_core_objects(&state);
		return res;
	}

	/* Construct DOCA buffer for each address range */
	res = doca_buf_inventory_buf_by_addr(state.buf_inv, state.mmap, dst_buffer, length, &dst_doca_buf);
	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to acquire DOCA buffer representing destination buffer: %s", doca_get_error_string(res));
		doca_buf_refcount_rm(src_doca_buf, NULL);
		cleanup_core_objects(&state);
		destroy_core_objects(&state);
		return res;
	}

	/* Construct DMA job */
	doca_job.type = DOCA_DMA_JOB_MEMCPY;
	doca_job.flags = DOCA_JOB_FLAGS_NONE;
	doca_job.ctx = state.ctx;

	dma_job.base = doca_job;
	if(dma_params->dma_type == WRITE) {
		dma_job.dst_buff = dst_doca_buf;
	 	dma_job.src_buff = src_doca_buf;
	}
	else {
		dma_job.dst_buff = src_doca_buf;
	 	dma_job.src_buff = dst_doca_buf;
	}
	dma_job.num_bytes_to_copy = length;

	res = do_dma_operations(dma_params,
							&state,
							&dma_job,
							src_doca_buf,
							dst_doca_buf,
							NULL,
							&event);
	
	if (res != DOCA_SUCCESS){
		DOCA_LOG_ERR("Failed to do dma operations: %s", doca_get_error_string(res));
		return res;
	}

	if (doca_buf_refcount_rm(src_doca_buf, NULL) | doca_buf_refcount_rm(dst_doca_buf, NULL))
		DOCA_LOG_ERR("Failed to decrease DOCA buffer reference count");

	/* Clean and destroy all relevant objects */
	cleanup_core_objects(&state);

	destroy_core_objects(&state);

	return res;
}

doca_error_t remote_dma_prepare(struct dma_parameters *dma_params, 
                                char *src_buffer, 
                                size_t length){

	struct dma_state state = {0};
	doca_error_t res;
	char *export_str;
	size_t export_str_len;

	res = open_local_device(&(dma_params->pcie_addr), &state);
	if (res != DOCA_SUCCESS){
		DOCA_LOG_ERR("open_local_device false");
		return res;
	}

	res = init_remote_core_objects(&state);
	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("init_core_objects_sender false");
		destroy_remote_core_objects(&state);
		return res;
	}

	res = populate_mmap(state.mmap, src_buffer, length, dma_params->pg_sz);
	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("populate_mmap false");
		destroy_remote_core_objects(&state);
		return res;
	}

	/* Export DOCA mmap to enable DMA */
	res = doca_mmap_export(state.mmap, state.dev, (uint8_t **)&export_str, &export_str_len);
	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("doca_mmap_export false");
		destroy_remote_core_objects(&state);
		return res;
	}
	
	// DOCA_LOG_INFO("Exported memory buffer content: %s", src_buffer);

	/* Send exported string and wait for ack that DMA was done on receiver node */
	if (!send_dma_info_to_remote(dma_params->ip, dma_params->port, export_str, export_str_len)) {
		DOCA_LOG_ERR("send_dma_info_to_remote false");
		destroy_remote_core_objects(&state);
		free(export_str);
		return DOCA_ERROR_NOT_CONNECTED;
	}
	

	wait_remote_end_ack();

	destroy_remote_core_objects(&state);

	/* Free pre-allocated exported string */
	free(export_str);

	return res;

}

doca_error_t dma_remote_copy(struct dma_parameters *dma_params, 
                            char *src_buffer, 
                            size_t length) {

	struct dma_state state = {0};
	struct doca_event event = {0};
	struct doca_job doca_job = {0};
	struct doca_dma_job_memcpy dma_job = {0};
	struct doca_buf *src_doca_buf;
	struct doca_buf *dst_doca_buf;
	struct doca_mmap *remote_mmap;
	doca_error_t res;

	char export_json[1024] = {0};
	char *remote_addr;
	size_t remote_addr_len;

	res = open_local_device(&(dma_params->pcie_addr), &state);
	if (res != DOCA_SUCCESS){
		DOCA_LOG_ERR("open_local_device false");
		return res;
	}

	res = create_core_objects(&state, dma_params->num_elements, dma_params->wq_num);
	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("create_core_objects false");
		destroy_core_objects(&state);
		return res;
	}

	res = init_core_objects(&state, dma_params->max_chunks);
	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("init_core_objects false");
		cleanup_core_objects(&state);
		destroy_core_objects(&state);
		return res;
	}

	res = populate_mmap(state.mmap, src_buffer, length, dma_params->pg_sz);
	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("populate_mmap false");
		cleanup_core_objects(&state);
		destroy_core_objects(&state);
		return res;
	}

	/* Receive exported data from sender */
	if (!receive_dma_info_from_remote(dma_params->port, export_json, sizeof(export_json) / sizeof(char), &remote_addr,
				      &remote_addr_len)) {
		DOCA_LOG_ERR("receive_dma_info_from_remote false");
		cleanup_core_objects(&state);
		destroy_core_objects(&state);
		send_end_ack_to_remote();
		return DOCA_ERROR_NOT_CONNECTED;
	}

	/* Create a local DOCA mmap from exported data */
	res = doca_mmap_create_from_export("my_mmap", (uint8_t *)export_json, strlen(export_json) + 1, state.dev,
					   &remote_mmap);			   
	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("doca_mmap_create_from_export false: %s", doca_get_error_string(res));
		cleanup_core_objects(&state);
		destroy_core_objects(&state);
		send_end_ack_to_remote();
		return res;
	}

	/* Construct DOCA buffer for each address range */
	// printf("remote_addr is %s\n", remote_addr);
	res = doca_buf_inventory_buf_by_addr(state.buf_inv, remote_mmap, remote_addr, remote_addr_len, &dst_doca_buf);
	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to acquire DOCA buffer representing remote buffer: %s", doca_get_error_string(res));
		doca_mmap_destroy(remote_mmap);
		cleanup_core_objects(&state);
		destroy_core_objects(&state);
		send_end_ack_to_remote();
		return res;
	}

	/* Construct DOCA buffer for each address range */
	res = doca_buf_inventory_buf_by_addr(state.buf_inv, state.mmap, src_buffer, length, &src_doca_buf);
	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to acquire DOCA buffer representing destination buffer: %s", doca_get_error_string(res));
		doca_buf_refcount_rm(src_doca_buf, NULL);
		doca_mmap_destroy(remote_mmap);
		cleanup_core_objects(&state);
		destroy_core_objects(&state);
		send_end_ack_to_remote();
		return res;
	}

	/* Construct DMA job */
	doca_job.type = DOCA_DMA_JOB_MEMCPY;
	doca_job.flags = DOCA_JOB_FLAGS_NONE;
	doca_job.ctx = state.ctx;

	dma_job.base = doca_job;
	if(dma_params->dma_type == WRITE) {
		dma_job.dst_buff = dst_doca_buf;
	 	dma_job.src_buff = src_doca_buf;
	}
	else {
		dma_job.dst_buff = src_doca_buf;
	 	dma_job.src_buff = dst_doca_buf;
	}
	dma_job.num_bytes_to_copy = length;

	res = do_dma_operations(dma_params,
							&state,
							&dma_job,
							src_doca_buf,
							dst_doca_buf,
							remote_mmap,
							&event);
	
	if (res != DOCA_SUCCESS){
		DOCA_LOG_ERR("Failed to do dma operations: %s", doca_get_error_string(res));
		return res;
	}

	if (doca_buf_refcount_rm(src_doca_buf, NULL) | doca_buf_refcount_rm(dst_doca_buf, NULL))
		DOCA_LOG_ERR("Failed to decrease DOCA buffer reference count");

	/* Destroy remote memory map */
	if (doca_mmap_destroy(remote_mmap))
		DOCA_LOG_ERR("Failed to destroy remote memory map");

	/* Inform sender node that DMA operation is done */
	send_end_ack_to_remote();

	/* Clean and destroy all relevant objects */
	cleanup_core_objects(&state);

	destroy_core_objects(&state);

	return res;

}



