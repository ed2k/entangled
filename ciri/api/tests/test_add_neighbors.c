/*
 * Copyright (c) 2018 IOTA Stiftung
 * https://github.com/iotaledger/entangled
 *
 * Refer to the LICENSE file for licensing information
 */

#include <unity/unity.h>

#include "ciri/api/api.h"
#include "gossip/node.h"

static iota_api_t api;
static node_t node;

void test_add_neighbors(void) {
  add_neighbors_req_t *req = add_neighbors_req_new();
  add_neighbors_res_t *res = add_neighbors_res_new();

  TEST_ASSERT_EQUAL_INT(neighbors_count(node.neighbors), 0);

  add_neighbors_req_uris_add(req, "udp://8.8.8.1:15001");
  add_neighbors_req_uris_add(req, "udp://8.8.8.2:15002");
  add_neighbors_req_uris_add(req, "tcp://8.8.8.3:15003");
  add_neighbors_req_uris_add(req, "tcp://8.8.8.4:15004");

  TEST_ASSERT(iota_api_add_neighbors(&api, req, res) == RC_OK);

  TEST_ASSERT_EQUAL_INT(neighbors_count(node.neighbors), 4);
  TEST_ASSERT_EQUAL_INT(res->added_neighbors, 4);

  neighbor_t *neighbor = node.neighbors;
  TEST_ASSERT_EQUAL_STRING(neighbor->endpoint.host, "8.8.8.4");
  TEST_ASSERT_EQUAL_INT(neighbor->endpoint.port, 15004);
  TEST_ASSERT_EQUAL_INT(neighbor->endpoint.protocol, PROTOCOL_TCP);
  neighbor = neighbor->next;

  TEST_ASSERT_EQUAL_STRING(neighbor->endpoint.host, "8.8.8.3");
  TEST_ASSERT_EQUAL_INT(neighbor->endpoint.port, 15003);
  TEST_ASSERT_EQUAL_INT(neighbor->endpoint.protocol, PROTOCOL_TCP);
  neighbor = neighbor->next;

  TEST_ASSERT_EQUAL_STRING(neighbor->endpoint.host, "8.8.8.2");
  TEST_ASSERT_EQUAL_INT(neighbor->endpoint.port, 15002);
  TEST_ASSERT_EQUAL_INT(neighbor->endpoint.protocol, PROTOCOL_UDP);
  neighbor = neighbor->next;

  TEST_ASSERT_EQUAL_STRING(neighbor->endpoint.host, "8.8.8.1");
  TEST_ASSERT_EQUAL_INT(neighbor->endpoint.port, 15001);
  TEST_ASSERT_EQUAL_INT(neighbor->endpoint.protocol, PROTOCOL_UDP);
  neighbor = neighbor->next;

  TEST_ASSERT_NULL(neighbor);

  add_neighbors_req_free(&req);
  add_neighbors_res_free(&res);
}

void test_add_neighbors_with_already_paired(void) {
  add_neighbors_req_t *req = add_neighbors_req_new();
  add_neighbors_res_t *res = add_neighbors_res_new();

  TEST_ASSERT_EQUAL_INT(neighbors_count(node.neighbors), 4);

  add_neighbors_req_uris_add(req, "udp://8.8.8.1:15001");
  add_neighbors_req_uris_add(req, "udp://8.8.8.5:15005");
  add_neighbors_req_uris_add(req, "tcp://8.8.8.3:15003");

  TEST_ASSERT(iota_api_add_neighbors(&api, req, res) == RC_OK);

  TEST_ASSERT_EQUAL_INT(neighbors_count(node.neighbors), 5);
  TEST_ASSERT_EQUAL_INT(res->added_neighbors, 1);

  neighbor_t *neighbor = node.neighbors;
  TEST_ASSERT_EQUAL_STRING(neighbor->endpoint.host, "8.8.8.5");
  TEST_ASSERT_EQUAL_INT(neighbor->endpoint.port, 15005);
  TEST_ASSERT_EQUAL_INT(neighbor->endpoint.protocol, PROTOCOL_UDP);

  add_neighbors_req_free(&req);
  add_neighbors_res_free(&res);
}

void test_add_neighbors_with_invalid(void) {
  add_neighbors_req_t *req = add_neighbors_req_new();
  add_neighbors_res_t *res = add_neighbors_res_new();

  TEST_ASSERT_EQUAL_INT(neighbors_count(node.neighbors), 5);

  add_neighbors_req_uris_add(req, "udp://8.8.8.6:15006");
  add_neighbors_req_uris_add(req, "udp://8.8.8.7@15007");
  add_neighbors_req_uris_add(req, "udp://8.8.8.8:15008");

  TEST_ASSERT(iota_api_add_neighbors(&api, req, res) ==
              RC_NEIGHBOR_FAILED_URI_PARSING);

  TEST_ASSERT_EQUAL_INT(neighbors_count(node.neighbors), 6);
  TEST_ASSERT_EQUAL_INT(res->added_neighbors, 1);

  neighbor_t *neighbor = node.neighbors;
  TEST_ASSERT_EQUAL_STRING(neighbor->endpoint.host, "8.8.8.6");
  TEST_ASSERT_EQUAL_INT(neighbor->endpoint.port, 15006);
  TEST_ASSERT_EQUAL_INT(neighbor->endpoint.protocol, PROTOCOL_UDP);

  add_neighbors_req_free(&req);
  add_neighbors_res_free(&res);
}

int main(void) {
  UNITY_BEGIN();

  api.node = &node;

  node.neighbors = NULL;
  rw_lock_handle_init(&node.neighbors_lock);

  RUN_TEST(test_add_neighbors);
  RUN_TEST(test_add_neighbors_with_already_paired);
  RUN_TEST(test_add_neighbors_with_invalid);

  neighbors_free(&node.neighbors);
  rw_lock_handle_destroy(&node.neighbors_lock);

  return UNITY_END();
}
