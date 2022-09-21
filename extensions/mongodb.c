//
// The Hook Programming Language
// mongodb.c
//

#include "mongodb.h"
#include <mongoc/mongoc.h>
#include <bson/bson.h>
#include "hk_memory.h"
#include "hk_status.h"
#include "hk_error.h"

typedef struct
{
  HK_USERDATA_HEADER
  mongoc_uri_t *uri;
  mongoc_client_t *cli;
} mongodb_client_t;

typedef struct
{
  HK_USERDATA_HEADER
  mongodb_client_t *client;
  hk_string_t *name;
  mongoc_database_t *db;
} mongodb_database_t;

typedef struct
{
  HK_USERDATA_HEADER
  mongodb_database_t *database;
  hk_string_t *name;
  mongoc_collection_t *coll;
} mongodb_collection_t;

static int32_t init_count = 0;

static inline mongodb_client_t *mongodb_client_new(mongoc_uri_t *uri, mongoc_client_t *cli);
static inline mongodb_database_t *mongodb_database_new(mongodb_client_t *client,
  hk_string_t *name, mongoc_database_t *db);
static inline mongodb_collection_t *mongodb_collection_new(mongodb_database_t *database,
  hk_string_t *name, mongoc_collection_t *coll);
static void mongodb_client_deinit(hk_userdata_t *udata);
static void mongodb_database_deinit(hk_userdata_t *udata);
static void mongodb_collection_deinit(hk_userdata_t *udata);
static int32_t instance_to_bson(hk_vm_t *vm, hk_instance_t *inst, bson_t **result);
static int32_t bson_to_instance(hk_vm_t *vm, bson_t *bson, hk_instance_t **result);
static int32_t append_value_to_bson(hk_vm_t *vm, bson_t *bson, char *key, int32_t key_len,
  hk_value_t value);
static int32_t new_client_call(hk_vm_t *vm, hk_value_t *args);
static int32_t get_database_call(hk_vm_t *vm, hk_value_t *args);
static int32_t get_collection_call(hk_vm_t *vm, hk_value_t *args);
static int32_t insert_one_call(hk_vm_t *vm, hk_value_t *args);

static inline mongodb_client_t *mongodb_client_new(mongoc_uri_t *uri, mongoc_client_t *cli)
{
  if (!init_count)
    mongoc_init();
  ++init_count;
  mongodb_client_t *client = (mongodb_client_t *) hk_allocate(sizeof(*client));
  hk_userdata_init((hk_userdata_t *) client, &mongodb_client_deinit);
  client->uri = uri;
  client->cli = cli;
  return client;
}

static inline mongodb_database_t *mongodb_database_new(mongodb_client_t *client,
  hk_string_t *name, mongoc_database_t *db)
{
  mongodb_database_t *database = (mongodb_database_t *) hk_allocate(sizeof(*database));
  hk_userdata_init((hk_userdata_t *) database, &mongodb_database_deinit);
  hk_incr_ref(client);
  hk_incr_ref(name);
  database->client = client;
  database->name = name;
  database->db = db;
  return database;
}

static inline mongodb_collection_t *mongodb_collection_new(mongodb_database_t *database,
  hk_string_t *name, mongoc_collection_t *coll)
{
  mongodb_collection_t *collection = (mongodb_collection_t *) hk_allocate(sizeof(*collection));
  hk_userdata_init((hk_userdata_t *) collection, &mongodb_collection_deinit);
  hk_incr_ref(database);
  hk_incr_ref(name);
  collection->database = database;
  collection->name = name;
  collection->coll = coll;
  return collection;
}

static void mongodb_client_deinit(hk_userdata_t *udata)
{
  mongodb_client_t *client = (mongodb_client_t *) udata;
  mongoc_uri_destroy(client->uri);
  mongoc_client_destroy(client->cli);
  --init_count;
  if (!init_count)
    mongoc_cleanup();
}

