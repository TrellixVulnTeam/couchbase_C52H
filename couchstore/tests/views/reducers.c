/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */

#include "view_tests.h"
#include "../src/couch_btree.h"
#include "../src/views/mapreduce/mapreduce.h"
#include <string.h>
#include <inttypes.h>

#define BITMAP_SIZE 1024

#define VIEW_KV_CHUNK_THRESHOLD (7 * 1024)
#define VIEW_KP_CHUNK_THRESHOLD (6 * 1024)

int view_btree_cmp(const sized_buf *key1, const sized_buf *key2);
couchfile_modify_result *make_modres(arena* a, couchfile_modify_request *rq);
static void free_node_list(nodelist *nl)
{
    nodelist *tmp;
    tmp = nl;
    while (tmp != NULL ){
        nl = nl->next;
        free(tmp->pointer);
        free(tmp);
        tmp = nl;
    }
}

static void free_id_value(view_id_btree_value_t *value)
{
    unsigned i;

    for (i = 0; i < value->num_view_keys_map; ++i) {
        free(value->view_keys_map[i].json_keys);
    }
    free(value->view_keys_map);
}

static void free_view_value(view_btree_value_t *value)
{
    free(value->values);
}

static void free_view_reduction(view_btree_reduction_t *red)
{
    free(red->reduce_values);
}

static void test_view_id_btree_reducer(void)
{
    nodelist *nl = NULL;
    node_pointer *np = NULL;
    node_pointer *np2 = NULL;
    view_id_btree_reduction_t *r;
    char dst[MAX_REDUCTION_SIZE];
    size_t size_r;
    int i, count = 0;

    view_id_btree_key_t key1;
    char *key_bin1 = NULL;
    size_t key_bin1_size = 0;

    view_id_btree_value_t data1;
    char *data_bin1 = NULL;
    size_t data_bin1_size = 0;

    view_id_btree_reduction_t reduction1;
    char reduction_bin1[512];
    size_t reduction_bin1_size = 0;
    view_id_btree_reduction_t reduction2;
    char reduction_bin2[512];
    size_t reduction_bin2_size = 0;

    nodelist *nl2 = NULL;

    view_id_btree_key_t key2;
    char *key_bin2 = NULL;
    size_t key_bin2_size = 0;

    view_id_btree_value_t data2;
    char *data_bin2 = NULL;
    size_t data_bin2_size = 0;

    key1.partition = 67;
    key1.doc_id.buf = "doc_00000057";
    key1.doc_id.size = sizeof("doc_00000057") - 1;
    assert(encode_view_id_btree_key(&key1, &key_bin1, &key_bin1_size) == COUCHSTORE_SUCCESS);

    data1.partition = 67;
    data1.num_view_keys_map = 2;
    data1.view_keys_map = (view_keys_mapping_t *) malloc(sizeof(view_keys_mapping_t) * 2);
    assert(data1.view_keys_map != NULL);

    data1.view_keys_map[0].view_id = 0;
    data1.view_keys_map[0].num_keys = 2;
    data1.view_keys_map[0].json_keys = (sized_buf *) malloc(sizeof(sized_buf) * 2);
    assert(data1.view_keys_map[0].json_keys != NULL);
    data1.view_keys_map[0].json_keys[0].buf = "-321";
    data1.view_keys_map[0].json_keys[0].size = sizeof("-321") - 1;
    data1.view_keys_map[0].json_keys[1].buf = "[123,\"foobar\"]";
    data1.view_keys_map[0].json_keys[1].size = sizeof("[123,\"foobar\"]") - 1;

    data1.view_keys_map[1].view_id = 1;
    data1.view_keys_map[1].num_keys = 1;
    data1.view_keys_map[1].json_keys = (sized_buf *) malloc(sizeof(sized_buf) * 1);
    data1.view_keys_map[1].json_keys[0].buf = "[5,6,7]";
    data1.view_keys_map[1].json_keys[0].size = sizeof("[5,6,7]") - 1;

    assert(encode_view_id_btree_value(&data1, &data_bin1, &data_bin1_size) == COUCHSTORE_SUCCESS);
    free_id_value(&data1);

    key2.partition = 57;
    key2.doc_id.buf = "foobar";
    key2.doc_id.size = sizeof("foobar") - 1;
    assert(encode_view_id_btree_key(&key2, &key_bin2, &key_bin2_size) == COUCHSTORE_SUCCESS);

    data2.partition = 57;
    data2.num_view_keys_map = 2;
    data2.view_keys_map = (view_keys_mapping_t *) malloc(sizeof(view_keys_mapping_t) * 2);
    assert(data2.view_keys_map != NULL);

    data2.view_keys_map[0].view_id = 0;
    data2.view_keys_map[0].num_keys = 1;
    data2.view_keys_map[0].json_keys = (sized_buf *) malloc(sizeof(sized_buf) * 1);
    assert(data2.view_keys_map[0].json_keys != NULL);
    data2.view_keys_map[0].json_keys[0].buf = "\"abc\"";
    data2.view_keys_map[0].json_keys[0].size = sizeof("\"abc\"") - 1;

    data2.view_keys_map[1].view_id = 1;
    data2.view_keys_map[1].num_keys = 1;
    data2.view_keys_map[1].json_keys = (sized_buf *) malloc(sizeof(sized_buf) * 1);
    data2.view_keys_map[1].json_keys[0].buf = "\"qwerty\"";
    data2.view_keys_map[1].json_keys[0].size = sizeof("\"qwerty\"") - 1;

    assert(encode_view_id_btree_value(&data2, &data_bin2, &data_bin2_size) == COUCHSTORE_SUCCESS);
    free_id_value(&data2);

    reduction1.kv_count = 11;
    memset(&reduction1.partitions_bitmap, 0, sizeof(reduction1.partitions_bitmap));
    set_bit(&reduction1.partitions_bitmap, 10);
    set_bit(&reduction1.partitions_bitmap, 1001);
    assert(encode_view_id_btree_reduction(&reduction1, reduction_bin1, &reduction_bin1_size) == COUCHSTORE_SUCCESS);

    reduction2.kv_count = 22;
    memset(&reduction2.partitions_bitmap, 0, sizeof(reduction2.partitions_bitmap));
    set_bit(&reduction2.partitions_bitmap, 7);
    set_bit(&reduction2.partitions_bitmap, 77);
    assert(encode_view_id_btree_reduction(&reduction2, reduction_bin2, &reduction_bin2_size) == COUCHSTORE_SUCCESS);

    nl = (nodelist *) malloc(sizeof(nodelist));
    assert(nl != NULL);

    count++;
    nl->data.buf = data_bin1;
    nl->data.size = data_bin1_size;
    nl->key.buf = key_bin1;
    nl->key.size = key_bin1_size;
    nl->pointer = NULL;
    nl->next = NULL;

    np = (node_pointer *) malloc(sizeof(node_pointer));
    assert(np != NULL);
    np->key.buf = key_bin1;
    np->key.size = key_bin1_size;
    np->reduce_value.buf = reduction_bin1;
    np->reduce_value.size = reduction_bin1_size;
    np->pointer = 0;
    np->subtreesize = 3;
    nl->pointer = np;

    nl2 = (nodelist *) malloc(sizeof(nodelist));
    assert(nl2 != NULL);

    count++;
    nl2->data.buf = data_bin2;
    nl2->data.size = data_bin2_size;
    nl2->key.buf = key_bin2;
    nl2->key.size = key_bin2_size;
    nl2->pointer = NULL;
    nl2->next = NULL;
    nl->next = nl2;

    assert(view_id_btree_reduce(dst, &size_r, nl, count, NULL) == COUCHSTORE_SUCCESS);
    assert(decode_view_id_btree_reduction(dst, &r) == COUCHSTORE_SUCCESS);
    assert(r->kv_count == (uint64_t) count);

    for (i = 0; i < BITMAP_SIZE; ++i) {
        if ((i != 57) && (i != 67)) {
            assert(!is_bit_set(&r->partitions_bitmap, i));
        } else {
            assert(is_bit_set(&r->partitions_bitmap, i));
        }
    }

    free_view_id_btree_reduction(r);

    np2 = (node_pointer *) malloc(sizeof(node_pointer));
    assert(np2 != NULL);
    np2->key.buf = key_bin2;
    np2->key.size = key_bin2_size;
    np2->reduce_value.buf = reduction_bin2;
    np2->reduce_value.size = reduction_bin2_size;
    np2->pointer = 0;
    np2->subtreesize = 3;
    nl2->pointer = np2;

    assert(view_id_btree_rereduce(dst, &size_r, nl, count, NULL) == COUCHSTORE_SUCCESS);
    assert(decode_view_id_btree_reduction(dst, &r) == COUCHSTORE_SUCCESS);
    assert(r->kv_count == 33);

    for (i = 0; i < BITMAP_SIZE; ++i) {
        if ((i != 7) && (i != 77) && (i != 10) && (i != 1001)) {
            assert(!is_bit_set(&r->partitions_bitmap, i));
        } else {
            assert(is_bit_set(&r->partitions_bitmap, i));
        }
    }

    free_view_id_btree_reduction(r);
    free_node_list(nl);
    free(key_bin1);
    free(key_bin2);
    free(data_bin1);
    free(data_bin2);
}

