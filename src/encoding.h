#ifndef ENCODING_H_
#define ENCODING_H_

#include <stdint.h>

/**
 * @brief Encodes a 4-bit data value into three chip symbols
 * @param data 4-bit value to encode (0-15)
 * @param chip1 First chip symbol (0-2)
 * @param chip2 Second chip symbol (0-2)
 * @param chip3 Third chip symbol (0-2)
 */
void Response_EncodeData(uint8_t data, volatile uint8_t *chip1, volatile uint8_t *chip2, volatile uint8_t *chip3);

#endif /* ENCODING_H_ */