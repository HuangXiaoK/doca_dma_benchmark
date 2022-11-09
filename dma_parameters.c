/*
 * Created by Hxk
 * 2022/10/13
 */

#include"dma_parameters.h"
 
int str_to_doca_pci_bdf(void *str, struct doca_pci_bdf *pcie_addr ){

	unsigned int bus_bitmask = 0xFFFFFF00;
	unsigned int dev_bitmask = 0xFFFFFFE0;
	unsigned int func_bitmask = 0xFFFFFFF8;

    char tmps[4];
    uint32_t tmpu;
    char *bdf = (char*) str;

    // Verify that the DMA device address is correct
    if (bdf == NULL || strlen(bdf) != 7 || bdf[2] != ':' || bdf[5] != '.'){
        printf("DMA device error\n");
        return 0;
    }

    tmps[0] = bdf[0];
	tmps[1] = bdf[1];
	tmps[2] = '\0';

	tmpu = strtoul(tmps, NULL, 16);
	if ((tmpu & bus_bitmask) != 0){
        printf("DMA device BUS error\n");
        return 0;
    }
    pcie_addr->bus = tmpu;

    tmps[0] = bdf[3];
	tmps[1] = bdf[4];
	tmps[2] = '\0';
	tmpu = strtoul(tmps, NULL, 16);
	if ((tmpu & dev_bitmask) != 0){
        printf("DMA device DEVICE error\n");
        return 0;
    }
	pcie_addr->device = tmpu;

    tmps[0] = bdf[6];
	tmps[1] = '\0';
	tmpu = strtoul(tmps, NULL, 16);
	if ((tmpu & func_bitmask) != 0){
        printf("DMA device FUNCTION error\n");
        return 0;
    }
    pcie_addr->function = tmpu;

    return 1;
}

void usage(int local){
    printf("Options:\n");
    printf(" -h <help>            Help information\n");

    if(local < 1){
        printf("****The default is server, and the client needs to add the \"-a\" parameter****\n");

        printf(" -a <ip>          IP address. Only client need this. (default %s)\n", DEF_REMOTE_IP);
        printf(" -p <port>        Port. (default %d)\n\n", DEF_REMOTE_PORT);
    }

    printf(" -s <size>            The size of message (default %d)\n", DEF_SIZE);
    printf(" -t <iterations>      The number of iterations (default %d)\n", DEF_ITERS);
    printf(" -w <warn_up>         The number of preheats (default %d)\n \
                     DMA seems to have the problem of cold start\n", DEF_PREHEATS);
    printf(" -g <page_size>       Page size alignment of the provided memory range. \n \
                     Must be >= 4096 and a power of 2. (default %d)\n", DEF_PG_SZ);
    printf(" -q <wq_num>          The num of work queue (default %d)\n", DEF_WQ_NUM);
    printf(" -e <element_num>     Initial number of elements in the inventory (default %d)\n", DEF_ELEMENTS_NUM);
    printf(" -c <max_chunks>      The new value of the property. (default %d)\n", DEF_MAX_CHUNKS);
    printf(" -d <pcie_device>     The address of DMA device (default %s)\n", DEF_PCI_BDF);
}

