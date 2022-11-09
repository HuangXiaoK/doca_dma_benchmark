/*
 * Created by Hxk
 * 2022/10/18
 */

#ifndef DMA_COMMUNICATION_H
#define DMA_COMMUNICATION_H

#include "get_clock.h"
#include "dma_common.h"
#include "dma_parameters.h"

#include <json-c/json.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <doca_error.h>
// #include <doca_log.h>
#include <doca_mmap.h>

// When use remote dma, we need use tcp to transfer information.
int tcp_fd;

// Use tpc to send dma information to remote side.
// Remote side needs this information to do dma operations.
// char *ip : remote ip
// uint16_t port : remote port
// char *export_str : DMA information in json format
// size_t export_str_len : size of export_str
// if succeed, return true; else return false.
static bool send_dma_info_to_remote(char *ip, 
                                    uint16_t port, 
                                    char *export_str, 
                                    size_t export_str_len);

// Use tcp to receive dma information.
// size_t port : tcp port
// char **remote_addr : remote dma address
// size_t *remote_addr_len : size of remote_addr
// if succeed, return true; else return false.
static bool receive_dma_info_from_remote(const size_t port , 
                                         char *export_buffer, 
                                         size_t export_buffer_len,
                                         char **remote_addr,
			                             size_t *remote_addr_len);

// Tell remote side that dma operations is over.
static void send_end_ack_to_remote();

// Wait remote end info.
static void wait_remote_end_ack();

// Run local dma.
// struct dma_parameters *dma_params : dma parameters
// char *dst_buffer : destination buffer
// char *src_buffer : initiator buffer
// size_t length : buffer size
doca_error_t dma_local_copy(struct dma_parameters *dma_params, 
                            char *dst_buffer, 
                            char *src_buffer, 
                            size_t length);

// The remote dma side prepares dma related resources
// struct dma_parameters *dma_params : dma parameters
// char *src_buffer : buffer
// size_t length : buffer size
doca_error_t remote_dma_prepare(struct dma_parameters *dma_params, 
                                char *src_buffer, 
                                size_t length);

// Run remote dma
// struct dma_parameters *dma_params : dma parameters
// char *src_buffer : initiator buffer
// size_t length : buffer size
doca_error_t dma_remote_copy(struct dma_parameters *dma_params, 
                            char *src_buffer, 
                            size_t length);

#endif // DMA_COMMUNICATION_H
