#ifndef ENCRYPTION_H
#define ENCRYPTION_H

#include "main.h"

/**
 * @brief initialise the mbedtls module
 * 
 */
int init_mbedtls_encryption();

/**
 * @brief generate random numbers (uint8_t) of lenth len)
 * 
 * @param len number of random numbers to generate
 * @param output_buf buffer to store generated numbers
 */
int generate_random_numbers(uint8_t len, uint8_t *output_buf);

/**
 * @brief generate md5 checksum for data
 * 
 * @param input input data
 * @param input_len size of input data
 * @param output_buf output buf = md5(input data)
 * 
 * @return 0 on success
 */
int generate_md5_checksum(uint8_t *input, uint8_t input_len, uint8_t *output_buf);

#endif