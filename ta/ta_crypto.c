#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>
#include <string.h>
#include "common.h"

#define KEY_SIZE 16
#define KEY_ID "tee_sm4_key"
#define SM4_BLOCK_SIZE 16

static TEE_ObjectHandle key_handle = TEE_HANDLE_NULL;

static TEE_Result generate_key(void)
{
    TEE_Result res;
    TEE_ObjectHandle transient_key = TEE_HANDLE_NULL;
    TEE_ObjectHandle persistent_key = TEE_HANDLE_NULL;
    const char *obj_id = KEY_ID;
    uint32_t obj_id_len = strlen(obj_id);

    res = TEE_AllocateTransientObject(TEE_TYPE_SM4, KEY_SIZE * 8, &transient_key);
    if (res != TEE_SUCCESS) return res;

    res = TEE_GenerateKey(transient_key, KEY_SIZE * 8, NULL, 0);
    if (res != TEE_SUCCESS) goto cleanup;

    res = TEE_CreatePersistentObject(TEE_STORAGE_PRIVATE, obj_id, obj_id_len,
        TEE_DATA_FLAG_ACCESS_READ | TEE_DATA_FLAG_ACCESS_WRITE,
        transient_key, NULL, 0, &persistent_key);
    if (res != TEE_SUCCESS) goto cleanup;

    key_handle = persistent_key;

cleanup:
    if (transient_key != TEE_HANDLE_NULL)
        TEE_FreeTransientObject(transient_key);
    return res;
}

static TEE_Result load_key(void)
{
    const char *obj_id = KEY_ID;
    uint32_t obj_id_len = strlen(obj_id);
    return TEE_OpenPersistentObject(TEE_STORAGE_PRIVATE, obj_id, obj_id_len,
        TEE_DATA_FLAG_ACCESS_READ, &key_handle);
}

static TEE_Result encrypt_data(uint8_t *in, size_t in_len, uint8_t *out, size_t *out_len)
{
    TEE_Result res;
    TEE_OperationHandle op = TEE_HANDLE_NULL;

    if (in_len % SM4_BLOCK_SIZE != 0)
        return TEE_ERROR_BAD_PARAMETERS;

    *out_len = in_len;

    res = TEE_AllocateOperation(&op, TEE_ALG_SM4_ECB_NOPAD, TEE_MODE_ENCRYPT, KEY_SIZE * 8);
    if (res != TEE_SUCCESS) return res;

    res = TEE_SetOperationKey(op, key_handle);
    if (res != TEE_SUCCESS) goto exit;

    TEE_CipherUpdate(op, in, in_len, out, out_len);

exit:
    TEE_FreeOperation(op);
    return res;
}

static TEE_Result decrypt_data(uint8_t *in, size_t in_len, uint8_t *out, size_t *out_len)
{
    TEE_Result res;
    TEE_OperationHandle op = TEE_HANDLE_NULL;

    if (in_len % SM4_BLOCK_SIZE != 0)
        return TEE_ERROR_BAD_PARAMETERS;

    *out_len = in_len;

    res = TEE_AllocateOperation(&op, TEE_ALG_SM4_ECB_NOPAD, TEE_MODE_DECRYPT, KEY_SIZE * 8);
    if (res != TEE_SUCCESS) return res;

    res = TEE_SetOperationKey(op, key_handle);
    if (res != TEE_SUCCESS) goto exit;

    TEE_CipherUpdate(op, in, in_len, out, out_len);

exit:
    TEE_FreeOperation(op);
    return res;
}

TEE_Result TA_CreateEntryPoint(void) { return TEE_SUCCESS; }
void TA_DestroyEntryPoint(void)
{
    if (key_handle != TEE_HANDLE_NULL)
        TEE_CloseObject(key_handle);
}

TEE_Result TA_OpenSessionEntryPoint(uint32_t pt, TEE_Param p[4], void **ctx)
{
    (void)pt; (void)p;
    *ctx = NULL;
    return TEE_SUCCESS;
}

void TA_CloseSessionEntryPoint(void *ctx) { (void)ctx; }

TEE_Result TA_InvokeCommandEntryPoint(void *ctx, uint32_t cmd,
                                      uint32_t pt, TEE_Param p[4])
{
    TEE_Result res;
    (void)ctx; (void)pt;

    switch (cmd) {
    case CMD_GEN_KEY:
        return generate_key();
    case CMD_ENCRYPT:
        if (key_handle == TEE_HANDLE_NULL) {
            res = load_key();
            if (res != TEE_SUCCESS) return res;
        }
        return encrypt_data(p[0].memref.buffer, p[0].memref.size,
                           p[1].memref.buffer, &p[1].memref.size);
    case CMD_DECRYPT:
        if (key_handle == TEE_HANDLE_NULL) {
            res = load_key();
            if (res != TEE_SUCCESS) return res;
        }
        return decrypt_data(p[0].memref.buffer, p[0].memref.size,
                           p[1].memref.buffer, &p[1].memref.size);
    default:
        return TEE_ERROR_BAD_PARAMETERS;
    }
}