#include "ethernet_encoding.h"

// Deleted: This file has been replaced by encoding.c

/**
 * @brief Encode 4-bit data into three chip symbols (0, 1, 2) based on the provided mapping table.
 * @param data: 4-bit encoded data (0x0 to 0xF)
 * @param chip1: Pointer to store the first chip value (0, 1, or 2)
 * @param chip2: Pointer to store the second chip value (0, 1, or 2)
 * @param chip3: Pointer to store the third chip value (0, 1, or 2)
 * @retval None
 */
void Ethernet_EncodeData(uint8_t data, uint8_t *chip1, uint8_t *chip2, uint8_t *chip3)
{
    // Lookup table based on the provided encoding table
    static const uint8_t lookup_table[16][3] = {
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

    if (data >= 16) {
        // Invalid input - clamp to valid range
        data = 0;
    }

    *chip1 = lookup_table[data][0];
    *chip2 = lookup_table[data][1];
    *chip3 = lookup_table[data][2];
}