/*
 * Created by Hxk
 * 2022/10/18
 */

#ifndef DMA_COMMUNICATION_H
#define DMA_COMMUNICATION_H
#define _GNU_SOURCE

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
#include <pthread.h>
#include <sched.h>

#include <doca_error.h>
// #include <doca_log.h>
#include <doca_mmap.h>

// Bind the thread to the specified core
// int core_id : The id of the core to bind
// if succeed, return true; else return false.
static bool bind_thread_to_core(int core_id);

// Client connects to server.
// int *fd : tcp fd
// char *ip : remote ip
// uint16_t port : remote port
// char *export_str : DMA information in json format
// size_t export_str_len : size of export_str
// if succeed, return true; else return false.
static bool client_connect(int *fd, char *ip, uint16_t port);

// Server accepts client's connection.
// int *fd : tcp fd
// size_t port :  port
// if succeed, return true; else return false.
static bool server_accept(int *fd, const size_t port);

// Use tpc to send dma information to remote side.
// int fd : tcp fd
// char *export_str : DMA information in json format
// size_t export_str_len : size of export_str
// if succeed, return true; else return false.
static bool send_dma_info_to_remote(int fd, 
                                    char *export_str, 
                                    size_t export_str_len); 

// Use tcp to receive dma information.
// int fd : tcp fd
// char *export_buffer : remote DMA information in json format
// size_t export_buffer_len : export buffer len
// char **remote_addr : remote dma address
// size_t *remote_addr_len : size of remote_addr
// if succeed, return true; else return false.
static bool receive_dma_info_from_remote(int fd, 
                                         char *export_buffer, 
                                         size_t export_buffer_len,
                                         char **remote_addr,
			                             size_t *remote_addr_len);

// Tell remote side that dma operations is over.
static void send_end_ack_to_remote(struct dma_parameters *dma_params);

// Wait remote end info.
static void wait_remote_end_ack(struct dma_parameters *dma_params);

// Poll wq for completion events
// if succeed, return DOCA_SUCCESS.
doca_error_t poll_wq(struct doca_workq *workq, struct doca_event *event);

// Run remote dma read lat test.
// struct dma_parameters *dma_params : dma parameters
doca_error_t run_remote_dma_read_lat(struct dma_parameters *dma_params);

// Run remote dma write lat test.
// struct dma_parameters *dma_params : dma parameters
doca_error_t run_remote_dma_write_lat(struct dma_parameters *dma_params);

// Run local dma read lat test.
// struct dma_parameters *dma_params : dma parameters
doca_error_t run_local_dma_read_lat(struct dma_parameters *dma_params);

// Run local dma write lat test.
// struct local_write_lat_parameters *local_write_lat_params : local write lat parameters, 
//                                                   including dma_parameters and buffers 
void *run_local_dma_write_lat(struct local_write_lat_parameters *local_write_lat_params);

// Run local dma read lat test.
// struct dma_parameters *dma_params : dma parameters
doca_error_t run_local_dma_read_lat(struct dma_parameters *dma_params);

// // Monitor bw completion or not.
// // struct dma_parameters *dma_params : dma parameters
// void *bw_monitor(struct bw_parameters *bw_params);

// Run local dma bw test.
// struct dma_parameters *dma_params : bw parameters
void *run_local_dma_bw(struct bw_parameters *bw_params);

// Run remote dma bw test.
// struct dma_parameters *dma_params : bw parameters
void *run_remote_dma_bw(struct bw_parameters *bw_params);
 
#endif // DMA_COMMUNICATION_H
