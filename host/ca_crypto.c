#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <tee_client_api.h>
#include "../ta/common.h"

void print_hex(uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++)
        printf("%02x ", data[i]);
    printf("\n");
}

int main(void)
{
    TEEC_Result res;
    TEEC_Context ctx;
    TEEC_Session sess;
    TEEC_UUID uuid = TA_CRYPTO_UUID;
    uint32_t err_origin;
    char input[256];
    uint8_t plaintext[256];
    uint8_t ciphertext[256];
    uint8_t decrypted[256];
    size_t len;

    printf("\n========================================\n");
    printf("   TEE SM4 加密系统\n");
    printf("========================================\n\n");

    res = TEEC_InitializeContext(NULL, &ctx);
    if (res != TEEC_SUCCESS) {
        printf("TEE初始化失败\n");
        return -1;
    }

    res = TEEC_OpenSession(&ctx, &sess, &uuid, TEEC_LOGIN_PUBLIC, NULL, NULL, &err_origin);
    if (res != TEEC_SUCCESS) {
        printf("TA会话打开失败\n");
        TEEC_FinalizeContext(&ctx);
        return -1;
    }

    printf("[1] 生成密钥...\n");
    res = TEEC_InvokeCommand(&sess, CMD_GEN_KEY, NULL, &err_origin);
    if (res != TEEC_SUCCESS) {
        printf("密钥生成失败\n");
        goto cleanup;
    }
    printf("✓ 密钥已生成并存储于TEE安全区域\n\n");

    printf("请输入需要加密的字符串: ");
    fflush(stdout);
    fgets(input, sizeof(input), stdin);
    input[strcspn(input, "\n")] = 0;

    len = strlen(input);
    if (len == 0) {
        printf("输入为空\n");
        goto cleanup;
    }

    memcpy(plaintext, input, len);
    while (len % 16 != 0) {
        plaintext[len] = ' ';
        len++;
    }

    printf("\n[加密] 明文: %s\n", input);

    TEEC_Operation op_enc = {0};
    op_enc.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT, TEEC_MEMREF_TEMP_OUTPUT, TEEC_NONE, TEEC_NONE);
    op_enc.params[0].tmpref.buffer = plaintext;
    op_enc.params[0].tmpref.size = len;
    op_enc.params[1].tmpref.buffer = ciphertext;
    op_enc.params[1].tmpref.size = sizeof(ciphertext);

    res = TEEC_InvokeCommand(&sess, CMD_ENCRYPT, &op_enc, &err_origin);
    if (res != TEEC_SUCCESS) {
        printf("加密失败\n");
        goto cleanup;
    }
    printf("[加密] 密文: ");
    print_hex(ciphertext, op_enc.params[1].tmpref.size);

    printf("\n[解密] 开始解密...\n");
    TEEC_Operation op_dec = {0};
    op_dec.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT, TEEC_MEMREF_TEMP_OUTPUT, TEEC_NONE, TEEC_NONE);
    op_dec.params[0].tmpref.buffer = ciphertext;
    op_dec.params[0].tmpref.size = op_enc.params[1].tmpref.size;
    op_dec.params[1].tmpref.buffer = decrypted;
    op_dec.params[1].tmpref.size = sizeof(decrypted);

    res = TEEC_InvokeCommand(&sess, CMD_DECRYPT, &op_dec, &err_origin);
    if (res != TEEC_SUCCESS) {
        printf("解密失败\n");
        goto cleanup;
    }
    decrypted[op_dec.params[1].tmpref.size] = 0;
    printf("[解密] 明文: %s\n", decrypted);

    printf("\n[验证] ");
    if (strncmp(input, (char*)decrypted, strlen(input)) == 0)
        printf("✓ 成功\n");
    else
        printf("✗ 失败\n");

cleanup:
    TEEC_CloseSession(&sess);
    TEEC_FinalizeContext(&ctx);
    return 0;
}