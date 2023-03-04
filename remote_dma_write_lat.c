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
DOCA_LOG_REGISTER(REMOTE_DMA_WRITE_LAT);

int main(int argc, char *argv[]) {

    struct dma_parameters dma_param = {0};
	doca_error_t res;
    dma_param.is_local = 0;
    dma_param.dma_type = WRITE;
    dma_param.test_type = LAT;
    parse_args(&dma_param, argc, argv);
    print_dma_parameters(&dma_param);

	res = run_remote_dma_write_lat(&dma_param);
    if (res != DOCA_SUCCESS){
    	DOCA_LOG_ERR("Unable to remote dma: %s", doca_get_error_string(res));
    }
	else {
		print_report_lat(&dma_param);
	}

    if(dma_param.tposted != NULL) {
      	free(dma_param.tposted);
    }
	if(dma_param.export_json != NULL) {
		free(dma_param.export_json);
	}
	if(dma_param.export_str != NULL) {
		free(dma_param.export_str);
	}
	
    return 0;
}
