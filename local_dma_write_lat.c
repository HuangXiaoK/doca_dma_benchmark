/*
 * Created by Hxk
 * 2022/11/04
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "dma_communication.h"
#include "dma_parameters.h"
#include "dma_common.h"

DOCA_LOG_REGISTER(LOCAL_DMA_WRITE_LAT);

int main(int argc, char *argv[]) {

    // Use 2 threads to simulate server and client like remote write
    struct local_write_lat_parameters server_param = {0};
    struct local_write_lat_parameters client_param = {0};

    server_param.dma_params.is_local = 1;
    server_param.dma_params.dma_type = WRITE;
    server_param.dma_params.test_type = LAT;
    server_param.dma_params.is_server = 1;
    parse_args(&server_param.dma_params, argc, argv);

    client_param.dma_params = server_param.dma_params;
    // tposted need to malloc a piece of memory
    ALLOCATE(client_param.dma_params.tposted, cycles_t, client_param.dma_params.iters);

    client_param.dma_params.is_server = 0;

    print_dma_parameters(&server_param.dma_params);
    print_dma_parameters(&client_param.dma_params);

    size_t length = server_param.dma_params.size;
    server_param.send_buf = (char*)malloc(length);
    client_param.send_buf = (char*)malloc(length);
    server_param.dst_buf = client_param.recv_buf = (char*)malloc(length);
    client_param.dst_buf = server_param.recv_buf = (char*)malloc(length);

	pthread_t server_tid;
	pthread_t client_tid;

    if(pthread_create(&server_tid, NULL, run_local_dma_write_lat, &server_param) < 0 || 
       pthread_create(&client_tid, NULL, run_local_dma_write_lat, &client_param) < 0) {
        DOCA_LOG_ERR("Thread create err.");
    }

    pthread_join(server_tid, NULL);
    pthread_join(client_tid, NULL);


    if (server_param.res != DOCA_SUCCESS){
    	DOCA_LOG_ERR("Server unable to dma_local_copy: %s", doca_get_error_string(server_param.res));
    }
    else {
        printf("Server result is : \n");
        print_report_lat(&server_param.dma_params);
    }

    if (client_param.res != DOCA_SUCCESS){
    	DOCA_LOG_ERR("Server unable to dma_local_copy: %s", doca_get_error_string(client_param.res));
    }
    else {
        printf("client result is : \n");
        print_report_lat(&client_param.dma_params);       
    }

    if(server_param.dma_params.tposted != NULL) {
    	free(server_param.dma_params.tposted);
    }
    if(client_param.dma_params.tposted != NULL) {
    	free(client_param.dma_params.tposted);
    }

    free(server_param.send_buf);
    free(server_param.recv_buf);
    free(server_param.dst_buf);
    free(client_param.send_buf);

    return 0;

}
