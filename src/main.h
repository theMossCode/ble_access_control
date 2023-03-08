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

#define DEFAULT_ADDR_FILTER_LEN             25

#define DEFAULT_TIMEOUT_FOR_SCANS_SECONDS   10

#endif