static void test_view_btree_sum_reducer(void)
{
    char *error_msg = NULL;
    view_reducer_ctx_t *ctx = NULL;
    const char *function_sources[] = { "_sum" };
    int i;

    view_btree_key_t key1, key2, key3;
    char *key1_bin = NULL;
    char *key2_bin = NULL;
    char *key3_bin = NULL;
    size_t key1_bin_size = 0;
    size_t key2_bin_size = 0;
    size_t key3_bin_size = 0;

    view_btree_value_t value1, value2, value3;
    char *value1_bin = NULL;
    char *value2_bin = NULL;
    char *value3_bin = NULL;
    size_t value1_bin_size = 0;
    size_t value2_bin_size = 0;
    size_t value3_bin_size = 0;

    view_btree_reduction_t reduction1;
    char reduction1_bin[512];
    size_t reduction1_bin_size = 0;
    view_btree_reduction_t reduction2;
    char reduction2_bin[512];
    size_t reduction2_bin_size = 0;

    nodelist *nl = NULL, *nl2 = NULL, *nl3 = NULL;
    node_pointer *np = NULL, *np2 = NULL;

    view_btree_reduction_t *red = NULL;
    char red_bin[512];
    size_t red_bin_size = 0;

    ctx = make_view_reducer_ctx(function_sources, 1, &error_msg);
    assert(ctx != NULL);

    key1.json_key.buf = "10";
    key1.json_key.size = sizeof("10") - 1;
    key1.doc_id.buf = "doc_10";
    key1.doc_id.size = sizeof("doc_10") - 1;
    assert(encode_view_btree_key(&key1, &key1_bin, &key1_bin_size) == COUCHSTORE_SUCCESS);

    key2.json_key.buf = "11";
    key2.json_key.size = sizeof("11") - 1;
    key2.doc_id.buf = "doc_11";
    key2.doc_id.size = sizeof("doc_11") - 1;
    assert(encode_view_btree_key(&key2, &key2_bin, &key2_bin_size) == COUCHSTORE_SUCCESS);

    key3.json_key.buf = "12";
    key3.json_key.size = sizeof("12") - 1;
    key3.doc_id.buf = "doc_12";
    key3.doc_id.size = sizeof("doc_12") - 1;
    assert(encode_view_btree_key(&key3, &key3_bin, &key3_bin_size) == COUCHSTORE_SUCCESS);

    value1.partition = 7;
    value1.num_values = 2;
    value1.values = (sized_buf *) malloc(sizeof(sized_buf) * 2);
    value1.values[0].buf = "100";
    value1.values[0].size = sizeof("100") - 1;
    value1.values[1].buf = "1";
    value1.values[1].size = sizeof("1") - 1;
    assert(encode_view_btree_value(&value1, &value1_bin, &value1_bin_size) == COUCHSTORE_SUCCESS);

    value2.partition = 666;
    value2.num_values = 1;
    value2.values = (sized_buf *) malloc(sizeof(sized_buf) * 1);
    value2.values[0].buf = "1000";
    value2.values[0].size = sizeof("1000") - 1;
    assert(encode_view_btree_value(&value2, &value2_bin, &value2_bin_size) == COUCHSTORE_SUCCESS);

    value3.partition = 1023;
    value3.num_values = 1;
    value3.values = (sized_buf *) malloc(sizeof(sized_buf) * 1);
    value3.values[0].buf = "5000.33";
    value3.values[0].size = sizeof("5000.33") - 1;
    assert(encode_view_btree_value(&value3, &value3_bin, &value3_bin_size) == COUCHSTORE_SUCCESS);

    nl = (nodelist *) malloc(sizeof(nodelist));
    assert(nl != NULL);
    nl->data.buf = value1_bin;
    nl->data.size = value1_bin_size;
    nl->key.buf = key1_bin;
    nl->key.size = key1_bin_size;
    nl->pointer = NULL;

    nl2 = (nodelist *) malloc(sizeof(nodelist));
    assert(nl2 != NULL);
    nl2->data.buf = value2_bin;
    nl2->data.size = value2_bin_size;
    nl2->key.buf = key2_bin;
    nl2->key.size = key2_bin_size;
    nl2->pointer = NULL;
    nl->next = nl2;

    nl3 = (nodelist *) malloc(sizeof(nodelist));
    assert(nl3 != NULL);
    nl3->data.buf = value3_bin;
    nl3->data.size = value3_bin_size;
    nl3->key.buf = key3_bin;
    nl3->key.size = key3_bin_size;
    nl3->pointer = NULL;
    nl3->next = NULL;
    nl2->next = nl3;

    assert(view_btree_reduce(red_bin, &red_bin_size, nl, 3, ctx) == COUCHSTORE_SUCCESS);
    assert(decode_view_btree_reduction(red_bin, red_bin_size, &red) == COUCHSTORE_SUCCESS);
    assert(red->kv_count == 4);
    assert(red->num_values == 1);
    assert(red->reduce_values[0].size == (sizeof("6101.33") - 1));
    assert(strncmp(red->reduce_values[0].buf, "6101.33", sizeof("6101.33") - 1) == 0);

    for (i = 0; i < BITMAP_SIZE; ++i) {
        if ((i != 7) && (i != 666) && (i != 1023)) {
            assert(!is_bit_set(&red->partitions_bitmap, i));
        } else {
            assert(is_bit_set(&red->partitions_bitmap, i));
        }
    }
    free_view_btree_reduction(red);
    red = NULL;

    /* Test _sum reduce error */

    value3.values[0].buf = "\"foobar\"";
    value3.values[0].size = sizeof("\"foobar\"") - 1;
    free(value3_bin);
    assert(encode_view_btree_value(&value3, &value3_bin, &value3_bin_size) == COUCHSTORE_SUCCESS);

    nl3->data.buf = value3_bin;
    nl3->data.size = value3_bin_size;

    assert(view_btree_reduce(red_bin, &red_bin_size, nl, 3, ctx) == COUCHSTORE_ERROR_REDUCER_FAILURE);
    assert(ctx->error != NULL);
    assert(strcmp(ctx->error, "Value is not a number (key 12)") == 0);

    /* Test _sum rereduce */

    reduction1.kv_count = 11;
    memset(&reduction1.partitions_bitmap, 0, sizeof(reduction1.partitions_bitmap));
    set_bit(&reduction1.partitions_bitmap, 10);
    set_bit(&reduction1.partitions_bitmap, 1011);
    reduction1.num_values = 1;
    reduction1.reduce_values = (sized_buf *) malloc(sizeof(sized_buf) * 1);
    assert(reduction1.reduce_values != NULL);
    reduction1.reduce_values[0].buf = "4444.11";
    reduction1.reduce_values[0].size = sizeof("4444.11") - 1;
    assert(encode_view_btree_reduction(&reduction1, reduction1_bin, &reduction1_bin_size) == COUCHSTORE_SUCCESS);

    reduction2.kv_count = 44;
    memset(&reduction2.partitions_bitmap, 0, sizeof(reduction2.partitions_bitmap));
    set_bit(&reduction2.partitions_bitmap, 10);
    set_bit(&reduction2.partitions_bitmap, 777);
    set_bit(&reduction2.partitions_bitmap, 333);
    reduction2.num_values = 1;
    reduction2.reduce_values = (sized_buf *) malloc(sizeof(sized_buf) * 1);
    assert(reduction2.reduce_values != NULL);
    reduction2.reduce_values[0].buf = "-100";
    reduction2.reduce_values[0].size = sizeof("-100") - 1;
    assert(encode_view_btree_reduction(&reduction2, reduction2_bin, &reduction2_bin_size) == COUCHSTORE_SUCCESS);

    np = (node_pointer *) malloc(sizeof(node_pointer));
    assert(np != NULL);
    np->key.buf = key1_bin;
    np->key.size = key1_bin_size;
    np->reduce_value.buf = reduction1_bin;
    np->reduce_value.size = reduction1_bin_size;
    np->pointer = 0;
    np->subtreesize = 222;
    nl->pointer = np;

    np2 = (node_pointer *) malloc(sizeof(node_pointer));
    assert(np2 != NULL);
    np2->key.buf = key2_bin;
    np2->key.size = key2_bin_size;
    np2->reduce_value.buf = reduction2_bin;
    np2->reduce_value.size = reduction2_bin_size;
    np2->pointer = 0;
    np2->subtreesize = 333;
    nl2->pointer = np2;

    assert(view_btree_rereduce(red_bin, &red_bin_size, nl, 2, ctx) == COUCHSTORE_SUCCESS);
    assert(decode_view_btree_reduction(red_bin, red_bin_size, &red) == COUCHSTORE_SUCCESS);
    assert(red->kv_count == 55);
    assert(red->num_values == 1);
    assert(red->reduce_values[0].size == (sizeof("4344.11") - 1));
    assert(strncmp(red->reduce_values[0].buf, "4344.11", sizeof("4344.11") - 1) == 0);

    for (i = 0; i < BITMAP_SIZE; ++i) {
        if ((i != 10) && (i != 333) && (i != 777) && (i != 1011)) {
            assert(!is_bit_set(&red->partitions_bitmap, i));
        } else {
            assert(is_bit_set(&red->partitions_bitmap, i));
        }
    }

    /* Test _sum rereduce error */

    free_view_btree_reduction(red);
    red = NULL;

    reduction2.reduce_values[0].buf = "true";
    reduction2.reduce_values[0].size = sizeof("true") - 1;
    assert(encode_view_btree_reduction(&reduction2, reduction2_bin, &reduction2_bin_size) == COUCHSTORE_SUCCESS);

    np2->reduce_value.buf = reduction2_bin;
    np2->reduce_value.size = reduction2_bin_size;

    assert(view_btree_rereduce(red_bin, &red_bin_size, nl, 2, ctx) == COUCHSTORE_ERROR_REDUCER_FAILURE);
    assert(ctx->error != NULL);
    assert(strcmp(ctx->error, "Value is not a number") == 0);

    free_view_reduction(&reduction1);
    free_view_reduction(&reduction2);
    free_view_btree_reduction(red);
    free_view_reducer_ctx(ctx);
    free(key1_bin);
    free(key2_bin);
    free(key3_bin);
    free_view_value(&value1);
    free_view_value(&value2);
    free_view_value(&value3);
    free(value1_bin);
    free(value2_bin);
    free(value3_bin);
    free_node_list(nl);
}

