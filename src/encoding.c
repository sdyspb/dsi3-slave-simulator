#include "encoding.h"

void Response_EncodeData(uint8_t data, volatile uint8_t *chip1, volatile uint8_t *chip2, volatile uint8_t *chip3)
{
    // Define the encoding table for 4-bit data to 3-symbol response
    // Each symbol can have values 0, 1, or 2
    // Table format: for each data value (0-15), define the three corresponding chip values
    static const uint8_t encoding_table[16][3] = {
        {1, 1, 0},  // 0000 -> 0
        {2, 1, 1},  // 0001 -> 1
        {1, 0, 2},  // 0010 -> 2
        {2, 0, 2},  // 0011 -> 3
        {1, 0, 0},  // 0100 -> 4
        {2, 1, 2},  // 0101 -> 5
        {1, 1, 2},  // 0110 -> 6
        {2, 0, 1},  // 0111 -> 7
        {2, 2, 0},  // 1000 -> 8
        {2, 1, 0},  // 1001 -> 9
        {1, 2, 2},  // 1010 -> A
        {2, 2, 1},  // 1011 -> B
        {1, 2, 0},  // 1100 -> C
        {2, 0, 0},  // 1101 -> D
        {1, 0, 1},  // 1110 -> E
        {1, 2, 1}   // 1111 -> F
    };

    // Ensure data is within valid range
    if (data > 15) {
        data = 0; // Default to first entry if invalid
    }

    // Get the encoded values from the table
    *chip1 = encoding_table[data][0];
    *chip2 = encoding_table[data][1];
    *chip3 = encoding_table[data][2];
}