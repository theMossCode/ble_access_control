#include "storage.h"

#include <zephyr/fs/nvs.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/device.h>

#include <nrfx_nvmc.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(STORAGE);

#define NVS_PARTITION		    storage_partition
#define NVS_PARTITION_DEVICE	FIXED_PARTITION_DEVICE(NVS_PARTITION)
#define NVS_PARTITION_OFFSET	FIXED_PARTITION_OFFSET(NVS_PARTITION)

struct nvs_fs fs;

int init_nvs()
{
    int ret = 0;
    struct flash_pages_info info;
	fs.flash_device = NVS_PARTITION_DEVICE;
	if (!device_is_ready(fs.flash_device)){
        LOG_ERR("nvs device not ready");
		return -ENODEV;
	}

	fs.offset = NVS_PARTITION_OFFSET;
	ret = flash_get_page_info_by_offs(fs.flash_device, fs.offset, &info);
	if (ret){
		LOG_ERR("nvs failed to get page info");
		return ret;
	}

	fs.sector_size = info.size;
	fs.sector_count = 2U;

	ret = nvs_mount(&fs);
	if(ret){
		LOG_ERR("NVS mount fail(err %d)", ret);
		return ret;
	}

    return 0;
}

int read_nvs_data(uint8_t id, uint8_t *data, uint8_t len)
{
    int ret = 0;
    ret = nvs_read(&fs, id, data, len);
    if((ret != len) && (ret != 0)){
        return -EMSGSIZE;
    }
    return 0;
}

int write_nvs_data(uint8_t id, uint8_t *data, uint8_t len)
{
    int ret = 0;
    ret = nvs_write(&fs, id, data, len);
    if((ret != len) && (ret != 0)){
        return -EMSGSIZE;
    }
    return 0;
}

int read_uicr(int customer_addr, uint32_t *value)
{
    *value = nrfx_nvmc_uicr_word_read(&NRF_UICR->CUSTOMER[customer_addr]);
    return 0;
}

int update_uicr(uint32_t TL, uint32_t TS, uint32_t VC, uint32_t VW)
{
    // Erase
    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Een;
    while(NRF_NVMC->READY == NVMC_READY_READY_Busy){;}
    NRF_NVMC->ERASEUICR = 1;
    while(NRF_NVMC->READY == NVMC_READY_READY_Busy){;}

    // Write
    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen;
    while(NRF_NVMC->READY == NVMC_READY_READY_Busy){;}
    NRF_UICR->CUSTOMER[TL_UICR_ADDRESS] = TL;
    while(NRF_NVMC->READY == NVMC_READY_READY_Busy){;}
    NRF_UICR->CUSTOMER[TS_UICR_ADDRESS] = TS;
    while(NRF_NVMC->READY == NVMC_READY_READY_Busy){;}
    NRF_UICR->CUSTOMER[VC_UICR_ADDRESS] = VC;
    while(NRF_NVMC->READY == NVMC_READY_READY_Busy){;}
    NRF_UICR->CUSTOMER[VW_UICR_ADDRESS] = VW;
    while(NRF_NVMC->READY == NVMC_READY_READY_Busy){;}

    // Check
    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren;
    while(NRF_NVMC->READY == NVMC_READY_READY_Busy){;}
    LOG_DBG("UICR VAL TL %u", NRF_UICR->CUSTOMER[TL_UICR_ADDRESS]);
    LOG_DBG("UICR VAL TS %u", NRF_UICR->CUSTOMER[TS_UICR_ADDRESS]);
    LOG_DBG("UICR VAL VC %u", NRF_UICR->CUSTOMER[VC_UICR_ADDRESS]);
    LOG_DBG("UICR VAL VW %u", NRF_UICR->CUSTOMER[VW_UICR_ADDRESS]);
    if(NRF_UICR->CUSTOMER[TL_UICR_ADDRESS] != TL ||
        NRF_UICR->CUSTOMER[TS_UICR_ADDRESS] != TS ||
        NRF_UICR->CUSTOMER[VC_UICR_ADDRESS] != VC ||
        NRF_UICR->CUSTOMER[VW_UICR_ADDRESS] != VW)
    {
        LOG_ERR("Write UICR check fail");
        return -ENODATA;
    }
    return 0;
}