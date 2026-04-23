#ifndef ENCODING_H
#define ENCODING_H

#include <stdint.h>

/**
 * @brief Encodes a 4-bit data value into three chip symbols (0, 1, 2).
 * @param data: Input 4-bit data (0-15)
 * @param chip1: Output for first chip value
 * @param chip2: Output for second chip value
 * @param chip3: Output for third chip value
 */
void Response_EncodeData(uint8_t data, uint8_t *chip1, uint8_t *chip2, uint8_t *chip3);

#endif /* ENCODING_H */