static void mongodb_database_deinit(hk_userdata_t *udata)
{
  mongodb_database_t *database = (mongodb_database_t *) udata;
  hk_userdata_release((hk_userdata_t *) database->client);
  hk_string_release(database->name);
  mongoc_database_destroy(database->db);
}

static void mongodb_collection_deinit(hk_userdata_t *udata)
{
  mongodb_collection_t *collection = (mongodb_collection_t *) udata;
  hk_userdata_release((hk_userdata_t *) collection->database);
  hk_string_release(collection->name);
  mongoc_collection_destroy(collection->coll);
}

static int32_t instance_to_bson(hk_vm_t *vm, hk_instance_t *inst, bson_t **result)
{
  bson_t *bson = bson_new();
  hk_struct_t *ztruct = inst->ztruct;
  int32_t length = ztruct->length;
  hk_field_t *fields = ztruct->fields;
  hk_value_t *values = inst->values;
  for (int32_t i = 0; i < length; ++i)
  {
    hk_field_t *field = &fields[i];
    hk_string_t *key = field->name;
    hk_value_t value = values[field->index];
    if (append_value_to_bson(vm, bson, key->chars, key->length, value) == HK_STATUS_ERROR)
    {
      bson_destroy(bson);
      return HK_STATUS_ERROR;
    }
  }
  *result = bson;
  return HK_STATUS_OK;
}

static int32_t bson_to_instance(hk_vm_t *vm, bson_t *bson, hk_instance_t **result)
{
  (void) vm;
  hk_struct_t *ztruct = hk_struct_new(NULL);
  bson_iter_t iter;
  bson_iter_init(&iter, bson);
  while (bson_iter_next(&iter))
  {
    const char *chars = bson_iter_key(&iter);
    int32_t length = bson_iter_key_len(&iter);
    hk_string_t *field_name = hk_string_from_chars(length, chars);
    hk_assert(hk_struct_define_field(ztruct, field_name), "failed to define field");
  }
  hk_instance_t *inst = hk_instance_new(ztruct);
  bson_iter_init(&iter, bson);
  int32_t index = 0;
  while (bson_iter_next(&iter))
  {
    switch (bson_iter_type(&iter))
    {
      case BSON_TYPE_EOD:
      case BSON_TYPE_UNDEFINED:
      case BSON_TYPE_REGEX:
      case BSON_TYPE_DBPOINTER:
      case BSON_TYPE_CODE:
      case BSON_TYPE_SYMBOL:
      case BSON_TYPE_CODEWSCOPE:
      case BSON_TYPE_MAXKEY:
      case BSON_TYPE_MINKEY:
      case BSON_TYPE_DECIMAL128:
        hk_runtime_error("unsupported BSON type");
        hk_struct_release(ztruct);
        return HK_STATUS_ERROR;
      case BSON_TYPE_DOUBLE:
        {
          hk_value_t value = hk_float_value(bson_iter_double(&iter));
          hk_instance_inplace_set_field(inst, index, value);
        }
        break;
      case BSON_TYPE_UTF8:
        {
          uint32_t length;
          const char *chars = bson_iter_utf8(&iter, &length);
          hk_value_t value = hk_string_value(hk_string_from_chars((int32_t) length, chars));
          hk_instance_inplace_set_field(inst, index, value);
        }
        break;
      case BSON_TYPE_DOCUMENT:
        // TODO implement
        break;
      case BSON_TYPE_ARRAY:
        // TODO implement
        break;
      case BSON_TYPE_BINARY:
        // TODO implement
        break;
      case BSON_TYPE_OID:
        // TODO implement
        break;
      case BSON_TYPE_BOOL:
        // TODO implement
        break;
      case BSON_TYPE_DATE_TIME:
        // TODO implement
        break;
      case BSON_TYPE_NULL:
        // TODO implement
        break;
      case BSON_TYPE_INT32:
        // TODO implement
        break;
      case BSON_TYPE_TIMESTAMP:
        // TODO implement
        break;
      case BSON_TYPE_INT64:
        break;
      }
      ++index;
  }
  *result = inst;
  return HK_STATUS_OK;
}

