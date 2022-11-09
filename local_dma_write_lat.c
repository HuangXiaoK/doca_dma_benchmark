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
    dma_param.is_local = 1;
    dma_param.dma_type = WRITE;
    dma_param.test_type = LAT;
    parse_args(&dma_param, argc, argv);
    print_dma_parameters(&dma_param);

    void *src_buf = malloc(dma_param.size);
    void *dst_buf = malloc(dma_param.size);

    doca_error_t res;
    res = dma_local_copy(&dma_param, dst_buf, src_buf, dma_param.size);

    if (res != DOCA_SUCCESS){
    	DOCA_LOG_ERR("Unable to dma_local_copy: %s", doca_get_error_string(res));
    }
    else {
    	print_report_lat(&dma_param);
    }

    if(dma_param.tposted != NULL) {
    	free(dma_param.tposted);
    }
    free(src_buf);
    free(dst_buf);

    return 0;

}
