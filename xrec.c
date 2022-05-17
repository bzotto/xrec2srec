/*
 * xrec.c
 *
 * A simple library for reading "xrec" binary load files, as
 * used by SWTPC tapes and maybe others.
 *
 * Copyright (c) 2022 Ben Zotto
 * Provided with absolutely no warranty, use at your own risk only.
 * Use and distribute freely, mark modified copies as such.
 */

#include "xrec.h"

#define XREC_START 'X'

enum xrec_read_state {
    READ_WAIT_FOR_START = 0,
    READ_RECORD_TYPE,
    READ_COUNT,
    READ_ADDRESS_HIGH,
    READ_ADDRESS_LOW,
    READ_DATA,
    READ_CHECKSUM,
    READ_COMPLETE,
    READ_ERROR
};

void
xrec_begin_read (struct xrec_state *xrec) {
    xrec->read_state = READ_WAIT_FOR_START;
    xrec->type = 0;
    xrec->byte_count = 0;
    xrec->length = 0;
    xrec->last_strict_error = XREC_ERROR_NONE;
}

void
xrec_read_byte (struct xrec_state *xrec, char byte) {
    uint8_t b = (uint8_t)byte;

    switch (xrec->read_state) {
        case READ_WAIT_FOR_START:
        {
            if (b == XREC_START) {
                xrec->read_state = READ_RECORD_TYPE;
            } else {
                // Ignore this byte. Remain in the wait state.
            }
            break;
        }
        case READ_RECORD_TYPE:
        {
            if (b == '1') {
                xrec->type = XREC_DATA_16BIT;
                xrec->read_state = READ_COUNT;
            } else if (b == '9') {
                xrec->type = XREC_TERMINATION_16BIT;
                xrec->read_state = READ_COMPLETE;
            } else {
                // Anything else is who knows, so revert to the wait state
                // to try to re-sync.
                xrec->last_strict_error = XREC_ERROR_UNKNOWN_RECORD_TYPE;
                xrec->read_state = READ_WAIT_FOR_START;
            }
            break;
        }
        case READ_COUNT:
        {
            xrec->byte_count = (unsigned int)b + (int)1;
            xrec->data[xrec->length++] = b;
            xrec->read_state = READ_ADDRESS_HIGH;
            break;
        }
        case READ_ADDRESS_HIGH:
        case READ_ADDRESS_LOW:
        {
            xrec->data[xrec->length++] = b;
            xrec->read_state++;
            break;
        }
        case READ_DATA:
        {
            xrec->data[xrec->length++] = b;
            if (--xrec->byte_count == 0) {
                // Restore the byte count to its correct value and move on.
                xrec->byte_count = xrec->length - 2 - 1;  // two address bytes, one length byte.
                xrec->read_state = READ_CHECKSUM;
            }
            break;
        }
        case READ_CHECKSUM:
        {
            xrec->data[xrec->length++] = b;
            xrec->read_state = READ_COMPLETE;
            break;
        }
    }
    
    // If we have reached either terminal state, invoke the appropriate callback.
    if (xrec->read_state == READ_COMPLETE) {
        // Get the address into a single value. It occupies bytes two and three
        // of the data.
        uint16_t address = (xrec->data[1] << 8) | (xrec->data[2]);
        int checksum = 0;
        if (xrec->type == XREC_DATA_16BIT) {
            // Compute the checksum across the buffer so far. Use an unsigned
            // value to accumulate and ignore rollover/carry.
            uint8_t sum = 0;
            for (int i = 0; i < xrec->length - 1; i++) {
                sum += xrec->data[i];
            }
            // Checksum is the one's complement of the lower byte of the sum.
            uint8_t invsum = ~sum;
            uint8_t lastbyte = xrec->data[xrec->length - 1];
            checksum = lastbyte - invsum;
            if (checksum != 0) {
                xrec->last_strict_error = XREC_ERROR_INVALID_CHECKSUM;
            }
        }
        
        xrec_data_read(xrec, xrec->type, address, &xrec->data[3], xrec->byte_count, checksum != 0);
        
        // Reset the state.
        xrec->read_state = READ_WAIT_FOR_START;
        xrec->type = 0;
        xrec->byte_count = 0;
        xrec->length = 0;
    }
}

void
xrec_read_bytes (struct xrec_state * restrict xrec,
                 const char * restrict data,
                 int count) {
    while (count > 0) {
        xrec_read_byte(xrec, *data++);
        --count;
    }
}
