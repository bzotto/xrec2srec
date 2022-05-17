/*
 * xrec.h
 *
 * A simple library for reading "xrec" binary load files, as
 * used by SWTPC tapes and maybe others.
 *
 * Copyright (c) 2022 Ben Zotto
 * Provided with absolutely no warranty, use at your own risk only.
 * Use and distribute freely, mark modified copies as such. This was
 * inspired by Kimmo Kulovesi's kk_srec.
 *
 *      USAGE
 *      -----
 * 
 * To decompose an xrec binary file into raw binary data, the library is used
 * as follows:
 *
 *      struct xrec_state xrec;
 *      xrec_begin_read(xrec);
 *      xrec_read_bytes(&xrec, my_input_bytes, length_of_my_input_bytes);
 *
 * The function `xrec_read_bytes` may be called any number of times with
 * any amount of data at a time (i.e., it does not need to be called with
 * full lines at a time). At any time during a call to `xrec_read_bytes`, the
 * callback function `xrec_data_read` may be called by the library.
 *
 * An optional void * "context" field is provided in the state structure in case
 * the caller needs to associate the callback with any particular object.
 *
 * The callbacks must be provided by the user, e.g., as follows:
 *
 *      void xrec_data_read (struct xrec_state *xrec,
 *                           int record_type,
 *                           uint16_t address,
 *                           uint8_t *data,
 *                           int length, int checksum_error) {
 *          if (record_type == XREC_DATA_16BIT && !checksum_error) {
 *              (void) fseek(outfile, address, SEEK_SET);
 *              (void) fwrite(data, 1, length, outfile);
 *          } else if (record_type == XREC_TERMINATION_16BIT) {
 *              (void) fclose(outfile);
 *          }
 *      }
 *
 * The library is quite forgiving, and has no error modes that stop its
 * processing. Data that doesn't begin with the X1/X9 start tokens will
 * generally be ignored, but of course feeding the parser garbage might
 * end up triggering incorrect analysis of garbage data. The checksum
 * error is presetned to the callback but it's up to the client to decide
 * what to do with it. The xrec structure does contain a last_strict_error
 * field, which will be set when any unrecognized records or checksum errors
 * occur. If the client wishes to parse the input "strictly", they can:
 *          (a) ensure that this field is clear after completing parsing and
 *          (b) ensure that the xrec->read_state field is clear (0) and
 *          (c) ensure that the final record read out was a termination.
 *
 */

#ifndef XREC_H
#define XREC_H

#include <stdint.h>

enum xrec_record_number {
    XREC_DATA_16BIT         = 1,
    XREC_TERMINATION_16BIT  = 9
};

enum xrec_error {
    XREC_ERROR_NONE,
    XREC_ERROR_UNKNOWN_RECORD_TYPE = 1,
    XREC_ERROR_INVALID_CHECKSUM
};

typedef struct xrec_state {
    int             read_state;
    int             type;
    int             byte_count;
    int             length;
    uint8_t         data[1 + 2 + 256 + 1];  // This buffer contains the count, address, data, and checksum.
    enum xrec_error last_strict_error;
    void *          context;
} xrec_t;

// Begin reading
void xrec_begin_read(struct xrec_state *xrec);

// Read a single character
void xrec_read_byte(struct xrec_state *xrec, char chr);

// Read `count` characters from `data`
void xrec_read_bytes(struct xrec_state * restrict xrec,
                     const char * restrict data,
                     int count);

// Callback - this must be provided by the user of the library.
// The arguments are as follows:
//      xrec            - Pointer to the xrec_state structure
//      record_type     - Record type number (0-9)
//      address         - Address field of the record (16 bits)
//      data            - Pointer to the start of the data payload
//      length          - Length of data payload
//      checksum_error  - Nonzero if this record uses a checksum and it doesn't match
//
// Note that while the interpreted record is passed entirely as arguments,
// the raw data is available in the `xrec` structure, which includes the
// address and checksum as part of data.
extern void xrec_data_read(struct xrec_state *xrec,
                           int record_type,
                           uint16_t address,
                           uint8_t *data,
                           int length,
                           int checksum_error);
#endif