static void test_view_btree_count_reducer(void)
{
    char *error_msg = NULL;
    view_reducer_ctx_t *ctx = NULL;
    const char *function_sources[] = { "_count" };
    int i;

    view_btree_key_t key1, key2, key3;
    char *key1_bin = NULL;
    char *key2_bin = NULL;
    char *key3_bin = NULL;
    size_t key1_bin_size = 0;
    size_t key2_bin_size = 0;
    size_t key3_bin_size = 0;

    view_btree_value_t value1, value2, value3;
    char *value1_bin = NULL;
    char *value2_bin = NULL;
    char *value3_bin = NULL;
    size_t value1_bin_size = 0;
    size_t value2_bin_size = 0;
    size_t value3_bin_size = 0;

    view_btree_reduction_t reduction1;
    char reduction1_bin[512];
    size_t reduction1_bin_size = 0;
    view_btree_reduction_t reduction2;
    char reduction2_bin[512];
    size_t reduction2_bin_size = 0;

    nodelist *nl = NULL, *nl2 = NULL, *nl3 = NULL;
    node_pointer *np = NULL, *np2 = NULL;

    view_btree_reduction_t *red = NULL;
    char red_bin[512];
    size_t red_bin_size = 0;

    ctx = make_view_reducer_ctx(function_sources, 1, &error_msg);
    assert(ctx != NULL);

    key1.json_key.buf = "10";
    key1.json_key.size = sizeof("10") - 1;
    key1.doc_id.buf = "doc_10";
    key1.doc_id.size = sizeof("doc_10") - 1;
    assert(encode_view_btree_key(&key1, &key1_bin, &key1_bin_size) == COUCHSTORE_SUCCESS);

    key2.json_key.buf = "11";
    key2.json_key.size = sizeof("11") - 1;
    key2.doc_id.buf = "doc_11";
    key2.doc_id.size = sizeof("doc_11") - 1;
    assert(encode_view_btree_key(&key2, &key2_bin, &key2_bin_size) == COUCHSTORE_SUCCESS);

    key3.json_key.buf = "12";
    key3.json_key.size = sizeof("12") - 1;
    key3.doc_id.buf = "doc_12";
    key3.doc_id.size = sizeof("doc_12") - 1;
    assert(encode_view_btree_key(&key3, &key3_bin, &key3_bin_size) == COUCHSTORE_SUCCESS);

    value1.partition = 7;
    value1.num_values = 2;
    value1.values = (sized_buf *) malloc(sizeof(sized_buf) * 2);
    value1.values[0].buf = "100";
    value1.values[0].size = sizeof("100") - 1;
    value1.values[1].buf = "1";
    value1.values[1].size = sizeof("1") - 1;
    assert(encode_view_btree_value(&value1, &value1_bin, &value1_bin_size) == COUCHSTORE_SUCCESS);

    value2.partition = 666;
    value2.num_values = 1;
    value2.values = (sized_buf *) malloc(sizeof(sized_buf) * 1);
    value2.values[0].buf = "1000";
    value2.values[0].size = sizeof("1000") - 1;
    assert(encode_view_btree_value(&value2, &value2_bin, &value2_bin_size) == COUCHSTORE_SUCCESS);

    value3.partition = 1023;
    value3.num_values = 1;
    value3.values = (sized_buf *) malloc(sizeof(sized_buf) * 1);
    value3.values[0].buf = "5000.33";
    value3.values[0].size = sizeof("5000.33") - 1;
    assert(encode_view_btree_value(&value3, &value3_bin, &value3_bin_size) == COUCHSTORE_SUCCESS);

    nl = (nodelist *) malloc(sizeof(nodelist));
    assert(nl != NULL);
    nl->data.buf = value1_bin;
    nl->data.size = value1_bin_size;
    nl->key.buf = key1_bin;
    nl->key.size = key1_bin_size;
    nl->pointer = NULL;

    nl2 = (nodelist *) malloc(sizeof(nodelist));
    assert(nl2 != NULL);
    nl2->data.buf = value2_bin;
    nl2->data.size = value2_bin_size;
    nl2->key.buf = key2_bin;
    nl2->key.size = key2_bin_size;
    nl2->pointer = NULL;
    nl->next = nl2;

    nl3 = (nodelist *) malloc(sizeof(nodelist));
    assert(nl3 != NULL);
    nl3->data.buf = value3_bin;
    nl3->data.size = value3_bin_size;
    nl3->key.buf = key3_bin;
    nl3->key.size = key3_bin_size;
    nl3->pointer = NULL;
    nl3->next = NULL;
    nl2->next = nl3;

    assert(view_btree_reduce(red_bin, &red_bin_size, nl, 3, ctx) == COUCHSTORE_SUCCESS);
    assert(decode_view_btree_reduction(red_bin, red_bin_size, &red) == COUCHSTORE_SUCCESS);
    assert(red->kv_count == 4);
    assert(red->num_values == 1);
    assert(red->reduce_values[0].size == (sizeof("4") - 1));
    assert(strncmp(red->reduce_values[0].buf, "4", sizeof("4") - 1) == 0);

    for (i = 0; i < BITMAP_SIZE; ++i) {
        if ((i != 7) && (i != 666) && (i != 1023)) {
            assert(!is_bit_set(&red->partitions_bitmap, i));
        } else {
            assert(is_bit_set(&red->partitions_bitmap, i));
        }
    }
    free_view_btree_reduction(red);
    red = NULL;

    reduction1.kv_count = 11;
    memset(&reduction1.partitions_bitmap, 0, sizeof(reduction1.partitions_bitmap));
    set_bit(&reduction1.partitions_bitmap, 10);
    set_bit(&reduction1.partitions_bitmap, 1011);
    reduction1.num_values = 1;
    reduction1.reduce_values = (sized_buf *) malloc(sizeof(sized_buf) * 1);
    assert(reduction1.reduce_values != NULL);
    reduction1.reduce_values[0].buf = "4444";
    reduction1.reduce_values[0].size = sizeof("4444") - 1;
    assert(encode_view_btree_reduction(&reduction1, reduction1_bin, &reduction1_bin_size) == COUCHSTORE_SUCCESS);

    reduction2.kv_count = 44;
    memset(&reduction2.partitions_bitmap, 0, sizeof(reduction2.partitions_bitmap));
    set_bit(&reduction2.partitions_bitmap, 10);
    set_bit(&reduction2.partitions_bitmap, 777);
    set_bit(&reduction2.partitions_bitmap, 333);
    reduction2.num_values = 1;
    reduction2.reduce_values = (sized_buf *) malloc(sizeof(sized_buf) * 1);
    assert(reduction2.reduce_values != NULL);
    reduction2.reduce_values[0].buf = "100";
    reduction2.reduce_values[0].size = sizeof("100") - 1;
    assert(encode_view_btree_reduction(&reduction2, reduction2_bin, &reduction2_bin_size) == COUCHSTORE_SUCCESS);

    np = (node_pointer *) malloc(sizeof(node_pointer));
    assert(np != NULL);
    np->key.buf = key1_bin;
    np->key.size = key1_bin_size;
    np->reduce_value.buf = reduction1_bin;
    np->reduce_value.size = reduction1_bin_size;
    np->pointer = 0;
    np->subtreesize = 222;
    nl->pointer = np;

    np2 = (node_pointer *) malloc(sizeof(node_pointer));
    assert(np2 != NULL);
    np2->key.buf = key2_bin;
    np2->key.size = key2_bin_size;
    np2->reduce_value.buf = reduction2_bin;
    np2->reduce_value.size = reduction2_bin_size;
    np2->pointer = 0;
    np2->subtreesize = 333;
    nl2->pointer = np2;

    assert(view_btree_rereduce(red_bin, &red_bin_size, nl, 2, ctx) == COUCHSTORE_SUCCESS);
    assert(decode_view_btree_reduction(red_bin, red_bin_size, &red) == COUCHSTORE_SUCCESS);
    assert(red->kv_count == 55);
    assert(red->num_values == 1);
    assert(red->reduce_values[0].size == (sizeof("4544") - 1));
    assert(strncmp(red->reduce_values[0].buf, "4544", sizeof("4544") - 1) == 0);

    for (i = 0; i < BITMAP_SIZE; ++i) {
        if ((i != 10) && (i != 333) && (i != 777) && (i != 1011)) {
            assert(!is_bit_set(&red->partitions_bitmap, i));
        } else {
            assert(is_bit_set(&red->partitions_bitmap, i));
        }
    }

    free_view_reduction(&reduction1);
    free_view_reduction(&reduction2);
    free_view_btree_reduction(red);
    free_view_reducer_ctx(ctx);
    free(key1_bin);
    free(key2_bin);
    free(key3_bin);
    free_view_value(&value1);
    free_view_value(&value2);
    free_view_value(&value3);
    free(value1_bin);
    free(value2_bin);
    free(value3_bin);
    free_node_list(nl);
}

