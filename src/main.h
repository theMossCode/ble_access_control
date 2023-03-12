#ifndef MAIN_H
#define MAIN_H

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>

#include "main_evt_defs.h"
#include "input.h"
#include "output.h"
#include "storage.h"
#include "ble.h"
#include "encryption.h"

#define DEFAULT_ADDR_FILTER_LEN                 25
#define DEFAULT_OVERHEAD_LIGHT_TIMEOUT_NO_SCAN  (5 * 60)

#endif