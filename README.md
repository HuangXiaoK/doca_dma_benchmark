  

# A simple implementation of DOCA DMA benchmark

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
 -t <iterations>      The number of iterations (default 100000)
 -w <warn_up>         The number of preheats (default 10000)
                      DMA seems to have the problem of cold start
 -g <page_size>       Page size alignment of the provided memory range. 
                      Must be >= 4096 and a power of 2. (default 4096)
 -q <wq_num>          The num of work queue (default 1)
 -e <element_num>     Initial number of elements in the inventory (default 2)
 -c <max_chunks>      The new value of the property. (default 2)
 -d <pcie_device>     The address of DMA device (default af:00.1) 
```

Local dma only start one side, as an example:

`./local_dma_read_lat -s 1024 -t 10000 -d af:00.1`

The results are as follows: 

```
pcie_addr is af:00.1
Size is : 1024 
Iterations is :         10000 
Preheats is :           10000 
Page size is :          4096 
Work number is :        1 
Num elements is :       2 
Max chunks is :         2 
Pcie addr bus is :      af 
Pcie addr device is :   0 
Pcie addr function is : 1 
Local DMA
Do DMA read
Conflicting CPU frequency values detected: 1000.038000 != 3172.045000. CPU Frequency is not max.
Min  latency is : 2.300234 us
P50  latency is : 2.389809 us
Avg  latency is : 2.415862 us
P99  latency is : 2.507212 us
P999 latency is : 11.784680 us
```

**Remote DMA**

```
Options:
 -h <help>            Help information
****The default is server, and the client needs to add the "-a" parameter****
 -a <ip>          IP address. Only client need this. (default 10.10.10.211)
 -p <port>        Port. (default 10086)

 -s <size>            The size of message (default 1024)
 -t <iterations>      The number of iterations (default 100000)
 -w <warn_up>         The number of preheats (default 10000)
                      DMA seems to have the problem of cold start
 -g <page_size>       Page size alignment of the provided memory range. 
                      Must be >= 4096 and a power of 2. (default 4096)
 -q <wq_num>          The num of work queue (default 1)
 -e <element_num>     Initial number of elements in the inventory (default 2)
 -c <max_chunks>      The new value of the property. (default 2)
 -d <pcie_device>     The address of DMA device (default af:00.1)
```

Remote dma need start the server first, then start the client, as an example:

```
server side :  ./remote_dma_write_lat -p 10000 -s 1024 -t 10000 -d af:00.0

client side :  ./remote_dma_write_lat -a 10.10.10.211 -p 10000 -s 1024 -t 10000 -d af:00.1
```

The result is output on the server side, as follows:

```
pcie_addr is af:00.0
Size is : 1024 
Iterations is :         10000 
Preheats is :           10000 
Page size is :          4096 
Work number is :        1 
Num elements is :       2 
Max chunks is :         2 
Pcie addr bus is :      af 
Pcie addr device is :   0 
Pcie addr function is : 0 
Remote DMA
This is DMA Server
IP is : 10.10.10.211 
Port is : 10000 
Do DMA write
[18:15:16:017807][DOCA][INF][DMA_COMMON:115]: Waiting for remote node to send exported data
[18:15:17:738577][DOCA][INF][DMA_COMMON:139]: Exported data was received
Conflicting CPU frequency values detected: 1000.059000 != 3196.232000. CPU Frequency is not max.
Min  latency is : 2.337615 us
P50  latency is : 2.452408 us
Avg  latency is : 2.506266 us
P99  latency is : 2.842881 us
P999 latency is : 11.086277 us
```

## **Attention**

1. For now, benchmark only supports single-threaded.
2. ***No DOCA DMA non-blocking interface has been found***, so the results of the BW test are poor and similar to the results of the (1 second / time per DMA operation).
3. It was found that the DMA ***had a cold start,*** so the "-w" parameter was set and the real test was run after a certain number of dma operations. You can also set the "-t" parameter to a large value to avoid this problem.
4. The parameters *element_num, max_chunks*, etc. are required to initialize the DMA resources and can be used as default values.
5. According to personal experience, the error *"doca_mmap_create_from_export false: Unknown error"* is caused by 1. the pcie device settings are the same on both sides of the remote dma; 2. the client side may be disconnected.