static void test_view_btree_stats_reducer(void)
{
    char *error_msg = NULL;
    view_reducer_ctx_t *ctx = NULL;
    const char *function_sources[] = { "_stats" };
    int i;
#ifdef _MSC_VER
    const char *expected_reduction =
        "{\"sum\":3101.5,\"count\":4,\"min\":1,\"max\":2000.5,\"sumsqr\":5.012e+006}";
#else
    const char *expected_reduction =
        "{\"sum\":3101.5,\"count\":4,\"min\":1,\"max\":2000.5,\"sumsqr\":5.012e+06}";
#endif
    const char *expected_rereduction =
        "{\"sum\":10203.1,\"count\":8,\"min\":1,\"max\":2000.5,\"sumsqr\":15}";

    view_btree_key_t key1, key2, key3;
    char *key1_bin = NULL;
    char *key2_bin = NULL;
    char *key3_bin = NULL;
    size_t key1_bin_size = 0;
    size_t key2_bin_size = 0;
    size_t key3_bin_size = 0;

    view_btree_value_t value1, value2, value3;
    char *value1_bin = NULL;
    char *value2_bin = NULL;
    char *value3_bin = NULL;
    size_t value1_bin_size = 0;
    size_t value2_bin_size = 0;
    size_t value3_bin_size = 0;

    view_btree_reduction_t reduction1;
    char reduction1_bin[512];
    size_t reduction1_bin_size = 0;
    view_btree_reduction_t reduction2;
    char reduction2_bin[512];
    size_t reduction2_bin_size = 0;

    nodelist *nl = NULL, *nl2 = NULL, *nl3 = NULL;
    node_pointer *np = NULL, *np2 = NULL;

    view_btree_reduction_t *red = NULL;
    char red_bin[512];
    size_t red_bin_size = 0;

    ctx = make_view_reducer_ctx(function_sources, 1, &error_msg);
    assert(ctx != NULL);

    key1.json_key.buf = "10";
    key1.json_key.size = sizeof("10") - 1;
    key1.doc_id.buf = "doc_10";
    key1.doc_id.size = sizeof("doc_10") - 1;
    assert(encode_view_btree_key(&key1, &key1_bin, &key1_bin_size) == COUCHSTORE_SUCCESS);

    key2.json_key.buf = "11";
    key2.json_key.size = sizeof("11") - 1;
    key2.doc_id.buf = "doc_11";
    key2.doc_id.size = sizeof("doc_11") - 1;
    assert(encode_view_btree_key(&key2, &key2_bin, &key2_bin_size) == COUCHSTORE_SUCCESS);

    key3.json_key.buf = "12";
    key3.json_key.size = sizeof("12") - 1;
    key3.doc_id.buf = "doc_12";
    key3.doc_id.size = sizeof("doc_12") - 1;
    assert(encode_view_btree_key(&key3, &key3_bin, &key3_bin_size) == COUCHSTORE_SUCCESS);

    value1.partition = 7;
    value1.num_values = 2;
    value1.values = (sized_buf *) malloc(sizeof(sized_buf) * 2);
    value1.values[0].buf = "100";
    value1.values[0].size = sizeof("100") - 1;
    value1.values[1].buf = "1";
    value1.values[1].size = sizeof("1") - 1;
    assert(encode_view_btree_value(&value1, &value1_bin, &value1_bin_size) == COUCHSTORE_SUCCESS);

    value2.partition = 666;
    value2.num_values = 1;
    value2.values = (sized_buf *) malloc(sizeof(sized_buf) * 1);
    value2.values[0].buf = "1000";
    value2.values[0].size = sizeof("1000") - 1;
    assert(encode_view_btree_value(&value2, &value2_bin, &value2_bin_size) == COUCHSTORE_SUCCESS);

    value3.partition = 1023;
    value3.num_values = 1;
    value3.values = (sized_buf *) malloc(sizeof(sized_buf) * 1);
    value3.values[0].buf = "2000.50";
    value3.values[0].size = sizeof("2000.50") - 1;
    assert(encode_view_btree_value(&value3, &value3_bin, &value3_bin_size) == COUCHSTORE_SUCCESS);

    nl = (nodelist *) malloc(sizeof(nodelist));
    assert(nl != NULL);
    nl->data.buf = value1_bin;
    nl->data.size = value1_bin_size;
    nl->key.buf = key1_bin;
    nl->key.size = key1_bin_size;
    nl->pointer = NULL;

    nl2 = (nodelist *) malloc(sizeof(nodelist));
    assert(nl2 != NULL);
    nl2->data.buf = value2_bin;
    nl2->data.size = value2_bin_size;
    nl2->key.buf = key2_bin;
    nl2->key.size = key2_bin_size;
    nl2->pointer = NULL;
    nl->next = nl2;

    nl3 = (nodelist *) malloc(sizeof(nodelist));
    assert(nl3 != NULL);
    nl3->data.buf = value3_bin;
    nl3->data.size = value3_bin_size;
    nl3->key.buf = key3_bin;
    nl3->key.size = key3_bin_size;
    nl3->pointer = NULL;
    nl3->next = NULL;
    nl2->next = nl3;

    assert(view_btree_reduce(red_bin, &red_bin_size, nl, 3, ctx) == COUCHSTORE_SUCCESS);
    assert(decode_view_btree_reduction(red_bin, red_bin_size, &red) == COUCHSTORE_SUCCESS);
    assert(red->kv_count == 4);
    assert(red->num_values == 1);
    assert(red->reduce_values[0].size == strlen(expected_reduction));
    assert(strncmp(red->reduce_values[0].buf,
                   expected_reduction, strlen(expected_reduction)) == 0);

    for (i = 0; i < BITMAP_SIZE; ++i) {
        if ((i != 7) && (i != 666) && (i != 1023)) {
            assert(!is_bit_set(&red->partitions_bitmap, i));
        } else {
            assert(is_bit_set(&red->partitions_bitmap, i));
        }
    }
    free_view_btree_reduction(red);
    red = NULL;

    /* Test _stats reduce error */
    value3.values[0].buf = "\"foobar\"";
    value3.values[0].size = sizeof("\"foobar\"") - 1;
    free(value3_bin);
    assert(encode_view_btree_value(&value3, &value3_bin, &value3_bin_size) == COUCHSTORE_SUCCESS);

    nl3->data.buf = value3_bin;
    nl3->data.size = value3_bin_size;

    assert(view_btree_reduce(red_bin, &red_bin_size, nl, 3, ctx) == COUCHSTORE_ERROR_REDUCER_FAILURE);
    assert(ctx->error != NULL);
    assert(strcmp(ctx->error, "Value is not a number (key 12)") == 0);

    /* Test successful rereduce */

    reduction1.kv_count = 11;
    memset(&reduction1.partitions_bitmap, 0, sizeof(reduction1.partitions_bitmap));
    set_bit(&reduction1.partitions_bitmap, 10);
    set_bit(&reduction1.partitions_bitmap, 1011);
    reduction1.num_values = 1;
    reduction1.reduce_values = (sized_buf *) malloc(sizeof(sized_buf) * 1);
    assert(reduction1.reduce_values != NULL);
    reduction1.reduce_values[0].buf = "{\"sum\":3101.5,\"count\":4,\"min\":1,\"max\":2000.5,\"sumsqr\":5}";
    reduction1.reduce_values[0].size = strlen(reduction1.reduce_values[0].buf);
    assert(encode_view_btree_reduction(&reduction1, reduction1_bin, &reduction1_bin_size) == COUCHSTORE_SUCCESS);

    reduction2.kv_count = 44;
    memset(&reduction2.partitions_bitmap, 0, sizeof(reduction2.partitions_bitmap));
    set_bit(&reduction2.partitions_bitmap, 10);
    set_bit(&reduction2.partitions_bitmap, 777);
    set_bit(&reduction2.partitions_bitmap, 333);
    reduction2.num_values = 1;
    reduction2.reduce_values = (sized_buf *) malloc(sizeof(sized_buf) * 1);
    assert(reduction2.reduce_values != NULL);
    reduction2.reduce_values[0].buf = "{\"sum\":7101.6,\"count\":4,\"min\":3,\"max\":1000.5,\"sumsqr\":10}";
    reduction2.reduce_values[0].size = strlen(reduction2.reduce_values[0].buf);
    assert(encode_view_btree_reduction(&reduction2, reduction2_bin, &reduction2_bin_size) == COUCHSTORE_SUCCESS);

    np = (node_pointer *) malloc(sizeof(node_pointer));
    assert(np != NULL);
    np->key.buf = key1_bin;
    np->key.size = key1_bin_size;
    np->reduce_value.buf = reduction1_bin;
    np->reduce_value.size = reduction1_bin_size;
    np->pointer = 0;
    np->subtreesize = 222;
    nl->pointer = np;

    np2 = (node_pointer *) malloc(sizeof(node_pointer));
    assert(np2 != NULL);
    np2->key.buf = key2_bin;
    np2->key.size = key2_bin_size;
    np2->reduce_value.buf = reduction2_bin;
    np2->reduce_value.size = reduction2_bin_size;
    np2->pointer = 0;
    np2->subtreesize = 333;
    nl2->pointer = np2;

    assert(view_btree_rereduce(red_bin, &red_bin_size, nl, 2, ctx) == COUCHSTORE_SUCCESS);
    assert(decode_view_btree_reduction(red_bin, red_bin_size, &red) == COUCHSTORE_SUCCESS);
    assert(red->kv_count == 55);
    assert(red->num_values == 1);
    assert(red->reduce_values[0].size == strlen(expected_rereduction));
    assert(strncmp(red->reduce_values[0].buf, expected_rereduction, red->reduce_values[0].size) == 0);

    for (i = 0; i < BITMAP_SIZE; ++i) {
        if ((i != 10) && (i != 333) && (i != 777) && (i != 1011)) {
            assert(!is_bit_set(&red->partitions_bitmap, i));
        } else {
            assert(is_bit_set(&red->partitions_bitmap, i));
        }
    }

    free_view_reduction(&reduction1);
    free_view_reduction(&reduction2);
    free_view_btree_reduction(red);
    free_view_reducer_ctx(ctx);
    free(key1_bin);
    free(key2_bin);
    free(key3_bin);
    free_view_value(&value1);
    free_view_value(&value2);
    free_view_value(&value3);
    free(value1_bin);
    free(value2_bin);
    free(value3_bin);
    free_node_list(nl);
}

