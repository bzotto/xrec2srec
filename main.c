//
//  main.c
//
//  A simple utility for reading "xrec" binary load files (as
//  used by SWTPC tapes and maybe others) and converting to
//  Motorola S-record text format on stdout.
//
// Copyright (c) 2022 Ben Zotto
//

#include <stdio.h>
#include <stdlib.h>
#include "xrec.h"

#define MAX_DATA_BYTES_PER_LINE     16

struct srec_state {
    uint16_t address; // Starting address of this record
    uint8_t data[MAX_DATA_BYTES_PER_LINE]; // This is the largest byte count we'll output.
    int length;       // Valid bytes in the data buffer.
    int last_record_type;
};

void print_usage(const char * program)
{
    printf("usage: %s input_file\n", program);
}

int main(int argc, const char * argv[])
{
    if (argc != 2) {
        print_usage(argv[0]);
        return -1;
    }
    
    FILE * file = fopen(argv[1], "rb");
    if (!file) {
        printf("Unable to open %s\n", argv[1]);
        return -1;
    }
    
    if (fseek(file, 0 , SEEK_END) != 0) {
        printf("Error reading %s\n", argv[1]);
        return -1;
    }

    long file_size = ftell(file);

    unsigned char * data = malloc(file_size);
    if (data == NULL){
        printf("File too large to allocate work buffer");
        fclose(file);
        return -1;
    }
    
    if (fseek(file, 0 , SEEK_SET) != 0) {
        printf("Error reading %s\n", argv[1]);
        free(data);
        fclose(file);
        return -1;
    }
    
    unsigned long bytes_read = fread(data, 1, file_size, file);
    fclose(file);
    if (bytes_read != file_size) {
        printf("Error reading %s\n", argv[1]);
        free(data);
        return -1;
    }
    
    // Set up the output state.
    struct srec_state write_state;
    write_state.length = 0;
    write_state.address = 0;
    write_state.last_record_type = 0;
    
    // Set up input state and read/write
    struct xrec_state read_state;
    xrec_begin_read(&read_state);
    read_state.context = &write_state;
    xrec_read_bytes(&read_state, (const char *)data, (int)bytes_read);
    
    // Upon completion, display the stats and any error that occurred.
    if (read_state.last_strict_error == XREC_ERROR_UNKNOWN_RECORD_TYPE) {
        printf("\nWarning: input contained at least one unknown record type.\n");
    } else if (read_state.last_strict_error == XREC_ERROR_INVALID_CHECKSUM) {
        printf("\nWarning: input contained at least one failed data checksum. Beware corruption!\n");
    }
    if (write_state.last_record_type != XREC_TERMINATION_16BIT) {
        printf("\nWarning: did not encounter (or emit) closing termination record.\n");
    }
}

void flush_output(struct srec_state * srec)
{
    if (srec->length == 0) {
        return;
    }
    uint8_t output_count = 2 + srec->length + 1;
    printf("S1%02X%04X", output_count, srec->address);
    uint8_t sum = output_count + (srec->address >> 8) + (srec->address & 0xFF);
    for (int i = 0; i < srec->length; i++) {
        printf("%02X", srec->data[i]);
        sum += srec->data[i];
    }
    printf("%02X\n", (~sum & 0xFF));
    
    // Update address to point to next implied address and reset length.
    srec->address += srec->length;
    srec->length = 0;
}

// Required callback function for the parser
extern void xrec_data_read(struct xrec_state * xrec,
                           int record_type,
                           uint16_t address,
                           uint8_t * data,
                           int length,
                           int checksum_error)
{
    struct srec_state * srec = xrec->context;

    if (record_type == XREC_DATA_16BIT) {
        if (checksum_error) {
            // Don't print out this error because it will commingle with the
            // actual output. We will flag any strict errors at the end.
        }
        // If the address of the inbound record is not aligned with the presumed
        // next address of the current outbound record, flush it.
        if (address != srec->address + srec->length) {
            flush_output(srec);
            srec->address = address;
        }
        // Pour the inbound data into the outbound vessel.
        for (int i = 0; i < length; i++) {
            srec->data[srec->length++] = data[i];
            // If we hit the end of this output line, flush it
            // but keep on going.
            if (srec->length == MAX_DATA_BYTES_PER_LINE) {
                flush_output(srec);
            }
        }
    } else if (record_type == XREC_TERMINATION_16BIT) {
        printf("S9030000FC\n");
        
    }
    srec->last_record_type = record_type;
}

