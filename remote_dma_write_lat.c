/*
 * Created by Hxk
 * 2022/11/04
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dma_communication.h"
#include "dma_parameters.h"
#include "dma_common.h"

int main(int argc, char *argv[]) {

    struct dma_parameters dma_param;
    dma_param.is_local = 0;
    dma_param.dma_type = WRITE;
    dma_param.test_type = LAT;
    parse_args(&dma_param, argc, argv);
    print_dma_parameters(&dma_param);

    void *src_buf = malloc(dma_param.size);

    doca_error_t res;

    if(dma_param.is_server) {
		res = dma_remote_copy(&dma_param, src_buf, dma_param.size);
    }
    else {
		res = remote_dma_prepare(&dma_param, src_buf, dma_param.size);
    }

    if (res != DOCA_SUCCESS){
    	DOCA_LOG_ERR("Unable to remote dma: %s", doca_get_error_string(res));
    }
    else {
		if(dma_param.is_server) {
			print_report_lat(&dma_param);
		}
    }

    if(dma_param.tposted != NULL) {
      	free(dma_param.tposted);
    }
    free(src_buf);

    return 0;
}