static void test_view_btree_js_reducer(void)
{
    char *error_msg = NULL;
    view_reducer_ctx_t *ctx = NULL;
    const char *function_sources[] = {
        "function(key, values, rereduce) {"
        "  if (values[3] == 'foobar') throw('foobar');"
        "  if (rereduce) return sum(values);"
        "  return values.length;"
        "}"
    };
    int i;

    view_btree_key_t key1, key2, key3;
    char *key1_bin = NULL;
    char *key2_bin = NULL;
    char *key3_bin = NULL;
    size_t key1_bin_size = 0;
    size_t key2_bin_size = 0;
    size_t key3_bin_size = 0;

    view_btree_value_t value1, value2, value3;
    char *value1_bin = NULL;
    char *value2_bin = NULL;
    char *value3_bin = NULL;
    size_t value1_bin_size = 0;
    size_t value2_bin_size = 0;
    size_t value3_bin_size = 0;

    view_btree_reduction_t reduction1;
    char reduction1_bin[512];
    size_t reduction1_bin_size = 0;
    view_btree_reduction_t reduction2;
    char reduction2_bin[512];
    size_t reduction2_bin_size = 0;

    nodelist *nl = NULL, *nl2 = NULL, *nl3 = NULL;
    node_pointer *np = NULL, *np2 = NULL;

    view_btree_reduction_t *red = NULL;
    char red_bin[512];
    size_t red_bin_size = 0;

    ctx = make_view_reducer_ctx(function_sources, 1, &error_msg);
    assert(ctx != NULL);

    key1.json_key.buf = "10";
    key1.json_key.size = sizeof("10") - 1;
    key1.doc_id.buf = "doc_10";
    key1.doc_id.size = sizeof("doc_10") - 1;
    assert(encode_view_btree_key(&key1, &key1_bin, &key1_bin_size) == COUCHSTORE_SUCCESS);

    key2.json_key.buf = "11";
    key2.json_key.size = sizeof("11") - 1;
    key2.doc_id.buf = "doc_11";
    key2.doc_id.size = sizeof("doc_11") - 1;
    assert(encode_view_btree_key(&key2, &key2_bin, &key2_bin_size) == COUCHSTORE_SUCCESS);

    key3.json_key.buf = "12";
    key3.json_key.size = sizeof("12") - 1;
    key3.doc_id.buf = "doc_12";
    key3.doc_id.size = sizeof("doc_12") - 1;
    assert(encode_view_btree_key(&key3, &key3_bin, &key3_bin_size) == COUCHSTORE_SUCCESS);

    value1.partition = 7;
    value1.num_values = 2;
    value1.values = (sized_buf *) malloc(sizeof(sized_buf) * 2);
    value1.values[0].buf = "100";
    value1.values[0].size = sizeof("100") - 1;
    value1.values[1].buf = "1";
    value1.values[1].size = sizeof("1") - 1;
    assert(encode_view_btree_value(&value1, &value1_bin, &value1_bin_size) == COUCHSTORE_SUCCESS);

    value2.partition = 666;
    value2.num_values = 1;
    value2.values = (sized_buf *) malloc(sizeof(sized_buf) * 1);
    value2.values[0].buf = "1000";
    value2.values[0].size = sizeof("1000") - 1;
    assert(encode_view_btree_value(&value2, &value2_bin, &value2_bin_size) == COUCHSTORE_SUCCESS);

    value3.partition = 1023;
    value3.num_values = 1;
    value3.values = (sized_buf *) malloc(sizeof(sized_buf) * 1);
    value3.values[0].buf = "5000.33";
    value3.values[0].size = sizeof("5000.33") - 1;
    assert(encode_view_btree_value(&value3, &value3_bin, &value3_bin_size) == COUCHSTORE_SUCCESS);

    nl = (nodelist *) malloc(sizeof(nodelist));
    assert(nl != NULL);
    nl->data.buf = value1_bin;
    nl->data.size = value1_bin_size;
    nl->key.buf = key1_bin;
    nl->key.size = key1_bin_size;
    nl->pointer = NULL;

    nl2 = (nodelist *) malloc(sizeof(nodelist));
    assert(nl2 != NULL);
    nl2->data.buf = value2_bin;
    nl2->data.size = value2_bin_size;
    nl2->key.buf = key2_bin;
    nl2->key.size = key2_bin_size;
    nl2->pointer = NULL;
    nl->next = nl2;

    nl3 = (nodelist *) malloc(sizeof(nodelist));
    assert(nl3 != NULL);
    nl3->data.buf = value3_bin;
    nl3->data.size = value3_bin_size;
    nl3->key.buf = key3_bin;
    nl3->key.size = key3_bin_size;
    nl3->pointer = NULL;
    nl3->next = NULL;
    nl2->next = nl3;

    assert(view_btree_reduce(red_bin, &red_bin_size, nl, 3, ctx) == COUCHSTORE_SUCCESS);
    assert(decode_view_btree_reduction(red_bin, red_bin_size, &red) == COUCHSTORE_SUCCESS);
    assert(red->kv_count == 4);
    assert(red->num_values == 1);
    assert(red->reduce_values[0].size == (sizeof("4") - 1));
    assert(strncmp(red->reduce_values[0].buf, "4", sizeof("4") - 1) == 0);

    for (i = 0; i < BITMAP_SIZE; ++i) {
        if ((i != 7) && (i != 666) && (i != 1023)) {
            assert(!is_bit_set(&red->partitions_bitmap, i));
        } else {
            assert(is_bit_set(&red->partitions_bitmap, i));
        }
    }
    free_view_btree_reduction(red);
    red = NULL;

    /* Test JS reduce error */

    free(value3_bin);
    value3.values[0].buf = "\"foobar\"";
    value3.values[0].size = sizeof("\"foobar\"") - 1;
    assert(encode_view_btree_value(&value3, &value3_bin, &value3_bin_size) == COUCHSTORE_SUCCESS);

    nl3->data.buf = value3_bin;
    nl3->data.size = value3_bin_size;

    assert(view_btree_reduce(red_bin, &red_bin_size, nl, 3, ctx) == COUCHSTORE_ERROR_REDUCER_FAILURE);
    assert(ctx->error != NULL);
    assert(strcmp(ctx->error, "foobar") == 0);

    /* Test JS rereduce */

    reduction1.kv_count = 11;
    memset(&reduction1.partitions_bitmap, 0, sizeof(reduction1.partitions_bitmap));
    set_bit(&reduction1.partitions_bitmap, 10);
    set_bit(&reduction1.partitions_bitmap, 1011);
    reduction1.num_values = 1;
    reduction1.reduce_values = (sized_buf *) malloc(sizeof(sized_buf) * 1);
    assert(reduction1.reduce_values != NULL);
    reduction1.reduce_values[0].buf = "4444";
    reduction1.reduce_values[0].size = sizeof("4444") - 1;
    assert(encode_view_btree_reduction(&reduction1, reduction1_bin, &reduction1_bin_size) == COUCHSTORE_SUCCESS);

    reduction2.kv_count = 44;
    memset(&reduction2.partitions_bitmap, 0, sizeof(reduction2.partitions_bitmap));
    set_bit(&reduction2.partitions_bitmap, 10);
    set_bit(&reduction2.partitions_bitmap, 777);
    set_bit(&reduction2.partitions_bitmap, 333);
    reduction2.num_values = 1;
    reduction2.reduce_values = (sized_buf *) malloc(sizeof(sized_buf) * 1);
    assert(reduction2.reduce_values != NULL);
    reduction2.reduce_values[0].buf = "100";
    reduction2.reduce_values[0].size = sizeof("100") - 1;
    assert(encode_view_btree_reduction(&reduction2, reduction2_bin, &reduction2_bin_size) == COUCHSTORE_SUCCESS);

    np = (node_pointer *) malloc(sizeof(node_pointer));
    assert(np != NULL);
    np->key.buf = key1_bin;
    np->key.size = key1_bin_size;
    np->reduce_value.buf = reduction1_bin;
    np->reduce_value.size = reduction1_bin_size;
    np->pointer = 0;
    np->subtreesize = 222;
    nl->pointer = np;

    np2 = (node_pointer *) malloc(sizeof(node_pointer));
    assert(np2 != NULL);
    np2->key.buf = key2_bin;
    np2->key.size = key2_bin_size;
    np2->reduce_value.buf = reduction2_bin;
    np2->reduce_value.size = reduction2_bin_size;
    np2->pointer = 0;
    np2->subtreesize = 333;
    nl2->pointer = np2;

    assert(view_btree_rereduce(red_bin, &red_bin_size, nl, 2, ctx) == COUCHSTORE_SUCCESS);
    assert(decode_view_btree_reduction(red_bin, red_bin_size, &red) == COUCHSTORE_SUCCESS);
    assert(red->kv_count == 55);
    assert(red->num_values == 1);
    assert(red->reduce_values[0].size == (sizeof("4544") - 1));
    assert(strncmp(red->reduce_values[0].buf, "4544", sizeof("4544") - 1) == 0);

    for (i = 0; i < BITMAP_SIZE; ++i) {
        if ((i != 10) && (i != 333) && (i != 777) && (i != 1011)) {
            assert(!is_bit_set(&red->partitions_bitmap, i));
        } else {
            assert(is_bit_set(&red->partitions_bitmap, i));
        }
    }

    free_view_reduction(&reduction1);
    free_view_reduction(&reduction2);
    free_view_btree_reduction(red);
    free_view_reducer_ctx(ctx);
    free(key1_bin);
    free(key2_bin);
    free(key3_bin);
    free_view_value(&value1);
    free_view_value(&value2);
    free_view_value(&value3);
    free(value1_bin);
    free(value2_bin);
    free(value3_bin);
    free_node_list(nl);
}

