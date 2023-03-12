#include "encryption.h"

#include <mbedtls/md5.h>
#include <mbedtls/ctr_drbg.h>
#include <zephyr/random/rand32.h>

LOG_MODULE_REGISTER(ENCRYPTION);

uint8_t device_password[] = {0x36, 0x39, 0x37, 0x32, 0x34, 0x35, 0x33, 0x39};

mbedtls_ctr_drbg_context ctr_drbg;

int init_mbedtls_encryption()
{
    mbedtls_ctr_drbg_init(&ctr_drbg);

    return 0;
}

int generate_random_numbers(uint8_t len, uint8_t *buf)
{
    int ret = 0;
    sys_rand_get(buf, len);
    return 0;
}

int generate_md5_checksum(uint8_t *input, uint8_t input_len, uint8_t *output_buf)
{
    int ret = 0;

    ret = mbedtls_md5(input, input_len, output_buf);
    if(ret){
        LOG_ERR("Generate MD5 fail (err %d)", ret);
        return ret;
    }

    return 0;
}

