#ifndef STORAGE_H
#define STORAGE_H

#include "main.h"

#define TL_UICR_ADDRESS         0
#define TS_UICR_ADDRESS         1
#define VC_UICR_ADDRESS         2
#define VW_UICR_ADDRESS         3

/**
 * @brief init NVS on internal flash
 * 
 * @return 0 on success
 */
int init_nvs();

/**
 * @brief read data from flash
 * 
 * @param id NVS data id
 * @param data destination buffer
 * @param len length of buffer
 * @return 0 on success
 */
int read_nvs_data(uint8_t id, uint8_t *data, uint8_t len);

/**
 * @brief write data to NVS
 * 
 * @param id NVS data id
 * @param data data to write
 * @param len number of bytes
 * @return 0 on success
 */
int write_nvs_data(uint8_t id, uint8_t *data, uint8_t len);

/**
 * @brief read uicr value
 * 
 * @param customer_addr index of uicr customer space
 * @param value pointer to variable to read to
 * @return 0 on sucess
 */
int read_uicr(int customer_addr, uint32_t *value);

/**
 * @brief update uicr values
 * 
 * @param TL
 * @param TS
 * @param VC
 * @param VW
 * 
 * @return 0 on success
 */
int update_uicr(uint32_t TL, uint32_t TS, uint32_t VC, uint32_t VW);

#endif