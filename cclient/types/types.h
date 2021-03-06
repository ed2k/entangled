/*
 * Copyright (c) 2018 IOTA Stiftung
 * https://github.com/iotaledger/entangled
 *
 * Refer to the LICENSE file for licensing information
 */

#ifndef CCLIENT_TYPES_H
#define CCLIENT_TYPES_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "common/errors.h"
#include "common/model/transaction.h"
#include "common/trinary/trit_array.h"
#include "common/trinary/tryte.h"

#include "utils/containers/hash/hash243_queue.h"
#include "utils/containers/hash/hash243_stack.h"
#include "utils/containers/hash/hash8019_queue.h"
#include "utils/containers/hash/hash8019_stack.h"
#include "utils/containers/hash/hash81_queue.h"
#include "utils/containers/hash/hash_array.h"
#include "utils/logger_helper.h"

#include "utarray.h"
#include "utlist.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  size_t length;
  char* data;
} char_buffer_t;

typedef struct {
  size_t size;
  int* array;
} int_array;

void logger_init_types();
void logger_destroy_types();

char_buffer_t* char_buffer_new();
retcode_t char_buffer_allocate(char_buffer_t* in, const size_t n);
retcode_t char_buffer_set(char_buffer_t* in, char const* const str);
void char_buffer_free(char_buffer_t* in);

typedef UT_array* transaction_array_t;
transaction_array_t transaction_array_new();
void transaction_array_push_back(transaction_array_t txs,
                                 iota_transaction_t const* const tx);
size_t transaction_array_len(transaction_array_t txs);
void transaction_array_free(transaction_array_t txs);
iota_transaction_t* transaction_array_at(transaction_array_t txs, size_t index);
#define TX_OBJS_FOREACH(txs, tx)                                 \
  for (tx = (iota_transaction_t*)utarray_front(txs); tx != NULL; \
       tx = (iota_transaction_t*)utarray_next(txs, tx))

#ifdef __cplusplus
}
#endif

#endif  // CCLIENT_TYPES_H
