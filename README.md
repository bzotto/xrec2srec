# xrec2srec
A utility (and library) for parsing SWTPC "X-record" binary load format. Southwest Technical Products published software for SWTPC 6800 computer system (1975) on cassette tape. The "native" cassette format was the [Motorola S-record](https://en.wikipedia.org/wiki/SREC_(file_format)) ASCII format, but this is an inefficient encoding and was very slow to load. SWTPC published some tapes (e.g. [here](https://archive.org/details/swtpc-8k-basic-version-2.3)) that also featured a small loader stub and then the actual contents in a more compact "binary fomat". 

This utility can directly parse that binary format and turn it into standard S-record text for use elsewhere. (I refer to this format as "X-record" because of its similarity to S-record but with "X" as a record marker.)

## Building and using the tool

It's all in two files and there are no dependencies beyond the C standard libs. So go ahead and:

     cc main.c xrec2srec.c -o xrec2srec

Then just:

    ./xrec2srec input.bin 

Output is to stdout. The parser is fairly forgiving, but the tool will indicate after completion if any errors or warnings were encountered. If you are interested in strict adherence then treat any such warnings as indicating problems with the input data, especially a checksum error.

## Using the xrec parsing library

You can also incorporate the X-record parser into your own program. Just take the `xrec.h` and `xrec.c` files, and see the comments in `xrec.h` for how to invoke it and how to structure the callback. As with all binary parsers, I make no 

## What is the X-record format?

I reverse engineered the format from original examples and dissassmebled 6800 parse code. It is similar in structure to S-records, but instead of using "S" to mark a record start, it uses "X", with the contents of the record in raw binary rather than ASCII hex. So I guess I'm calling it "X-record". 

The records have the following structure:

1. Record start: Each record begins with a an uppercase letter "X" character (ASCII).
2. Record type: A single numeric digit (ASCII), defining the type of the record.  
3. Byte count: A single unsigned byte value indicating the number of raw data bytes to expect in the data field, minus one. So a zero value here indicates one byte of raw data; an 0xFF indicates 25**6** bytes of data. (There is never a need to express a zero-length data record.) 
4. Address: Two bytes in big-endian order giving the start address of the bytes in this record.
5. Data: Raw bytes of data, the number of which was derived from the byte count field above.
6. Checksum: A single byte value equal to the least-significant byte of the one's-complement of the sum of each byte of the byte count, address, and data fields. 

Fields 3-6 are only used for data records (type 1).

I have only encountered record types `X1` (data) and `X9` (termination), so that is all that is supported here. Presumably the other S-record formats could be easily added if they exist somewhere.  



