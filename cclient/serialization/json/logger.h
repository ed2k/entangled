/*
 * Copyright (c) 2018 IOTA Stiftung
 * https://github.com/iotaledger/entangled
 *
 * Refer to the LICENSE file for licensing information
 */

#ifndef CCLIENT_SERIALIZATION_JSON_LOGGER_H
#define CCLIENT_SERIALIZATION_JSON_LOGGER_H

#include "utils/logger_helper.h"

#ifdef __cplusplus
extern "C" {
#endif

extern logger_id_t json_logger_id;

void logger_init_json_serializer();
void logger_destroy_json_serializer();

#ifdef __cplusplus
}
#endif

#endif  // CCLIENT_SERIALIZATION_JSON_SERIALIZER_H
