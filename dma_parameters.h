/*
 * Created by Hxk
 * 2022/10/13
 */

#ifndef DMA_PARAMETERS_H
#define DMA_PARAMETERS_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include<stdlib.h>

#include <doca_dev.h>
#include "get_clock.h"
#include "dma_parameters.h"

/* Macro for allocating. */
#define ALLOCATE(var,type,size)                                     \
{ if((var = (type*)malloc(sizeof(type)*(size))) == NULL)        \
	{ fprintf(stderr," Cannot Allocate\n"); exit(1);}}


// Parameter default value
#define DEF_SIZE            (1024)
#define DEF_ITERS           (100000)
#define DEF_PREHEATS        (10000)
#define DEF_PG_SZ           (4096)
#define DEF_WQ_NUM          (1)
#define DEF_ELEMENTS_NUM    (2)
#define DEF_MAX_CHUNKS      (2)
#define DEF_PCI_BDF         "af:00.1"

#define DEF_IS_SERVER       (1)
#define DEF_REMOTE_IP       "10.10.10.211"
#define DEF_REMOTE_PORT     (10086)

// The type of dma operation
typedef enum { READ , WRITE } DmaType;

// The type of test
typedef enum { LAT , BW } TestType;

// Input parameters at runtime
struct dma_parameters{
    size_t size;
    size_t iters;
    size_t preheats;
    size_t pg_sz;
    uint32_t wq_num;
    size_t num_elements;
    uint32_t max_chunks;
    struct doca_pci_bdf pcie_addr;
    int is_local;
    int is_server;
    char*  ip;
    size_t port;
    // tposted for LAT test
    cycles_t *tposted;
    // start_time and end_time for BW test
    cycles_t start_time;
    cycles_t end_time;
    DmaType	dma_type;
    TestType test_type;        
};

// Convert a pcie address in string form to doca_pci_bdf.
// void *str : the pcie address in string form
// struct doca_pci_bdf *pcie_addr : converted value
// if succeed, return 1; else return 0.
int str_to_doca_pci_bdf(void *str, struct doca_pci_bdf *pcie_addr );

static void usage(int local);

void parse_args(struct dma_parameters *user_param, int argc, char *argv[]);
 
// Print lat results
void print_report_lat(struct dma_parameters *user_param);

// Print bw results
void print_report_bw(struct dma_parameters *user_param);

// Just for test
void print_dma_parameters(struct dma_parameters *user_param);

#endif // DMA_PARAMETERS_H