//
// The Hook Programming Language
// secp256r1.c
//

#include "secp256r1.h"
#define ECC_CURVE 32
#include "deps/ecc.h"
#include <hook/check.h>
#include <hook/status.h>

#define PUBLIC_KEY_SIZE  (ECC_BYTES + 1)
#define PRIVATE_KEY_SIZE ECC_BYTES
#define SECRET_SIZE      ECC_BYTES
#define HASH_SIZE        ECC_BYTES
#define SIGNATURE_SIZE   (ECC_BYTES << 1)

static int32_t new_key_pair_call(hk_state_t *state, hk_value_t *args);
static int32_t shared_secret_call(hk_state_t *state, hk_value_t *args);
static int32_t sign_hash_call(hk_state_t *state, hk_value_t *args);
static int32_t verify_signature_call(hk_state_t *state, hk_value_t *args);

static int32_t new_key_pair_call(hk_state_t *state, hk_value_t *args)
{
  (void) args;
  hk_string_t *pub_key = hk_string_new_with_capacity(PUBLIC_KEY_SIZE);
  pub_key->length = PUBLIC_KEY_SIZE;
  pub_key->chars[PUBLIC_KEY_SIZE] = '\0';
  hk_string_t *priv_key = hk_string_new_with_capacity(PRIVATE_KEY_SIZE);
  priv_key->length = PRIVATE_KEY_SIZE;
  priv_key->chars[PRIVATE_KEY_SIZE] = '\0';
  (void) ecc_make_key((uint8_t *) pub_key->chars, (uint8_t *) priv_key->chars);
  hk_array_t *arr = hk_array_new_with_capacity(2);
  arr->length = 2;
  hk_incr_ref(pub_key);
  hk_incr_ref(priv_key);
  arr->elements[0] = hk_string_value(pub_key);
  arr->elements[1] = hk_string_value(priv_key);
  if (hk_state_push_array(state, arr) == HK_STATUS_ERROR)
  {
    hk_array_free(arr);
    return HK_STATUS_ERROR;
  }
  return HK_STATUS_OK;
}

static int32_t shared_secret_call(hk_state_t *state, hk_value_t *args)
{
  if (hk_check_argument_string(args, 1) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_check_argument_string(args, 2) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  hk_string_t *pub_key = hk_as_string(args[1]);
  hk_string_t *priv_key = hk_as_string(args[2]);
  hk_string_t *secret = hk_string_new_with_capacity(SECRET_SIZE);
  secret->length = SECRET_SIZE;
  secret->chars[SECRET_SIZE] = '\0';
  (void) ecdh_shared_secret((uint8_t *) pub_key->chars, (uint8_t *) priv_key->chars,
    (uint8_t *) secret->chars);
  if (hk_state_push_string(state, secret) == HK_STATUS_ERROR)
  {
    hk_string_free(secret);
    return HK_STATUS_ERROR;
  }
  return HK_STATUS_OK;
}

static int32_t sign_hash_call(hk_state_t *state, hk_value_t *args)
{
  if (hk_check_argument_string(args, 1) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_check_argument_string(args, 2) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  hk_string_t *priv_key = hk_as_string(args[1]);
  hk_string_t *hash = hk_as_string(args[2]);
  hk_string_t *signature = hk_string_new_with_capacity(SIGNATURE_SIZE);
  signature->length = SIGNATURE_SIZE;
  signature->chars[SIGNATURE_SIZE] = '\0';
  (void) ecdsa_sign((uint8_t *) priv_key->chars, (uint8_t *) hash->chars,
    (uint8_t *) signature->chars);
  if (hk_state_push_string(state, signature) == HK_STATUS_ERROR)
  {
    hk_string_free(signature);
    return HK_STATUS_ERROR;
  }
  return HK_STATUS_OK;
}

static int32_t verify_signature_call(hk_state_t *state, hk_value_t *args)
{
  if (hk_check_argument_string(args, 1) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_check_argument_string(args, 2) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_check_argument_string(args, 3) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  hk_string_t *pub_key = hk_as_string(args[1]);
  hk_string_t *hash = hk_as_string(args[2]);
  hk_string_t *signature = hk_as_string(args[3]);
  bool valid = (bool) ecdsa_verify((uint8_t *) pub_key->chars, (uint8_t *) hash->chars,
    (uint8_t *) signature->chars);
  return hk_state_push_bool(state, valid);
}

HK_LOAD_FN(secp256r1)
{
  if (hk_state_push_string_from_chars(state, -1, "secp256r1") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_string_from_chars(state, -1, "PUBLIC_KEY_SIZE") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_number(state, PUBLIC_KEY_SIZE) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_string_from_chars(state, -1, "PRIVATE_KEY_SIZE") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_number(state, PRIVATE_KEY_SIZE) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_string_from_chars(state, -1, "SECRET_SIZE") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_number(state, SECRET_SIZE) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_string_from_chars(state, -1, "HASH_SIZE") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_number(state, HASH_SIZE) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_string_from_chars(state, -1, "SIGNATURE_SIZE") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_number(state, SIGNATURE_SIZE) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_string_from_chars(state, -1, "new_key_pair") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_new_native(state, "new_key_pair", 0, &new_key_pair_call) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_string_from_chars(state, -1, "shared_secret") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_new_native(state, "shared_secret", 2, &shared_secret_call) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_string_from_chars(state, -1, "sign_hash") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_new_native(state, "sign_hash", 2, &sign_hash_call) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_string_from_chars(state, -1, "verify_signature") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_new_native(state, "verify_signature", 3, &verify_signature_call) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  return hk_state_construct(state, 9);
}