static int32_t append_value_to_bson(hk_vm_t *vm, bson_t *bson, char *key, int32_t key_len,
  hk_value_t value)
{
  switch (value.type)
  {
  case HK_TYPE_NIL:
    bson_append_null(bson, key, key_len);
    break;
  case HK_TYPE_BOOL:
    bson_append_bool(bson, key, key_len, hk_as_bool(value));
    break;
  case HK_TYPE_FLOAT:
    bson_append_double(bson, key, key_len, hk_as_float(value));
    break;
  case HK_TYPE_STRING:
    {
      hk_string_t *str = hk_as_string(value);
      bson_append_utf8(bson, key, key_len, str->chars, str->length);
    }
    break;
  case HK_TYPE_RANGE:
  case HK_TYPE_STRUCT:
  case HK_TYPE_ITERATOR:
  case HK_TYPE_CALLABLE:
  case HK_TYPE_USERDATA:
    hk_runtime_error("cannot convert value to BSON");
    return HK_STATUS_ERROR;
  case HK_TYPE_ARRAY:
    {
      hk_array_t *arr = hk_as_array(value);
      int32_t length = arr->length;
      hk_value_t *elements = arr->elements;
      bson_t child;
      bson_append_array_begin(bson, key, key_len, &child);
      for (int32_t i = 0; i < length; ++i)
      {
        char index[16] = "";
        sprintf(index, "%d", i);
        int32_t index_len = (int32_t) strnlen(index, sizeof(index) - 1);
        hk_value_t elem = elements[i];
        if (append_value_to_bson(vm, &child, index, index_len, elem) == HK_STATUS_ERROR)
          return HK_STATUS_ERROR;
      }
      bson_append_array_end(bson, &child);
    }
    break;
  case HK_TYPE_INSTANCE:
    {
      hk_instance_t *inst = hk_as_instance(value);
      hk_struct_t *ztruct = inst->ztruct;
      int32_t length = ztruct->length;
      hk_field_t *fields = ztruct->fields;
      hk_value_t *values = inst->values;
      bson_t child;
      bson_append_document_begin(bson, key, key_len, &child);
      for (int32_t i = 0; i < length; ++i)
      {
        hk_field_t *field = &fields[i];
        hk_string_t *key = field->name;
        hk_value_t value = values[field->index];
        if (append_value_to_bson(vm, &child, key->chars, key->length, value) == HK_STATUS_ERROR)
          return HK_STATUS_ERROR;
      }
      bson_append_document_end(bson, &child);
    }
    break;
  }
  return HK_STATUS_OK;
}

