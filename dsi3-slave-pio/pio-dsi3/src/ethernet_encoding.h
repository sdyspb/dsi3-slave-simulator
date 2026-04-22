#ifndef ETHERNET_ENCODING_H
#define ETHERNET_ENCODING_H

// Deleted: This file has been replaced by encoding.h

#include <stdint.h>

/**
 * @brief Encodes a 4-bit data value into three chip symbols (0, 1, 2).
 * @param data: Input 4-bit data (0-15)
 * @param chip1: Output for first chip value
 * @param chip2: Output for second chip value
 * @param chip3: Output for third chip value
 */
void Ethernet_EncodeData(uint8_t data, uint8_t *chip1, uint8_t *chip2, uint8_t *chip3);

#endif /* ETHERNET_ENCODING_H */