void parse_args(struct dma_parameters *user_param, int argc, char *argv[]){
    user_param->size = DEF_SIZE;
    user_param->iters = DEF_ITERS;
    user_param->preheats = DEF_PREHEATS;
    user_param->pg_sz = DEF_PG_SZ;
    user_param->wq_num = DEF_WQ_NUM;
    user_param->num_elements = DEF_ELEMENTS_NUM;
    user_param->max_chunks = DEF_MAX_CHUNKS;
    str_to_doca_pci_bdf(DEF_PCI_BDF, &(user_param->pcie_addr));
    user_param->tposted = NULL;
    user_param->ip = DEF_REMOTE_IP;
    user_param->port = DEF_REMOTE_PORT;
    user_param->is_server = DEF_IS_SERVER;
    user_param->start_time = 0;
    user_param->end_time = 0;

    for (int i = 1; i < argc; ++i){

        if (strlen(argv[i]) == 2 && strcmp(argv[i], "-s") == 0){
            if (i+1 < argc){
                user_param->size = strtoull(argv[i+1], NULL, 10);
                i++;
            }
            else{
                usage(user_param->is_local);
                exit(EXIT_FAILURE);
            }
        }
        else if (strlen(argv[i]) == 2 && strcmp(argv[i], "-t") == 0){
            if (i+1 < argc){
                user_param->iters = strtoull(argv[i+1], NULL, 10);
                i++;
            }
            else{
                usage(user_param->is_local);
                exit(EXIT_FAILURE);
            }
        }
        else if (strlen(argv[i]) == 2 && strcmp(argv[i], "-w") == 0){
            if (i+1 < argc){
                user_param->preheats = strtoull(argv[i+1], NULL, 10);
                i++;
            }
            else{
                usage(user_param->is_local);
                exit(EXIT_FAILURE);
            }
        }
        else if (strlen(argv[i]) == 2 && strcmp(argv[i], "-g") == 0){
            if (i+1 < argc){
                size_t n = strtoull(argv[i+1], NULL, 10);
                if(n < 4096 || (n & n - 1) != 0){
                    printf("Page size is %ld, do not match the  rules!!!\n", n);
                    usage(user_param->is_local);
                    exit(EXIT_FAILURE);
                }
                user_param->pg_sz = n;
                i++;
            }
            else{
                usage(user_param->is_local);
                exit(EXIT_FAILURE);
            }
        }
        else if (strlen(argv[i]) == 2 && strcmp(argv[i], "-q") == 0){
            if (i+1 < argc){
                user_param->wq_num = strtoull(argv[i+1], NULL, 10);
                i++;
            }
            else{
                usage(user_param->is_local);
                exit(EXIT_FAILURE);
            }
        }
        else if (strlen(argv[i]) == 2 && strcmp(argv[i], "-e") == 0){
            if (i+1 < argc){
                user_param->num_elements = strtoull(argv[i+1], NULL, 10);
                i++;
            }
            else{
                usage(user_param->is_local);
                exit(EXIT_FAILURE);
            }
        }
        else if (strlen(argv[i]) == 2 && strcmp(argv[i], "-c") == 0){
            if (i+1 < argc){
                user_param->max_chunks = strtoull(argv[i+1], NULL, 10);
                i++;
            }
            else{
                usage(user_param->is_local);
                exit(EXIT_FAILURE);
            }
        }
        else if (strlen(argv[i]) == 2 && strcmp(argv[i], "-d") == 0){
            if (i+1 < argc){
                if(str_to_doca_pci_bdf(argv[i+1], &(user_param->pcie_addr) )){
                    printf("pcie_addr is %s\n", argv[i+1]);
                    i++;
                }
                else{
                    printf("pcie_addr error");
                    usage(user_param->is_local);
                    exit(EXIT_FAILURE);
                }
            }
            else{
                usage(user_param->is_local);
                exit(EXIT_FAILURE);
            }
        }
        else if (strlen(argv[i]) == 2 && strcmp(argv[i], "-p") == 0){
            if(user_param->is_local){
                printf("Local DMA, do not need port\n");
                usage(user_param->is_local);
                exit(EXIT_FAILURE);
            }
            else{
                if (i+1 < argc){
                    user_param->port = strtol(argv[i+1], NULL, 10);
                    if (user_param->port < 0 || user_param->port > 65535){
                        printf("invalid port number\n");
                        exit(EXIT_FAILURE);
                    }
                    i++;
                }
                else{
                    usage(user_param->is_local);
                    exit(EXIT_FAILURE);
                }
            }
        }
        else if (strlen(argv[i]) == 2 && strcmp(argv[i], "-a") == 0){
            if(user_param->is_local){
                printf("Local DMA, do not need ip\n");
                usage(user_param->is_local);
                exit(EXIT_FAILURE);
            }
            else{
                if(i+1 < argc){
                    user_param->ip = argv[i+1];
                    user_param->is_server = 0;
                    i++;
                }                
                else{
                    usage(user_param->is_local);
                    exit(EXIT_FAILURE);
                }             
            }
        }
        else if (strlen(argv[i]) == 2 && strcmp(argv[i], "-h") == 0){
            usage(user_param->is_local);
            exit(EXIT_SUCCESS);
        }else {
            printf("invalid option: %s\n", argv[i]);
            usage(user_param->is_local);
            exit(EXIT_FAILURE);
        }     
    }
    if(user_param->test_type == LAT) {
        user_param->iters++;
        ALLOCATE(user_param->tposted, cycles_t, user_param->iters);
    }
}

