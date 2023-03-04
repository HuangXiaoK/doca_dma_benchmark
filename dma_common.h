/*
 * Created by Hxk
 * 2022/09/27
 */

#ifndef DMA_COMMON_H_
#define DMA_COMMON_H_

#include <doca_error.h>
#include <doca_buf.h>
#include <doca_buf_inventory.h>
#include <doca_ctx.h>
#include <doca_dev.h>
#include <doca_dma.h>

#include <doca_log.h>

#include <doca_mmap.h>

#include <stdint.h>
#include <string.h>

// The set of parameters to be used during DMA
struct dma_state{
    struct doca_dev *dev;
    struct doca_mmap *local_mmap; 
    struct doca_mmap *remote_mmap;
    struct doca_mmap *provide_for_remote_mmap;
    struct doca_buf_inventory *buf_inv;
    struct doca_ctx *ctx;
    struct doca_dma *dma_ctx;
    struct doca_workq *wq;
};

// Open the device list and select the DMA device according to pcie_addr
doca_error_t open_local_device(struct doca_pci_bdf *pcie_addr, struct dma_state *state);

// Create core objects needed for DMA operations
doca_error_t create_core_objects(struct dma_state *state, size_t num_elements, uint32_t wq_depth);

// Init core objects
// Called by the initiator of the DMA
doca_error_t init_core_objects(struct dma_state *state, uint32_t max_chunks);

// Init remote core objects
// Called by the remote DMA recipient
doca_error_t init_remote_core_objects(struct dma_state *state);

// Bind buf to mmap
doca_error_t populate_mmap(struct doca_mmap *mmap, char *buf, size_t length, size_t pg_sz);

// The following are the functions related to resource release and cleanup
void cleanup_objects(struct dma_state *state);

void destroy_objects(struct dma_state *state);

#endif // DMA_COMMON_H_