static void test_view_btree_multiple_reducers(void)
{
    char *error_msg = NULL;
    view_reducer_ctx_t *ctx = NULL;
    const char *function_sources[] = {
        "_count",
        "function(key, values, rereduce) {"
        "  if (values[3] == 'foobar') throw('foobar');"
        "  if (rereduce) return sum(values);"
        "  return values.length;"
        "}",
        "_sum"
    };
    int i;

    view_btree_key_t key1, key2, key3;
    char *key1_bin = NULL;
    char *key2_bin = NULL;
    char *key3_bin = NULL;
    size_t key1_bin_size = 0;
    size_t key2_bin_size = 0;
    size_t key3_bin_size = 0;

    view_btree_value_t value1, value2, value3;
    char *value1_bin = NULL;
    char *value2_bin = NULL;
    char *value3_bin = NULL;
    size_t value1_bin_size = 0;
    size_t value2_bin_size = 0;
    size_t value3_bin_size = 0;

    view_btree_reduction_t reduction1;
    char reduction1_bin[512];
    size_t reduction1_bin_size = 0;
    view_btree_reduction_t reduction2;
    char reduction2_bin[512];
    size_t reduction2_bin_size = 0;

    nodelist *nl = NULL, *nl2 = NULL, *nl3 = NULL;
    node_pointer *np = NULL, *np2 = NULL;

    view_btree_reduction_t *red = NULL;
    char red_bin[512];
    size_t red_bin_size = 0;

    ctx = make_view_reducer_ctx(function_sources, 3, &error_msg);
    assert(ctx != NULL);

    key1.json_key.buf = "10";
    key1.json_key.size = sizeof("10") - 1;
    key1.doc_id.buf = "doc_10";
    key1.doc_id.size = sizeof("doc_10") - 1;
    assert(encode_view_btree_key(&key1, &key1_bin, &key1_bin_size) == COUCHSTORE_SUCCESS);

    key2.json_key.buf = "11";
    key2.json_key.size = sizeof("11") - 1;
    key2.doc_id.buf = "doc_11";
    key2.doc_id.size = sizeof("doc_11") - 1;
    assert(encode_view_btree_key(&key2, &key2_bin, &key2_bin_size) == COUCHSTORE_SUCCESS);

    key3.json_key.buf = "12";
    key3.json_key.size = sizeof("12") - 1;
    key3.doc_id.buf = "doc_12";
    key3.doc_id.size = sizeof("doc_12") - 1;
    assert(encode_view_btree_key(&key3, &key3_bin, &key3_bin_size) == COUCHSTORE_SUCCESS);

    value1.partition = 7;
    value1.num_values = 2;
    value1.values = (sized_buf *) malloc(sizeof(sized_buf) * 2);
    value1.values[0].buf = "100";
    value1.values[0].size = sizeof("100") - 1;
    value1.values[1].buf = "1";
    value1.values[1].size = sizeof("1") - 1;
    assert(encode_view_btree_value(&value1, &value1_bin, &value1_bin_size) == COUCHSTORE_SUCCESS);

    value2.partition = 666;
    value2.num_values = 1;
    value2.values = (sized_buf *) malloc(sizeof(sized_buf) * 1);
    value2.values[0].buf = "1000";
    value2.values[0].size = sizeof("1000") - 1;
    assert(encode_view_btree_value(&value2, &value2_bin, &value2_bin_size) == COUCHSTORE_SUCCESS);

    value3.partition = 1023;
    value3.num_values = 1;
    value3.values = (sized_buf *) malloc(sizeof(sized_buf) * 1);
    value3.values[0].buf = "5000.33";
    value3.values[0].size = sizeof("5000.33") - 1;
    assert(encode_view_btree_value(&value3, &value3_bin, &value3_bin_size) == COUCHSTORE_SUCCESS);

    nl = (nodelist *) malloc(sizeof(nodelist));
    assert(nl != NULL);
    nl->data.buf = value1_bin;
    nl->data.size = value1_bin_size;
    nl->key.buf = key1_bin;
    nl->key.size = key1_bin_size;
    nl->pointer = NULL;

    nl2 = (nodelist *) malloc(sizeof(nodelist));
    assert(nl2 != NULL);
    nl2->data.buf = value2_bin;
    nl2->data.size = value2_bin_size;
    nl2->key.buf = key2_bin;
    nl2->key.size = key2_bin_size;
    nl2->pointer = NULL;
    nl->next = nl2;

    nl3 = (nodelist *) malloc(sizeof(nodelist));
    assert(nl3 != NULL);
    nl3->data.buf = value3_bin;
    nl3->data.size = value3_bin_size;
    nl3->key.buf = key3_bin;
    nl3->key.size = key3_bin_size;
    nl3->pointer = NULL;
    nl3->next = NULL;
    nl2->next = nl3;

    assert(view_btree_reduce(red_bin, &red_bin_size, nl, 3, ctx) == COUCHSTORE_SUCCESS);
    assert(decode_view_btree_reduction(red_bin, red_bin_size, &red) == COUCHSTORE_SUCCESS);
    assert(red->kv_count == 4);
    assert(red->num_values == 3);
    assert(red->reduce_values[0].size == (sizeof("4") - 1));
    assert(strncmp(red->reduce_values[0].buf, "4", sizeof("4") - 1) == 0);
    assert(red->reduce_values[1].size == (sizeof("4") - 1));
    assert(strncmp(red->reduce_values[1].buf, "4", sizeof("4") - 1) == 0);
    assert(red->reduce_values[2].size == (sizeof("6101.33") - 1));
    assert(strncmp(red->reduce_values[2].buf, "6101.33", sizeof("6101.33") - 1) == 0);

    for (i = 0; i < BITMAP_SIZE; ++i) {
        if ((i != 7) && (i != 666) && (i != 1023)) {
            assert(!is_bit_set(&red->partitions_bitmap, i));
        } else {
            assert(is_bit_set(&red->partitions_bitmap, i));
        }
    }
    free_view_btree_reduction(red);
    red = NULL;

    /* Test JS reduce error */

    free(value3_bin);
    value3.values[0].buf = "\"foobar\"";
    value3.values[0].size = sizeof("\"foobar\"") - 1;
    assert(encode_view_btree_value(&value3, &value3_bin, &value3_bin_size) == COUCHSTORE_SUCCESS);

    nl3->data.buf = value3_bin;
    nl3->data.size = value3_bin_size;

    assert(view_btree_reduce(red_bin, &red_bin_size, nl, 3, ctx) == COUCHSTORE_ERROR_REDUCER_FAILURE);
    assert(ctx->error != NULL);
    assert(strcmp(ctx->error, "foobar") == 0);

    /* Test JS rereduce */

    reduction1.kv_count = 11;
    memset(&reduction1.partitions_bitmap, 0, sizeof(reduction1.partitions_bitmap));
    set_bit(&reduction1.partitions_bitmap, 10);
    set_bit(&reduction1.partitions_bitmap, 1011);
    reduction1.num_values = 3;
    reduction1.reduce_values = (sized_buf *) malloc(sizeof(sized_buf) * 3);
    assert(reduction1.reduce_values != NULL);
    reduction1.reduce_values[0].buf = "4444";
    reduction1.reduce_values[0].size = sizeof("4444") - 1;
    reduction1.reduce_values[1].buf = "44";
    reduction1.reduce_values[1].size = sizeof("44") - 1;
    reduction1.reduce_values[2].buf = "4000";
    reduction1.reduce_values[2].size = sizeof("4000") - 1;
    assert(encode_view_btree_reduction(&reduction1, reduction1_bin, &reduction1_bin_size) == COUCHSTORE_SUCCESS);

    reduction2.kv_count = 44;
    memset(&reduction2.partitions_bitmap, 0, sizeof(reduction2.partitions_bitmap));
    set_bit(&reduction2.partitions_bitmap, 10);
    set_bit(&reduction2.partitions_bitmap, 777);
    set_bit(&reduction2.partitions_bitmap, 333);
    reduction2.num_values = 3;
    reduction2.reduce_values = (sized_buf *) malloc(sizeof(sized_buf) * 3);
    assert(reduction2.reduce_values != NULL);
    reduction2.reduce_values[0].buf = "100";
    reduction2.reduce_values[0].size = sizeof("100") - 1;
    reduction2.reduce_values[1].buf = "100";
    reduction2.reduce_values[1].size = sizeof("100") - 1;
    reduction2.reduce_values[2].buf = "100";
    reduction2.reduce_values[2].size = sizeof("100") - 1;
    assert(encode_view_btree_reduction(&reduction2, reduction2_bin, &reduction2_bin_size) == COUCHSTORE_SUCCESS);

    np = (node_pointer *) malloc(sizeof(node_pointer));
    assert(np != NULL);
    np->key.buf = key1_bin;
    np->key.size = key1_bin_size;
    np->reduce_value.buf = reduction1_bin;
    np->reduce_value.size = reduction1_bin_size;
    np->pointer = 0;
    np->subtreesize = 222;
    nl->pointer = np;

    np2 = (node_pointer *) malloc(sizeof(node_pointer));
    assert(np2 != NULL);
    np2->key.buf = key2_bin;
    np2->key.size = key2_bin_size;
    np2->reduce_value.buf = reduction2_bin;
    np2->reduce_value.size = reduction2_bin_size;
    np2->pointer = 0;
    np2->subtreesize = 333;
    nl2->pointer = np2;

    assert(view_btree_rereduce(red_bin, &red_bin_size, nl, 2, ctx) == COUCHSTORE_SUCCESS);
    assert(decode_view_btree_reduction(red_bin, red_bin_size, &red) == COUCHSTORE_SUCCESS);
    assert(red->kv_count == 55);
    assert(red->num_values == 3);
    assert(red->reduce_values[0].size == (sizeof("4544") - 1));
    assert(strncmp(red->reduce_values[0].buf, "4544", sizeof("4544") - 1) == 0);
    assert(red->reduce_values[1].size == (sizeof("144") - 1));
    assert(strncmp(red->reduce_values[1].buf, "144", sizeof("144") - 1) == 0);
    assert(red->reduce_values[2].size == (sizeof("4100") - 1));
    assert(strncmp(red->reduce_values[2].buf, "4100", sizeof("4100") - 1) == 0);

    for (i = 0; i < BITMAP_SIZE; ++i) {
        if ((i != 10) && (i != 333) && (i != 777) && (i != 1011)) {
            assert(!is_bit_set(&red->partitions_bitmap, i));
        } else {
            assert(is_bit_set(&red->partitions_bitmap, i));
        }
    }

    free_view_reduction(&reduction1);
    free_view_reduction(&reduction2);
    free_view_btree_reduction(red);
    free_view_reducer_ctx(ctx);
    free(key1_bin);
    free(key2_bin);
    free(key3_bin);
    free_view_value(&value1);
    free_view_value(&value2);
    free_view_value(&value3);
    free(value1_bin);
    free(value2_bin);
    free(value3_bin);
    free_node_list(nl);
}