static int cycles_compare(const void *aptr, const void *bptr)
{
	const cycles_t *a = aptr;
	const cycles_t *b = bptr;
	if (*a < *b) return -1;
	if (*a > *b) return 1;

	return 0;
}

static inline cycles_t get_median(int n, cycles_t delta[])
{
	if ((n - 1) % 2)
		return(delta[n / 2] + delta[n / 2 - 1]) / 2;
	else
		return delta[n / 2];
}

void print_report_lat(struct dma_parameters *user_param) {
    cycles_t *delta = NULL;
    cycles_t median;
    size_t measure_cnt = user_param->iters - 1;
    double cycles_to_units;
    double average_sum = 0;
    int iters_99, iters_99_9;

    ALLOCATE(delta, cycles_t, measure_cnt);
    cycles_to_units = get_cpu_mhz(0);

    for (int i = 0; i < measure_cnt; ++i) {
        delta[i] = user_param->tposted[i + 1] - user_param->tposted[i];
    }

    // For test
    // for (int i = 0; i < measure_cnt; ++i) {
    //     printf("%lf\n", 1.0 * delta[i] / cycles_to_units);
    // }

    qsort(delta, measure_cnt, sizeof *delta, cycles_compare);
    median = get_median(measure_cnt, delta);

    for (int i = 0; i < measure_cnt; ++i)
		average_sum += 1.0 * (delta[i] / cycles_to_units);

    iters_99 = (int)(measure_cnt * 0.99);
	iters_99_9 = (int)(measure_cnt * 0.999);
    
    printf("Min  latency is : %lf us\n", 1.0 * delta[0] / cycles_to_units);
    printf("P50  latency is : %lf us\n", 1.0 * median / cycles_to_units);
    printf("Avg  latency is : %lf us\n", 1.0 * average_sum / measure_cnt);
    printf("P99  latency is : %lf us\n", 1.0 * delta[iters_99] / cycles_to_units);
    printf("P999 latency is : %lf us\n", 1.0 * delta[iters_99_9] / cycles_to_units);
    free(delta);
}

void print_report_bw(struct dma_parameters *user_param) {
    double cycles_to_units;
    cycles_t delta;
    long format_factor;

    cycles_to_units = get_cpu_mhz(0) * 1000000;
    delta = user_param->end_time - user_param->start_time;

    format_factor = 0x100000;

    // printf("Runtime  is : %lf sec\n", 1.0 * delta / cycles_to_units);

    double bw_avg = ((double)user_param->size * user_param->iters * cycles_to_units) / (delta * format_factor);
    double msgRate_avg = ((double)user_param->iters * cycles_to_units) / (delta * 1000000);

    printf("Avg BW  is : %lf MB/sec\n", bw_avg);
    printf("MsgateR is : %lf Mpps\n", msgRate_avg);
}

// For test
void print_dma_parameters(struct dma_parameters *user_param) {
    printf("Size is : %ld \n", user_param->size);
    if(user_param->is_server == 1) {
        if(user_param->test_type == LAT) {
            printf("Iterations is :         %ld \n", user_param->iters - 1);
        }
        else {
            printf("Iterations is :         %ld \n", user_param->iters);
        }
        printf("Preheats is :           %ld \n", user_param->preheats);
    }

    printf("Page size is :          %ld \n", user_param->pg_sz);
    printf("Work number is :        %d \n", user_param->wq_num);
    printf("Num elements is :       %ld \n", user_param->num_elements);
    printf("Max chunks is :         %d \n", user_param->max_chunks);

    printf("Pcie addr bus is :      %x \n", user_param->pcie_addr.bus);
    printf("Pcie addr device is :   %x \n", user_param->pcie_addr.device);
    printf("Pcie addr function is : %x \n", user_param->pcie_addr.function);

    if(user_param->is_local) {
        printf("Local DMA\n");
    }
    else {
        printf("Remote DMA\n");
        if(user_param->is_server) {
            printf("This is DMA Server\n");
        }
        else {
            printf("This is DMA Client\n");
        }
        printf("IP is : %s \n", user_param->ip);
        printf("Port is : %ld \n", user_param->port);
    }

    if(user_param->dma_type == WRITE) {
        printf("Do DMA write\n");
    }
    else {
        printf("Do DMA read\n");
    }
}
