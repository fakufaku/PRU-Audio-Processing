/**
 * @brief Loads the PRU files, executes them, and waits for completion.
 *        Usage:
 *        $ ./loader pru1.bin
 *        Compile with:
 *        gcc -o loader loader.c -lprussdrv
 *        Based on https://credentiality2.blogspot.com/2015/09/beaglebone-pru-gpio-example.html
 * 
 * @author Loïc Droz <lk.droz@gmail.com>
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <inttypes.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <prussdrv.h>
#include <pruss_intc_mapping.h>

#define PRU_NUM0 0
#define PRU_NUM1 1


int setup_mmaps(volatile uint32_t ** pru_mem, volatile void ** host_mem, unsigned int * host_mem_len, unsigned int * host_mem_phys_addr) {
    // Pointer into the PRU1 local data RAM, we use it to send to the PRU the host's memory physical address and length
    volatile void * PRU_mem_void = NULL;
    // For now, store data in 32 bits chunks
    volatile uint32_t * PRU_mem = NULL;
    int ret = prussdrv_map_prumem(PRUSS0_PRU1_DATARAM, (void **) &PRU_mem_void);
    if (ret != 0) {
        return ret;
    }
    PRU_mem = (uint32_t *) PRU_mem_void;

    // Pointer into the DDR RAM mapped by the uio_pruss kernel module, it is possible to change the amount of memory mapped by uio_pruss by reloading it with an argument (still have to find out how)
    volatile void * HOST_mem = NULL;
    ret = prussdrv_map_extmem((void **) &HOST_mem);
    if (ret != 0) {
        return ret;
    }
    unsigned int HOST_mem_len = prussdrv_extmem_size();
    // The PRU needs the physical address of the memory it will write to
    unsigned int HOST_mem_phys_addr = prussdrv_get_phys_addr((void *) HOST_mem);

    // Trim HOST_mem_len down so that it is a multiple of 8. This will ensure that HOST_mem_len / 2 is a multiple of 4, which allows the PRU to check if it has reached half if the buffer by doing a simple equality check (4 because it is writing 4 B at a time)
    HOST_mem_len = (HOST_mem_len >> 3) << 3;

    printf("%u bytes of Host memory available.\n", HOST_mem_len);
    printf("Physical (PRU-side) address: %x\n", HOST_mem_phys_addr);
    printf("Virtual (Host-side) address: %p\n\n", HOST_mem);

    // Use the first 8 bytes of PRU memory to tell it where the shared segment of Host memory is
    PRU_mem[0] = HOST_mem_phys_addr;
    PRU_mem[1] = HOST_mem_len;

    *pru_mem = PRU_mem;
    *host_mem = HOST_mem;
    *host_mem_len = HOST_mem_len;
    *host_mem_phys_addr = HOST_mem_phys_addr;

    return 0;
}


void processing(FILE * output, volatile void * host_mem, unsigned int host_mem_len) {
    // Make sure the size is indeed a multiple of 8
    assert(host_mem_len % 8 == 0);

    // Use this to limit the number of passes we do for now
    const int buffer_passes = 10;
    const size_t nmemb = (size_t) (host_mem_len / 2);
    int buffer_side = 0;
    for (int i = 0; i < 2 * buffer_passes; ++i) {
        // Wait for interrupt from PRU1
        printf("Waiting for PRU interrupt...\n");
        prussdrv_pru_wait_event(PRU_EVTOUT_1);
        prussdrv_pru_clear_event(PRU_EVTOUT_1, PRU1_ARM_INTERRUPT);
        // Initialize arguments for fwrite accordingly to the buffer we're writing to
        volatile void * ptr;
        if (buffer_side == 0) {
            printf("Received interrupt for buffer side 0\n");
            ptr = host_mem;
            buffer_side = 1;
        }
        else {
            printf("Received interrupt for buffer side 1\n");
            ptr = host_mem + host_mem_len / 2;
            buffer_side = 0;
        }

        // Write the data to the file from the buffer
        const size_t written = fwrite((const void *) ptr, 1, nmemb, output);
        if (written == nmemb) {
            printf("%zu bytes written to file.\n", written);
        }
        else {
            fprintf(stderr, "Error! Could not write to file (%d: %s)\n", errno, strerror(errno));
        }
    }
}


void stop(FILE * file) {
    prussdrv_pru_disable(PRU_NUM1);
    prussdrv_exit();

    if (file != NULL) {
        fclose(file);
    }
}


int main(int argc, char ** argv) {
    if (argc != 2) {
        printf("Usage: %s pru1.bin\n", argv[0]);
        return 1;
    }

    // ##### Prussdrv setup #####
    prussdrv_init();
    unsigned int ret = prussdrv_open(PRU_EVTOUT_1);
	if (ret) {
        fprintf(stderr, "PRU1 : prussdrv_open failed\n");
        return ret;
    }


    // Initialize interrupts or smth like that
    tpruss_intc_initdata pruss_intc_initdata = PRUSS_INTC_INITDATA;
    prussdrv_pruintc_init(&pruss_intc_initdata);

    // ##### Setup memory mappings #####
    volatile uint32_t * PRU_mem = NULL;
    volatile void * HOST_mem = NULL;
    unsigned int HOST_mem_len = 0;
    unsigned int HOST_mem_phys_addr = 0;
    // Setup memory maps and pass the physical address and length of the host's memory to the PRU
    int ret_setup = setup_mmaps(&PRU_mem, &HOST_mem, &HOST_mem_len, &HOST_mem_phys_addr);
    if (ret_setup != 0) {
        stop(NULL);
        return -1;
    } else if (PRU_mem == NULL || HOST_mem == NULL) {
        stop(NULL);
        return -1;
    }

    // Setup output files and any stuff required for properly reading the data output from the PRU
    FILE * output = fopen("../output/out.pcm", "w");
    if (output == NULL) {
        fprintf(stderr, "Error! Could not open file (%d)\n", errno);
        stop(NULL);
        return -1;
    }

    // Load the PRU program(s)
    printf("Loading \"%s\" program on PRU1\n", argv[1]);
    ret = prussdrv_exec_program(PRU_NUM1, argv[1]);
    if (ret) {
    	fprintf(stderr, "ERROR: could not open %s\n", argv[1]);
        stop(output);
    	return ret;
    }

    // Start processing on the received data
    processing(output, HOST_mem, HOST_mem_len);
    
    // Disable PRUs and the pruss driver. Also close the opened file.
    stop(output);

    return 0;
}