static void test_view_btree_no_reducers(void)
{
    char *error_msg = NULL;
    view_reducer_ctx_t *ctx = NULL;
    int i;

    view_btree_key_t key1, key2, key3;
    char *key1_bin = NULL;
    char *key2_bin = NULL;
    char *key3_bin = NULL;
    size_t key1_bin_size = 0;
    size_t key2_bin_size = 0;
    size_t key3_bin_size = 0;

    view_btree_value_t value1, value2, value3;
    char *value1_bin = NULL;
    char *value2_bin = NULL;
    char *value3_bin = NULL;
    size_t value1_bin_size = 0;
    size_t value2_bin_size = 0;
    size_t value3_bin_size = 0;

    view_btree_reduction_t reduction1;
    char reduction1_bin[512];
    size_t reduction1_bin_size = 0;
    view_btree_reduction_t reduction2;
    char reduction2_bin[512];
    size_t reduction2_bin_size = 0;

    nodelist *nl = NULL, *nl2 = NULL, *nl3 = NULL;
    node_pointer *np = NULL, *np2 = NULL;

    view_btree_reduction_t *red = NULL;
    char red_bin[512];
    size_t red_bin_size = 0;

    ctx = make_view_reducer_ctx(NULL, 0, &error_msg);
    assert(ctx != NULL);

    key1.json_key.buf = "10";
    key1.json_key.size = sizeof("10") - 1;
    key1.doc_id.buf = "doc_10";
    key1.doc_id.size = sizeof("doc_10") - 1;
    assert(encode_view_btree_key(&key1, &key1_bin, &key1_bin_size) == COUCHSTORE_SUCCESS);

    key2.json_key.buf = "11";
    key2.json_key.size = sizeof("11") - 1;
    key2.doc_id.buf = "doc_11";
    key2.doc_id.size = sizeof("doc_11") - 1;
    assert(encode_view_btree_key(&key2, &key2_bin, &key2_bin_size) == COUCHSTORE_SUCCESS);

    key3.json_key.buf = "12";
    key3.json_key.size = sizeof("12") - 1;
    key3.doc_id.buf = "doc_12";
    key3.doc_id.size = sizeof("doc_12") - 1;
    assert(encode_view_btree_key(&key3, &key3_bin, &key3_bin_size) == COUCHSTORE_SUCCESS);

    value1.partition = 7;
    value1.num_values = 2;
    value1.values = (sized_buf *) malloc(sizeof(sized_buf) * 2);
    value1.values[0].buf = "100";
    value1.values[0].size = sizeof("100") - 1;
    value1.values[1].buf = "1";
    value1.values[1].size = sizeof("1") - 1;
    assert(encode_view_btree_value(&value1, &value1_bin, &value1_bin_size) == COUCHSTORE_SUCCESS);

    value2.partition = 666;
    value2.num_values = 1;
    value2.values = (sized_buf *) malloc(sizeof(sized_buf) * 1);
    value2.values[0].buf = "1000";
    value2.values[0].size = sizeof("1000") - 1;
    assert(encode_view_btree_value(&value2, &value2_bin, &value2_bin_size) == COUCHSTORE_SUCCESS);

    value3.partition = 1023;
    value3.num_values = 1;
    value3.values = (sized_buf *) malloc(sizeof(sized_buf) * 1);
    value3.values[0].buf = "5000.33";
    value3.values[0].size = sizeof("5000.33") - 1;
    assert(encode_view_btree_value(&value3, &value3_bin, &value3_bin_size) == COUCHSTORE_SUCCESS);

    nl = (nodelist *) malloc(sizeof(nodelist));
    assert(nl != NULL);
    nl->data.buf = value1_bin;
    nl->data.size = value1_bin_size;
    nl->key.buf = key1_bin;
    nl->key.size = key1_bin_size;
    nl->pointer = NULL;

    nl2 = (nodelist *) malloc(sizeof(nodelist));
    assert(nl2 != NULL);
    nl2->data.buf = value2_bin;
    nl2->data.size = value2_bin_size;
    nl2->key.buf = key2_bin;
    nl2->key.size = key2_bin_size;
    nl2->pointer = NULL;
    nl->next = nl2;

    nl3 = (nodelist *) malloc(sizeof(nodelist));
    assert(nl3 != NULL);
    nl3->data.buf = value3_bin;
    nl3->data.size = value3_bin_size;
    nl3->key.buf = key3_bin;
    nl3->key.size = key3_bin_size;
    nl3->pointer = NULL;
    nl3->next = NULL;
    nl2->next = nl3;

    assert(view_btree_reduce(red_bin, &red_bin_size, nl, 3, ctx) == COUCHSTORE_SUCCESS);
    assert(decode_view_btree_reduction(red_bin, red_bin_size, &red) == COUCHSTORE_SUCCESS);
    assert(red->kv_count == 4);
    assert(red->num_values == 0);
    assert(red->reduce_values == NULL);

    for (i = 0; i < BITMAP_SIZE; ++i) {
        if ((i != 7) && (i != 666) && (i != 1023)) {
            assert(!is_bit_set(&red->partitions_bitmap, i));
        } else {
            assert(is_bit_set(&red->partitions_bitmap, i));
        }
    }
    free_view_btree_reduction(red);
    red = NULL;

    /* Test rereduce */

    reduction1.kv_count = 11;
    memset(&reduction1.partitions_bitmap, 0, sizeof(reduction1.partitions_bitmap));
    set_bit(&reduction1.partitions_bitmap, 10);
    set_bit(&reduction1.partitions_bitmap, 1011);
    reduction1.num_values = 0;
    reduction1.reduce_values = NULL;
    assert(encode_view_btree_reduction(&reduction1, reduction1_bin, &reduction1_bin_size) == COUCHSTORE_SUCCESS);

    reduction2.kv_count = 44;
    memset(&reduction2.partitions_bitmap, 0, sizeof(reduction2.partitions_bitmap));
    set_bit(&reduction2.partitions_bitmap, 10);
    set_bit(&reduction2.partitions_bitmap, 777);
    set_bit(&reduction2.partitions_bitmap, 333);
    reduction2.num_values = 0;
    reduction2.reduce_values = NULL;
    assert(encode_view_btree_reduction(&reduction2, reduction2_bin, &reduction2_bin_size) == COUCHSTORE_SUCCESS);

    np = (node_pointer *) malloc(sizeof(node_pointer));
    assert(np != NULL);
    np->key.buf = key1_bin;
    np->key.size = key1_bin_size;
    np->reduce_value.buf = reduction1_bin;
    np->reduce_value.size = reduction1_bin_size;
    np->pointer = 0;
    np->subtreesize = 222;
    nl->pointer = np;

    np2 = (node_pointer *) malloc(sizeof(node_pointer));
    assert(np2 != NULL);
    np2->key.buf = key2_bin;
    np2->key.size = key2_bin_size;
    np2->reduce_value.buf = reduction2_bin;
    np2->reduce_value.size = reduction2_bin_size;
    np2->pointer = 0;
    np2->subtreesize = 333;
    nl2->pointer = np2;

    assert(view_btree_rereduce(red_bin, &red_bin_size, nl, 2, ctx) == COUCHSTORE_SUCCESS);
    assert(decode_view_btree_reduction(red_bin, red_bin_size, &red) == COUCHSTORE_SUCCESS);
    assert(red->kv_count == 55);
    assert(red->num_values == 0);
    assert(red->reduce_values == NULL);

    for (i = 0; i < BITMAP_SIZE; ++i) {
        if ((i != 10) && (i != 333) && (i != 777) && (i != 1011)) {
            assert(!is_bit_set(&red->partitions_bitmap, i));
        } else {
            assert(is_bit_set(&red->partitions_bitmap, i));
        }
    }

    free_view_reduction(&reduction1);
    free_view_reduction(&reduction2);
    free_view_btree_reduction(red);
    free_view_reducer_ctx(ctx);
    free(key1_bin);
    free(key2_bin);
    free(key3_bin);
    free_view_value(&value1);
    free_view_value(&value2);
    free_view_value(&value3);
    free(value1_bin);
    free(value2_bin);
    free(value3_bin);
    free_node_list(nl);
}

