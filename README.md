  

# A simple implementation of DOCA DMA benchmark

DOCA version is 1.4

## Compilation

**cmake minimum required VERSION 3.16**

`./build.sh`

The executable files is generated in the ***. /build/bin*** directory

The *-Wdeprecated-declarations warning* during compilation is a problem with the Doca library itself (dog's head for life)

## Running

It is written with reference to *RDMA perftest*, so it generates a lot of executables, instead of specifying local/remote, write/read and lat/bw with parameters.

**The following command is already in the *bin* directory by default.**

**Local DMA**

```
Options:
 -h <help>            Help information
 -s <size>            The size of message (default 1024)
 -i <iterations>      The number of iterations (default 100000)
 -w <warn_up>         The number of preheats (default 10000)
                      DMA seems to have the problem of cold start
 -g <page_size>       Page size alignment of the provided memory range. 
                      Must be >= 4096 and a power of 2. (default 4096)
 -q <wq_depth>        The depth of work queue (default 1)
 -e <element_num>     Initial number of elements in the inventory (default 2)
 -c <max_chunks>      The new value of the property. (default 2)
 -d <pcie_device>     The address of DMA device (default af:00.1)
 -t <threads>         This parameter is only valid for bw test. 
                      For remote, server and clientmust have the same value. 
                      The number of DMA threads (default 1)
 -b <bind_core>       This parameter is only valid for bw test. 
                      Whether the DMA thread of bw is bound to the core (default 1)
                      If equal to 1 bind, otherwise not bind
```

Local dma only start one side, as an example:

`./local_dma_read_lat -s 1024 -i 10000 -d af:00.1`

**Remote DMA**

```
Options:
 -h <help>            Help information
****The default is server, and the client needs to add the "-a" parameter****
 -a <ip>          IP address. Only client need this. (default 10.10.10.211)
 -p <port>        Port. (default 10086)

 -s <size>            The size of message (default 1024)
 -i <iterations>      The number of iterations (default 100000)
 -w <warn_up>         The number of preheats (default 10000)
                      DMA seems to have the problem of cold start
 -g <page_size>       Page size alignment of the provided memory range. 
                      Must be >= 4096 and a power of 2. (default 4096)
 -q <wq_depth>        The depth of work queue (default 1)
 -e <element_num>     Initial number of elements in the inventory (default 2)
 -c <max_chunks>      The new value of the property. (default 2)
 -d <pcie_device>     The address of DMA device (default af:00.1)
 -t <threads>         This parameter is only valid for bw test. 
                      For remote, server and clientmust have the same value. 
                      The number of DMA threads (default 1)
 -b <bind_core>       This parameter is only valid for bw test. 
                      Whether the DMA thread of bw is bound to the core (default 1)
                      If equal to 1 bind, otherwise not bind
```

Remote dma need start the server first, then start the client, as an example:

```
server side :  ./remote_dma_write_lat -p 10000 -s 1024 -i 100000 -d af:00.0

client side :  ./remote_dma_write_lat -a 10.10.10.211 -p 10000 -s 1024 -i 100000 -d af:00.1
```

## **Attention**

1. ***No DOCA DMA non-blocking interface has been found***, so the results of the BW test are poor and similar to the results of the (1 second / time per DMA operation).
2. It was found that the DMA ***had a cold start,*** so the "-w" parameter was set and the real test was run after a certain number of dma operations. You can also set the "-i" parameter to a large value to avoid this problem.
3. The parameters *element_num, max_chunks*, etc. are required to initialize the DMA resources and can be used as default values.
4. According to personal experience, the error *"doca_mmap_create_from_export false: Unknown error"* is caused by 1. the pcie device settings are the same on both sides of the remote dma; 2. the client side may be disconnected.

## **Error**
1. There will be an error in the bw test of multi-threading, which seems to be a problem with DOCA itself.
```
[18:21:33:138828][DOCA][ERR][DOCA_DMA:717]: DMA work queue context unable to create QP. err=2
[18:21:33:138885][DOCA][ERR][DOCA_DMA:717]: DMA work queue context unable to create QP. err=2
[18:21:33:138949][DOCA][ERR][DOCA_DMA:717]: DMA work queue context unable to create QP. err=22
[18:21:33:139168][DOCA][ERR][DOCA_DMA:938]: Unable to create DMA work queue context
[18:21:33:139051][DOCA][ERR][DOCA_DMA:938]: Unable to create DMA work queue context
[18:21:33:139332][DOCA][ERR][DMA_COMMON:127]: Unable to register work queue with context: Memory allocation failure
[18:21:33:139295][DOCA][ERR][DMA_COMMON:127]: Unable to register work queue with context: Memory allocation failure
[18:21:33:139371][DOCA][ERR][DMA_COMMUNICATION:975]: init_core_objects false[18:21:33:139407][DOCA][ERR][DMA_COMMUNICATION:975]: init_core_objects false
```
 The current number of threads cannot be too large.
 In the bw test, each thread creates a wq. 
2. In the remote write lat test, the program may be stuck due to dma write failure.
 The reason for the above phenomenon is that the dma operation is performed at an unknown remote address, but there is no problem in sending and receiving the information of remote dma mmap, which is very strange.
