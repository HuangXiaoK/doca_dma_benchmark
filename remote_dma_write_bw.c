/*
 * Created by Hxk
 * 2022/11/07
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "dma_communication.h"
#include "dma_parameters.h"
#include "dma_common.h"

DOCA_LOG_REGISTER(REMOTE_DMA_WRITE_BW);

int main(int argc, char *argv[]) {

    struct bw_parameters *bw_param = NULL;
    pthread_t *tid = NULL;
    struct dma_parameters dma_param = {0};
    dma_param.is_local = 0;
    dma_param.dma_type = WRITE;
    dma_param.test_type = BW;
    parse_args(&dma_param, argc, argv);
    print_dma_parameters(&dma_param);

    size_t thread_num = dma_param.thread_num;
    ALLOCATE(bw_param ,struct bw_parameters ,thread_num);
    ALLOCATE(tid ,pthread_t ,thread_num);

    for (size_t i = 0; i < thread_num; i++) {
        bw_param[i].id = i;
        bw_param[i].dma_params = dma_param;
		bw_param[i].dma_params.port += i;
        if(pthread_create(&tid[i], NULL, run_remote_dma_bw, &bw_param[i]) < 0) {
            DOCA_LOG_ERR("Thread create err.");
        }
    }

    for (size_t i = 0; i < thread_num; i++) {
        pthread_join(tid[i], NULL);
    }

    doca_error_t res = DOCA_SUCCESS;
    for (size_t i = 0; i < thread_num; i++) {
        if (bw_param[i].res != DOCA_SUCCESS) {
            res = bw_param[i].res;
            printf("BW error\n");
            break;
        }
    }

    if(res == DOCA_SUCCESS) {
		if(dma_param.is_server) {
			print_report_bw(bw_param);
		}
    }

    free(bw_param);
    free(tid);

    return 0;
}