static void test_view_btree_large_reducer(void)
{
    char *error_msg = NULL;
    view_reducer_ctx_t *ctx = NULL;
    const char *function_sources[] = {
        "function(key, values, rereduce) {"
        "   if (rereduce) {"
        "       var a = \"a\";"
        "       for(i=0;i<6000;i++) {"
        "           a = a + \"a\";"
        "       }"
        "       return a;"
        "   }"
        "   return 1;"
        "}"
    };

    view_btree_key_t key1, key2;
    char *key1_bin = NULL;
    char *key2_bin = NULL;
    size_t key1_bin_size = 0;
    size_t key2_bin_size = 0;

    view_btree_value_t value1, value2;
    char *value1_bin = NULL;
    char *value2_bin = NULL;
    char *value3_bin = NULL;
    size_t value1_bin_size = 0;
    size_t value2_bin_size = 0;

    view_btree_reduction_t reduction1;
    char reduction1_bin[7000];
    size_t reduction1_bin_size = 0;
    view_btree_reduction_t reduction2;
    char reduction2_bin[7000];
    size_t reduction2_bin_size = 0;

    nodelist *nl = NULL, *nl2 = NULL, *nl0 = NULL;
    node_pointer *np = NULL, *np2 = NULL;

    view_btree_reduction_t *red = NULL;

    ctx = make_view_reducer_ctx(function_sources, 1, &error_msg);
    cb_assert(ctx != NULL);

    key1.json_key.buf = "10";
    key1.json_key.size = sizeof("10") - 1;
    key1.doc_id.buf = "doc_10";
    key1.doc_id.size = sizeof("doc_10") - 1;
    cb_assert(encode_view_btree_key(&key1, &key1_bin, &key1_bin_size)
        == COUCHSTORE_SUCCESS);

    key2.json_key.buf = "11";
    key2.json_key.size = sizeof("11") - 1;
    key2.doc_id.buf = "doc_11";
    key2.doc_id.size = sizeof("doc_11") - 1;
    cb_assert(encode_view_btree_key(&key2, &key2_bin, &key2_bin_size)
        == COUCHSTORE_SUCCESS);

    value1.partition = 7;
    value1.num_values = 1;
    value1.values = (sized_buf *) malloc(sizeof(sized_buf) * 1);
    value1.values[0].buf = malloc(6000);
    value1.values[0].size = 6000;
    cb_assert(encode_view_btree_value(&value1, &value1_bin, &value1_bin_size)
        == COUCHSTORE_SUCCESS);

    value2.partition = 666;
    value2.num_values = 1;
    value2.values = (sized_buf *) malloc(sizeof(sized_buf) * 1);
    value2.values[0].buf = malloc(6000);
    value2.values[0].size = 6000;
    cb_assert(encode_view_btree_value(&value2, &value2_bin, &value2_bin_size)
        == COUCHSTORE_SUCCESS);

    nl0 = (nodelist *) malloc(sizeof(nodelist));
    cb_assert(nl0 != NULL);
    nl0->pointer = NULL;

    nl = (nodelist *) malloc(sizeof(nodelist));
    cb_assert(nl != NULL);
    nl->data.buf = value1_bin;
    nl->data.size = value1_bin_size;
    nl->key.buf = key1_bin;
    nl->key.size = key1_bin_size;
    nl->pointer = NULL;
    nl0->next = nl;

    nl2 = (nodelist *) malloc(sizeof(nodelist));
    cb_assert(nl2 != NULL);
    nl2->data.buf = value2_bin;
    nl2->data.size = value2_bin_size;
    nl2->key.buf = key2_bin;
    nl2->key.size = key2_bin_size;
    nl2->pointer = NULL;
    nl->next = nl2;
    nl2->next = NULL;

    reduction1.kv_count = 11;
    memset(&reduction1.partitions_bitmap, 0, sizeof(reduction1.partitions_bitmap));
    set_bit(&reduction1.partitions_bitmap, 10);
    set_bit(&reduction1.partitions_bitmap, 1011);
    reduction1.num_values = 1;
    reduction1.reduce_values = (sized_buf *) malloc(sizeof(sized_buf) * 1);
    cb_assert(reduction1.reduce_values != NULL);
    reduction1.reduce_values[0].buf = "100";
    reduction1.reduce_values[0].size = 3;
    cb_assert(encode_view_btree_reduction(&reduction1, reduction1_bin, &reduction1_bin_size)
        == COUCHSTORE_SUCCESS);

    reduction2.kv_count = 44;
    memset(&reduction2.partitions_bitmap, 0, sizeof(reduction2.partitions_bitmap));
    set_bit(&reduction2.partitions_bitmap, 10);
    set_bit(&reduction2.partitions_bitmap, 777);
    set_bit(&reduction2.partitions_bitmap, 333);
    reduction2.num_values = 1;
    reduction2.reduce_values = (sized_buf *) malloc(sizeof(sized_buf) * 1);
    cb_assert(reduction2.reduce_values != NULL);
    reduction2.reduce_values[0].buf = "100";
    reduction2.reduce_values[0].size = 3;
    cb_assert(encode_view_btree_reduction(&reduction2, reduction2_bin, &reduction2_bin_size)
        == COUCHSTORE_SUCCESS);

    np = (node_pointer *) malloc(sizeof(node_pointer));
    cb_assert(np != NULL);
    np->key.buf = key1_bin;
    np->key.size = key1_bin_size;
    np->reduce_value.buf = reduction1_bin;
    np->reduce_value.size = reduction1_bin_size;
    np->pointer = 0;
    np->subtreesize = 222;
    nl->pointer = np;

    np2 = (node_pointer *) malloc(sizeof(node_pointer));
    cb_assert(np2 != NULL);
    np2->key.buf = key2_bin;
    np2->key.size = key2_bin_size;
    np2->reduce_value.buf = reduction2_bin;
    np2->reduce_value.size = reduction2_bin_size;
    np2->pointer = 0;
    np2->subtreesize = 333;
    nl2->pointer = np2;

    couchstore_error_t ret = COUCHSTORE_SUCCESS;
    arena *transient_arena = new_arena(0);
    arena *persistent_arena = new_arena(0);
    couchfile_modify_result *mr;
    tree_file index_file;

    compare_info cmp = {
        .compare = view_btree_cmp
    };
    char *dst_file = "dst_file";
    cb_assert(transient_arena != NULL && persistent_arena != NULL);
    ret = tree_file_open(&index_file, dst_file, O_CREAT | O_RDWR,
        couchstore_get_default_file_ops());
    cb_assert(ret == COUCHSTORE_SUCCESS);
    mr = new_btree_modres(persistent_arena,
            transient_arena,
            &index_file,
            &cmp,
            view_btree_reduce,
            view_btree_rereduce,
            ctx,
            VIEW_KV_CHUNK_THRESHOLD + (VIEW_KV_CHUNK_THRESHOLD / 3),
            VIEW_KP_CHUNK_THRESHOLD + (VIEW_KP_CHUNK_THRESHOLD / 3));

    cb_assert(mr != NULL);
    couchfile_modify_result* targ_mr = make_modres(mr->arena, mr->rq);
    cb_assert(targ_mr != NULL);
    targ_mr->modified = 1;
    targ_mr->node_type = KP_NODE;
    targ_mr->pointers = nl0;
    targ_mr->pointers_end = nl2;
    finish_root(mr->rq, targ_mr, &ret);
    cb_assert(ret == COUCHSTORE_SUCCESS);

    tree_file_close(&index_file);
    remove(dst_file);
    free_view_reduction(&reduction1);
    free_view_reduction(&reduction2);
    free_view_btree_reduction(red);
    free_view_reducer_ctx(ctx);
    free(key1_bin);
    free(key2_bin);
    free(value1.values[0].buf);
    free(value2.values[0].buf);
    free_view_value(&value1);
    free_view_value(&value2);
    free(value1_bin);
    free(value2_bin);
    free(value3_bin);
    free_node_list(nl0);
    delete_arena(persistent_arena);
    delete_arena(transient_arena);
}

void reducer_tests(void)
{
    fprintf(stderr, "Running built-in reducer tests ... \n");
    test_view_id_btree_reducer();
    fprintf(stderr, "End of built-in view id btree reducer tests\n");
    test_view_btree_sum_reducer();
    fprintf(stderr, "End of built-in view btree sum reducer tests\n");
    test_view_btree_count_reducer();
    fprintf(stderr, "End of built-in view btree count reducer tests\n");
    test_view_btree_stats_reducer();
    fprintf(stderr, "End of built-in view btree stats reducer tests\n");
    test_view_btree_js_reducer();
    fprintf(stderr, "End of view btree js reducer tests\n");
    test_view_btree_multiple_reducers();
    fprintf(stderr, "End of view btree multiple reducer tests\n");
    test_view_btree_no_reducers();
    fprintf(stderr, "End of view btree no reducer tests\n");
    test_view_btree_large_reducer();
    fprintf(stderr, "End of view btree large reducer tests\n");
}
