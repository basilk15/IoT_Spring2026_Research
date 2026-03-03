#include <stdio.h>
#include <stdlib.h>
#include <libserialport.h>
#include "edge-impulse-sdk/classifier/ei_run_classifier.h"

static float input_buf[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];

static int get_signal_data(size_t offset, size_t length, float *out_ptr) {
    memcpy(out_ptr, input_buf + offset, length * sizeof(float));
    return EIDSP_OK;
}

int main() {
    struct sp_port *port;
    if (sp_get_port_by_name("/dev/ttyUSB0", &port) != SP_OK) {
        printf("Port not found!\n");
        return 1;
    }

    sp_open(port, SP_MODE_READ);
    sp_set_baudrate(port, 115200);
    // Crucial: Set a very short timeout for real-time responsiveness
    sp_set_bits(port, 8);
    sp_set_parity(port, SP_PARITY_NONE);
    sp_set_stopbits(port, 1);
    sp_set_flowcontrol(port, SP_FLOWCONTROL_NONE);

    // Flush any data that was sitting in the buffer before we started
    sp_flush(port, SP_BUF_INPUT);

    printf("Sampling data (%d values)...\n", EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);

    int current_index = 0;
    char read_buf[1024]; // Large enough to grab chunks

    run_classifier_init();

    while (true) {
        // Use a non-blocking or short-timeout read
        int bytes = sp_blocking_read(port, read_buf, sizeof(read_buf) - 1, 10);
        
        if (bytes > 0) {
            read_buf[bytes] = '\0';

            char *ptr = strtok(read_buf, ", \n\r");
            while (ptr != NULL) {
                input_buf[current_index++] = strtof(ptr, NULL);

                if (current_index >= EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
                    // --- INFERENCE START ---
                    signal_t signal;
                    signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
                    signal.get_data = &get_signal_data;

                    ei_impulse_result_t result;
                    EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);

                    if (res == 0) {
                        ei_print_results(&ei_default_impulse, &result);
                    }
                    // --- INFERENCE END ---

                    // 1. Reset index
                    current_index = 0;

                    // 2. THE FIX: Clear the OS serial buffer. 
                    // While run_classifier was running, the ESP32 kept sending data.
                    // If we don't flush, the next 'read' will get data from 100ms ago.
                    sp_flush(port, SP_BUF_INPUT); 
                    
                    printf("\nBuffer flushed. Sampling fresh window...\n");
                }
                ptr = strtok(NULL, ", \n\r");
            }
        }
    }

    sp_close(port);
    return 0;
}