static int32_t new_client_call(hk_vm_t *vm, hk_value_t *args)
{
  if (hk_vm_check_string(args, 1) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  hk_string_t *str = hk_as_string(args[1]);
  bson_error_t error;
  mongoc_uri_t *uri = mongoc_uri_new_with_error(str->chars, &error);
  if (!uri)
  {
    hk_runtime_error("URI parser error: %s", error.message);
    return HK_STATUS_ERROR;
  }
  mongoc_client_t *cli = mongoc_client_new_from_uri(uri);
  if (!cli)
  {
    hk_runtime_error("cannot create client");
    mongoc_uri_destroy(uri);
    return HK_STATUS_ERROR;
  }
  if (hk_vm_push_userdata(vm, (hk_userdata_t *) mongodb_client_new(uri, cli)) == HK_STATUS_ERROR)
  {
    mongoc_uri_destroy(uri);
    mongoc_client_destroy(cli);
    return HK_STATUS_ERROR;
  }
  return HK_STATUS_OK;
}

static int32_t get_database_call(hk_vm_t *vm, hk_value_t *args)
{
  if (hk_vm_check_userdata(args, 1) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_vm_check_string(args, 2) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  mongodb_client_t *client = (mongodb_client_t *) hk_as_userdata(args[1]);
  hk_string_t *name = hk_as_string(args[2]);
  mongoc_database_t *db = mongoc_client_get_database(client->cli, name->chars);
  hk_assert(db, "mongoc_client_get_database returned NULL");
  if (hk_vm_push_userdata(vm, (hk_userdata_t *) mongodb_database_new(client, name, db))
    == HK_STATUS_ERROR)
  {
    mongoc_database_destroy(db);
    return HK_STATUS_ERROR;
  }
  return HK_STATUS_OK;
}

static int32_t get_collection_call(hk_vm_t *vm, hk_value_t *args)
{
  if (hk_vm_check_userdata(args, 1) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_vm_check_string(args, 2) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  mongodb_database_t *database = (mongodb_database_t *) hk_as_userdata(args[1]);
  hk_string_t *name = hk_as_string(args[2]);
  mongoc_collection_t *coll = mongoc_client_get_collection(database->client->cli,
    database->name->chars, name->chars);
  hk_assert(coll, "mongoc_client_get_collection returned NULL");
  if (hk_vm_push_userdata(vm, (hk_userdata_t *) mongodb_collection_new(database, name, coll))
    == HK_STATUS_ERROR)
  {
    mongoc_collection_destroy(coll);
    return HK_STATUS_ERROR;
  }
  return HK_STATUS_OK;
}

static int32_t insert_one_call(hk_vm_t *vm, hk_value_t *args)
{
  if (hk_vm_check_userdata(args, 1) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_vm_check_instance(args, 2) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_vm_check_types(args, 3, 2, (int32_t[]) {HK_TYPE_NIL, HK_TYPE_INSTANCE}) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  mongodb_collection_t *collection = (mongodb_collection_t *) hk_as_userdata(args[1]);
  hk_instance_t *inst = hk_as_instance(args[2]);
  bson_t *opts = NULL;
  if (!hk_is_nil(args[3]))
    if (instance_to_bson(vm, hk_as_instance(args[3]), &opts) == HK_STATUS_ERROR)
      return HK_STATUS_ERROR;
  bson_t reply;
  bson_error_t error;
  bson_t *doc;
  if (instance_to_bson(vm, inst, &doc) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (!mongoc_collection_insert_one(collection->coll, doc, opts, &reply, &error))
  {
    hk_runtime_error("cannot insert document: %s", error.message);
    bson_destroy(doc);
    return HK_STATUS_ERROR;
  }
  bson_destroy(doc);
  hk_instance_t *result;
  if (bson_to_instance(vm, &reply, &result) == HK_STATUS_ERROR)
  {
    bson_destroy(&reply);
    return HK_STATUS_ERROR;
  }
  bson_destroy(&reply);
  if (hk_vm_push_instance(vm, result) == HK_STATUS_ERROR)
  {
    hk_instance_free(result);
    return HK_STATUS_ERROR;
  }
  return HK_STATUS_OK;
}

HK_LOAD_FN(mongodb)
{
  if (hk_vm_push_string_from_chars(vm, -1, "mongodb") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_vm_push_string_from_chars(vm, -1, "new_client") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_vm_push_new_native(vm, "new_client", 1, &new_client_call) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_vm_push_string_from_chars(vm, -1, "get_database") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_vm_push_new_native(vm, "get_database", 2, &get_database_call) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_vm_push_string_from_chars(vm, -1, "get_collection") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_vm_push_new_native(vm, "get_collection", 2, &get_collection_call) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_vm_push_string_from_chars(vm, -1, "insert_one") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_vm_push_new_native(vm, "insert_one", 3, &insert_one_call) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  return hk_vm_construct(vm, 4);
}
