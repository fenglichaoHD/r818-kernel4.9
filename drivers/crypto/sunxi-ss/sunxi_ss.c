/*
 * The driver of SUNXI SecuritySystem controller.
 *
 * Copyright (C) 2013 Allwinner.
 *
 * Mintow <duanmintao@allwinnertech.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/clk/sunxi.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <crypto/hash.h>
#include <crypto/md5.h>
#include <crypto/des.h>
#include <crypto/internal/aead.h>
#include <crypto/internal/hash.h>
#include <crypto/internal/rng.h>

#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/dma/sunxi-dma.h>
#include <linux/dmapool.h>

#include "sunxi_ss.h"
#include "sunxi_ss_proc.h"
#include "sunxi_ss_reg.h"

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

static const struct of_device_id sunxi_ss_of_match[] = {
		{.compatible = "allwinner,sunxi-ce",},
		{},
};
MODULE_DEVICE_TABLE(of, sunxi_ss_of_match);
#endif

sunxi_ss_t *ss_dev;
sunxi_ce_cdev_t	*ce_cdev;

static DEFINE_MUTEX(ss_lock);

void ss_dev_lock(void)
{
	mutex_lock(&ss_lock);
}

void ss_dev_unlock(void)
{
	mutex_unlock(&ss_lock);
}

void __iomem *ss_membase(void)
{
	return ss_dev->base_addr;
}

void ss_reset(void)
{
	SS_ENTER();
	sunxi_periph_reset_assert(ss_dev->mclk);
	sunxi_periph_reset_deassert(ss_dev->mclk);
}

#ifdef SS_RSA_CLK_ENABLE
void ss_clk_set(u32 rate)
{
#ifdef CONFIG_EVB_PLATFORM
	int ret = 0;

	ret = clk_get_rate(ss_dev->mclk);
	if (ret == rate)
		return;

	SS_DBG("Change the SS clk to %d MHz.\n", rate/1000000);
	ret = clk_set_rate(ss_dev->mclk, rate);
	if (ret != 0)
		SS_ERR("clk_set_rate(%d) failed! return %d\n", rate, ret);
#endif
}
#endif

static int ss_aes_key_is_weak(const u8 *key, unsigned int keylen)
{
	s32 i;
	u8 tmp = key[0];

	for (i = 0; i < keylen; i++)
		if (tmp != key[i])
			return 0;

	SS_ERR("The key is weak!\n");
	return 1;
}

#ifdef SS_GCM_MODE_ENABLE
static int sunxi_aes_gcm_setkey(struct crypto_aead *tfm, const u8 *key,
				unsigned int keylen)
{
	ss_aead_ctx_t *ctx = crypto_aead_ctx(tfm);

	if (keylen != AES_KEYSIZE_256 &&
	    keylen != AES_KEYSIZE_192 &&
	    keylen != AES_KEYSIZE_128) {
		crypto_aead_set_flags(tfm, CRYPTO_TFM_RES_BAD_KEY_LEN);
		return -EINVAL;
	}

	memcpy(ctx->key, key, keylen);
	ctx->key_size = keylen;

	return 0;
}

static int sunxi_aes_gcm_setauthsize(struct crypto_aead *tfm,
				     unsigned int authsize)
{
	/* Same as crypto_gcm_authsize() from crypto/gcm.c */
	switch (authsize) {
	case 4:
	case 8:
	case 12:
	case 13:
	case 14:
	case 15:
	case 16:
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
#endif

static int ss_aes_setkey(struct crypto_ablkcipher *tfm, const u8 *key,
				unsigned int keylen)
{
	int ret = 0;
	ss_aes_ctx_t *ctx = crypto_ablkcipher_ctx(tfm);

	SS_DBG("keylen = %d\n", keylen);
	if (ctx->comm.flags & SS_FLAG_NEW_KEY) {
		SS_ERR("The key has already update.\n");
		return -EBUSY;
	}

	ret = ss_aes_key_valid(tfm, keylen);
	if (ret != 0)
		return ret;

	if (ss_aes_key_is_weak(key, keylen)) {
		crypto_ablkcipher_tfm(tfm)->crt_flags
					|= CRYPTO_TFM_REQ_WEAK_KEY;
		/* testmgr.c need this, but we don't want to support it. */
/*		return -EINVAL; */
	}

	ctx->key_size = keylen;
	memcpy(ctx->key, key, keylen);
	if (keylen < AES_KEYSIZE_256)
		memset(&ctx->key[keylen], 0, AES_KEYSIZE_256 - keylen);

	ctx->comm.flags |= SS_FLAG_NEW_KEY;
	return 0;
}

static int ss_aes_ecb_encrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req,
		SS_DIR_ENCRYPT, SS_METHOD_AES, SS_AES_MODE_ECB);
}

static int ss_aes_ecb_decrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req,
		SS_DIR_DECRYPT, SS_METHOD_AES, SS_AES_MODE_ECB);
}

static int ss_aes_cbc_encrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req,
		SS_DIR_ENCRYPT, SS_METHOD_AES, SS_AES_MODE_CBC);
}

static int ss_aes_cbc_decrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req,
		SS_DIR_DECRYPT, SS_METHOD_AES, SS_AES_MODE_CBC);
}

#ifdef SS_CTR_MODE_ENABLE
static int ss_aes_ctr_encrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req,
		SS_DIR_ENCRYPT, SS_METHOD_AES, SS_AES_MODE_CTR);
}

static int ss_aes_ctr_decrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req,
		SS_DIR_DECRYPT, SS_METHOD_AES, SS_AES_MODE_CTR);
}
#endif

#ifdef SS_CTS_MODE_ENABLE
static int ss_aes_cts_encrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req,
		SS_DIR_ENCRYPT, SS_METHOD_AES, SS_AES_MODE_CTS);
}

static int ss_aes_cts_decrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req,
		SS_DIR_DECRYPT, SS_METHOD_AES, SS_AES_MODE_CTS);
}
#endif

#ifdef SS_XTS_MODE_ENABLE
static int ss_aes_xts_encrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req,
			SS_DIR_ENCRYPT, SS_METHOD_AES, SS_AES_MODE_XTS);
}

static int ss_aes_xts_decrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req,
			SS_DIR_DECRYPT, SS_METHOD_AES, SS_AES_MODE_XTS);
}
#endif

#ifdef SS_OFB_MODE_ENABLE
static int ss_aes_ofb_encrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req,
		SS_DIR_ENCRYPT, SS_METHOD_AES, SS_AES_MODE_OFB);
}

static int ss_aes_ofb_decrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req,
		SS_DIR_DECRYPT, SS_METHOD_AES, SS_AES_MODE_OFB);
}
#endif

#ifdef SS_CFB_MODE_ENABLE
static int ss_aes_cfb1_encrypt(struct ablkcipher_request *req)
{
	ss_aes_req_ctx_t *req_ctx = ablkcipher_request_ctx(req);

	req_ctx->bitwidth = 1;
	return ss_aes_crypt(req,
		SS_DIR_ENCRYPT, SS_METHOD_AES, SS_AES_MODE_CFB);
}

static int ss_aes_cfb1_decrypt(struct ablkcipher_request *req)
{
	ss_aes_req_ctx_t *req_ctx = ablkcipher_request_ctx(req);

	req_ctx->bitwidth = 1;
	return ss_aes_crypt(req,
		SS_DIR_DECRYPT, SS_METHOD_AES, SS_AES_MODE_CFB);
}

static int ss_aes_cfb8_encrypt(struct ablkcipher_request *req)
{
	ss_aes_req_ctx_t *req_ctx = ablkcipher_request_ctx(req);

	req_ctx->bitwidth = 8;
	return ss_aes_crypt(req,
		SS_DIR_ENCRYPT, SS_METHOD_AES, SS_AES_MODE_CFB);
}

static int ss_aes_cfb8_decrypt(struct ablkcipher_request *req)
{
	ss_aes_req_ctx_t *req_ctx = ablkcipher_request_ctx(req);

	req_ctx->bitwidth = 8;
	return ss_aes_crypt(req,
		SS_DIR_DECRYPT, SS_METHOD_AES, SS_AES_MODE_CFB);
}

static int ss_aes_cfb64_encrypt(struct ablkcipher_request *req)
{
	ss_aes_req_ctx_t *req_ctx = ablkcipher_request_ctx(req);

	req_ctx->bitwidth = 64;
	return ss_aes_crypt(req,
		SS_DIR_ENCRYPT, SS_METHOD_AES, SS_AES_MODE_CFB);
}

static int ss_aes_cfb64_decrypt(struct ablkcipher_request *req)
{
	ss_aes_req_ctx_t *req_ctx = ablkcipher_request_ctx(req);

	req_ctx->bitwidth = 64;
	return ss_aes_crypt(req,
		SS_DIR_DECRYPT, SS_METHOD_AES, SS_AES_MODE_CFB);
}

static int ss_aes_cfb128_encrypt(struct ablkcipher_request *req)
{
	ss_aes_req_ctx_t *req_ctx = ablkcipher_request_ctx(req);

	req_ctx->bitwidth = 128;
	return ss_aes_crypt(req,
		SS_DIR_ENCRYPT, SS_METHOD_AES, SS_AES_MODE_CFB);
}

static int ss_aes_cfb128_decrypt(struct ablkcipher_request *req)
{
	ss_aes_req_ctx_t *req_ctx = ablkcipher_request_ctx(req);

	req_ctx->bitwidth = 128;
	return ss_aes_crypt(req,
		SS_DIR_DECRYPT, SS_METHOD_AES, SS_AES_MODE_CFB);
}
#endif

#ifdef SS_GCM_MODE_ENABLE
static int sunxi_aes_gcm_encrypt(struct aead_request *req)
{
	return ss_aead_crypt(req,
		SS_DIR_ENCRYPT, SS_METHOD_AES, SS_AES_MODE_GCM);
}

static int sunxi_aes_gcm_decrypt(struct aead_request *req)
{
	return ss_aead_crypt(req,
		SS_DIR_DECRYPT, SS_METHOD_AES, SS_AES_MODE_GCM);
}
#endif

static int ss_des_ecb_encrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req,
		SS_DIR_ENCRYPT, SS_METHOD_DES, SS_AES_MODE_ECB);
}

static int ss_des_ecb_decrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req,
		SS_DIR_DECRYPT, SS_METHOD_DES, SS_AES_MODE_ECB);
}

static int ss_des_cbc_encrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req,
		SS_DIR_ENCRYPT, SS_METHOD_DES, SS_AES_MODE_CBC);
}

static int ss_des_cbc_decrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req,
		SS_DIR_DECRYPT, SS_METHOD_DES, SS_AES_MODE_CBC);
}

static int ss_des3_ecb_encrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req,
		SS_DIR_ENCRYPT, SS_METHOD_3DES, SS_AES_MODE_ECB);
}

static int ss_des3_ecb_decrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req,
		SS_DIR_DECRYPT, SS_METHOD_3DES, SS_AES_MODE_ECB);
}

static int ss_des3_cbc_encrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req,
		SS_DIR_ENCRYPT, SS_METHOD_3DES, SS_AES_MODE_CBC);
}

static int ss_des3_cbc_decrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req,
		SS_DIR_DECRYPT, SS_METHOD_3DES, SS_AES_MODE_CBC);
}

#ifdef SS_RSA_ENABLE
static int ss_rsa_encrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req, SS_DIR_ENCRYPT,
		SS_METHOD_RSA, CE_RSA_OP_M_EXP);
}

static int ss_rsa_decrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req, SS_DIR_DECRYPT,
		SS_METHOD_RSA, CE_RSA_OP_M_EXP);
}
#endif

#ifdef SS_DH_ENABLE
static int ss_dh_encrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req, SS_DIR_ENCRYPT,
		SS_METHOD_DH, CE_RSA_OP_M_EXP);
}

static int ss_dh_decrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req, SS_DIR_DECRYPT,
		SS_METHOD_DH, CE_RSA_OP_M_EXP);
}
#endif

#ifdef SS_ECC_ENABLE
static int ss_ecdh_encrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req, SS_DIR_ENCRYPT, SS_METHOD_ECC,
				CE_ECC_OP_POINT_MUL);
}

static int ss_ecdh_decrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req, SS_DIR_DECRYPT, SS_METHOD_ECC,
				CE_ECC_OP_POINT_MUL);
}

static int ss_ecc_sign_encrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req, SS_DIR_ENCRYPT, SS_METHOD_ECC, CE_ECC_OP_SIGN);
}

static int ss_ecc_sign_decrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req, SS_DIR_DECRYPT, SS_METHOD_ECC, CE_ECC_OP_SIGN);
}

static int ss_ecc_verify_encrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req, SS_DIR_ENCRYPT, SS_METHOD_RSA,
				CE_RSA_OP_M_MUL);
}

static int ss_ecc_verify_decrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req, SS_DIR_DECRYPT, SS_METHOD_RSA,
				CE_RSA_OP_M_MUL);
}

#endif

#ifdef SS_HMAC_SHA1_ENABLE
static int ss_hmac_sha1_encrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req,
		SS_DIR_ENCRYPT, SS_METHOD_HMAC_SHA1, SS_AES_MODE_ECB);
}
#endif

#ifdef SS_HMAC_SHA256_ENABLE
static int ss_hmac_sha256_encrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req,
		SS_DIR_ENCRYPT, SS_METHOD_HMAC_SHA256, SS_AES_MODE_ECB);
}
#endif

int ss_rng_reset(struct crypto_rng *tfm, const u8 *seed, u32 slen)
{
	int len = slen > SS_PRNG_SEED_LEN ? SS_PRNG_SEED_LEN : slen;
	ss_aes_ctx_t *ctx = crypto_rng_ctx(tfm);

	SS_DBG("Seed len: %d/%d, flags = %#x\n", len, slen, ctx->comm.flags);
	ctx->key_size = len;
	memset(ctx->key, 0, SS_PRNG_SEED_LEN);
	memcpy(ctx->key, seed, len);
	ctx->comm.flags |= SS_FLAG_NEW_KEY;

	return 0;
}

#ifdef SS_DRBG_MODE_ENABLE
int ss_drbg_reset(struct crypto_rng *tfm, const u8 *seed, u32 slen)
{
	int len = slen > SS_PRNG_SEED_LEN ? SS_PRNG_SEED_LEN : slen;
	ss_drbg_ctx_t *ctx = crypto_rng_ctx(tfm);

	SS_DBG("Seed len: %d/%d, flags = %#x\n", len, slen, ctx->comm.flags);
	ctx->person_size = len;
	memset(ctx->person, 0, SS_PRNG_SEED_LEN);
	memcpy(ctx->person, seed, slen);
	ctx->comm.flags |= SS_FLAG_NEW_KEY;

	return 0;
}

void ss_drbg_set_ent(struct crypto_rng *tfm, const u8 *entropy, u32 entropy_len)
{
	int len = entropy_len > SS_PRNG_SEED_LEN ? SS_PRNG_SEED_LEN : entropy_len;
	ss_drbg_ctx_t *ctx = crypto_rng_ctx(tfm);

	SS_DBG("Seed len: %d / %d, flags = %#x\n", len, entropy_len, ctx->comm.flags);
	ctx->entropt_size = entropy_len;
	memset(ctx->entropt, 0, SS_PRNG_SEED_LEN);
	memcpy(ctx->entropt, entropy, len);
	ctx->comm.flags |= SS_FLAG_NEW_KEY;
}
#endif

int ss_flow_request(ss_comm_ctx_t *comm)
{
	int i;
	unsigned long flags = 0;

	spin_lock_irqsave(&ss_dev->lock, flags);
	for (i = 0; i < SS_FLOW_NUM; i++) {
		if (ss_dev->flows[i].available == SS_FLOW_AVAILABLE) {
			comm->flow = i;
			ss_dev->flows[i].available = SS_FLOW_UNAVAILABLE;
			SS_DBG("The flow %d is available.\n", i);
			break;
		}
	}
	spin_unlock_irqrestore(&ss_dev->lock, flags);

	if (i == SS_FLOW_NUM) {
		SS_ERR("Failed to get an available flow.\n");
		i = -1;
	}
	return i;
}

void ss_flow_release(ss_comm_ctx_t *comm)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&ss_dev->lock, flags);
	ss_dev->flows[comm->flow].available = SS_FLOW_AVAILABLE;
	spin_unlock_irqrestore(&ss_dev->lock, flags);
}

#ifdef SS_GCM_MODE_ENABLE
static int sunxi_aes_gcm_init(struct crypto_aead *tfm)
{
	if (ss_flow_request(crypto_aead_ctx(tfm)) < 0)
		return -1;

	crypto_aead_set_reqsize(tfm, sizeof(ss_aes_req_ctx_t));
	SS_DBG("reqsize = %d\n", tfm->reqsize);

	return 0;
}

static void sunxi_aes_gcm_exit(struct crypto_aead *tfm)
{
	SS_ENTER();
	ss_flow_release(crypto_aead_ctx(tfm));
	/* sun8iw6 and sun9iw1 need reset SS controller after each operation. */
#ifdef SS_IDMA_ENABLE
	ss_reset();
#endif

}
#endif

static int sunxi_ss_cra_init(struct crypto_tfm *tfm)
{
	if (ss_flow_request(crypto_tfm_ctx(tfm)) < 0)
		return -1;

	tfm->crt_ablkcipher.reqsize = sizeof(ss_aes_req_ctx_t);
	SS_DBG("reqsize = %d\n", tfm->crt_u.ablkcipher.reqsize);
	return 0;
}

static int sunxi_ss_cra_rng_init(struct crypto_tfm *tfm)
{
	if (ss_flow_request(crypto_tfm_ctx(tfm)) < 0)
		return -1;

	return 0;
}

static int sunxi_ss_cra_hash_init(struct crypto_tfm *tfm)
{
	if (ss_flow_request(crypto_tfm_ctx(tfm)) < 0)
		return -1;

	crypto_ahash_set_reqsize(__crypto_ahash_cast(tfm),
				 sizeof(ss_aes_req_ctx_t));

	SS_DBG("reqsize = %zu\n", sizeof(ss_aes_req_ctx_t));
	return 0;
}

static void sunxi_ss_cra_exit(struct crypto_tfm *tfm)
{
	SS_ENTER();
	ss_flow_release(crypto_tfm_ctx(tfm));
	/* sun8iw6 and sun9iw1 need reset SS controller after each operation. */
#ifdef SS_IDMA_ENABLE
	ss_reset();
#endif
}

static int ss_hash_init(struct ahash_request *req, int type, int size, char *iv)
{
	ss_aes_req_ctx_t *req_ctx = ahash_request_ctx(req);
	ss_hash_ctx_t *ctx = crypto_ahash_ctx(crypto_ahash_reqtfm(req));

	SS_DBG("Method: %d\n", type);

	memset(req_ctx, 0, sizeof(ss_aes_req_ctx_t));
	req_ctx->type = type;

	ctx->md_size = size;
	memcpy(ctx->md, iv, size);

	ctx->cnt = 0;
	memset(ctx->pad, 0, SS_HASH_PAD_SIZE);
	return 0;
}

static int ss_md5_init(struct ahash_request *req)
{
	int iv[MD5_DIGEST_SIZE/4] = {SHA1_H0, SHA1_H1, SHA1_H2, SHA1_H3};

	return ss_hash_init(req, SS_METHOD_MD5, MD5_DIGEST_SIZE, (char *)iv);
}

static int ss_sha1_init(struct ahash_request *req)
{
	int iv[SHA1_DIGEST_SIZE/4] = {
			SHA1_H0, SHA1_H1, SHA1_H2, SHA1_H3, SHA1_H4};

#ifdef SS_SHA_SWAP_PRE_ENABLE
#ifdef SS_SHA_NO_SWAP_IV4
	ss_hash_swap((char *)iv, SHA1_DIGEST_SIZE - 4);
#else
	ss_hash_swap((char *)iv, SHA1_DIGEST_SIZE);
#endif
#endif

	return ss_hash_init(req, SS_METHOD_SHA1, SHA1_DIGEST_SIZE, (char *)iv);
}

#ifdef SS_SHA224_ENABLE
static int ss_sha224_init(struct ahash_request *req)
{
	int iv[SHA256_DIGEST_SIZE/4] = {
			SHA224_H0, SHA224_H1, SHA224_H2, SHA224_H3,
			SHA224_H4, SHA224_H5, SHA224_H6, SHA224_H7};

#ifdef SS_SHA_SWAP_PRE_ENABLE
	ss_hash_swap((char *)iv, SHA256_DIGEST_SIZE);
#endif

	return ss_hash_init(req,
		SS_METHOD_SHA224, SHA256_DIGEST_SIZE, (char *)iv);
}
#endif

#ifdef SS_SHA256_ENABLE
static int ss_sha256_init(struct ahash_request *req)
{
	int iv[SHA256_DIGEST_SIZE/4] = {
			SHA256_H0, SHA256_H1, SHA256_H2, SHA256_H3,
			SHA256_H4, SHA256_H5, SHA256_H6, SHA256_H7};

#ifdef SS_SHA_SWAP_PRE_ENABLE
	ss_hash_swap((char *)iv, SHA256_DIGEST_SIZE);
#endif

	return ss_hash_init(req,
			SS_METHOD_SHA256, SHA256_DIGEST_SIZE, (char *)iv);
}
#endif

#define GET_U64_HIGH(data64)	(int)(data64 >> 32)
#define GET_U64_LOW(data64)		(int)(data64 & 0xFFFFFFFF)

#ifdef SS_SHA384_ENABLE
static int ss_sha384_init(struct ahash_request *req)
{
	int iv[SHA512_DIGEST_SIZE/4] = {
			GET_U64_HIGH(SHA384_H0), GET_U64_LOW(SHA384_H0),
			GET_U64_HIGH(SHA384_H1), GET_U64_LOW(SHA384_H1),
			GET_U64_HIGH(SHA384_H2), GET_U64_LOW(SHA384_H2),
			GET_U64_HIGH(SHA384_H3), GET_U64_LOW(SHA384_H3),
			GET_U64_HIGH(SHA384_H4), GET_U64_LOW(SHA384_H4),
			GET_U64_HIGH(SHA384_H5), GET_U64_LOW(SHA384_H5),
			GET_U64_HIGH(SHA384_H6), GET_U64_LOW(SHA384_H6),
			GET_U64_HIGH(SHA384_H7), GET_U64_LOW(SHA384_H7)};

#ifdef SS_SHA_SWAP_PRE_ENABLE
	ss_hash_swap((char *)iv, SHA512_DIGEST_SIZE);
#endif

	return ss_hash_init(req,
			SS_METHOD_SHA384, SHA512_DIGEST_SIZE, (char *)iv);
}
#endif

#ifdef SS_SHA512_ENABLE
static int ss_sha512_init(struct ahash_request *req)
{
	int iv[SHA512_DIGEST_SIZE/4] = {
			GET_U64_HIGH(SHA512_H0), GET_U64_LOW(SHA512_H0),
			GET_U64_HIGH(SHA512_H1), GET_U64_LOW(SHA512_H1),
			GET_U64_HIGH(SHA512_H2), GET_U64_LOW(SHA512_H2),
			GET_U64_HIGH(SHA512_H3), GET_U64_LOW(SHA512_H3),
			GET_U64_HIGH(SHA512_H4), GET_U64_LOW(SHA512_H4),
			GET_U64_HIGH(SHA512_H5), GET_U64_LOW(SHA512_H5),
			GET_U64_HIGH(SHA512_H6), GET_U64_LOW(SHA512_H6),
			GET_U64_HIGH(SHA512_H7), GET_U64_LOW(SHA512_H7)};

#ifdef SS_SHA_SWAP_PRE_ENABLE
	ss_hash_swap((char *)iv, SHA512_DIGEST_SIZE);
#endif

	return ss_hash_init(req,
			SS_METHOD_SHA512, SHA512_DIGEST_SIZE, (char *)iv);
}
#endif

#define DES_MIN_KEY_SIZE	DES_KEY_SIZE
#define DES_MAX_KEY_SIZE	DES_KEY_SIZE
#define DES3_MIN_KEY_SIZE	DES3_EDE_KEY_SIZE
#define DES3_MAX_KEY_SIZE	DES3_EDE_KEY_SIZE

#define DECLARE_SS_AES_ALG(utype, ltype, lmode, block_size, iv_size) \
{ \
	.cra_name	 = #lmode"("#ltype")", \
	.cra_driver_name = "ss-"#lmode"-"#ltype, \
	.cra_flags	 = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC, \
	.cra_type	 = &crypto_ablkcipher_type, \
	.cra_blocksize	 = block_size, \
	.cra_alignmask	 = 3, \
	.cra_u.ablkcipher = { \
		.setkey      = ss_aes_setkey, \
		.encrypt     = ss_##ltype##_##lmode##_encrypt, \
		.decrypt     = ss_##ltype##_##lmode##_decrypt, \
		.min_keysize = utype##_MIN_KEY_SIZE, \
		.max_keysize = utype##_MAX_KEY_SIZE, \
		.ivsize	     = iv_size, \
	} \
}

#ifdef SS_XTS_MODE_ENABLE
#define DECLARE_SS_AES_XTS_ALG(utype, ltype, lmode, block_size, iv_size) \
{ \
	.cra_name	 = #lmode"("#ltype")", \
	.cra_driver_name = "ss-"#lmode"-"#ltype, \
	.cra_flags	 = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC, \
	.cra_type	 = &crypto_ablkcipher_type, \
	.cra_blocksize	 = block_size, \
	.cra_alignmask	 = 3, \
	.cra_u.ablkcipher = { \
		.setkey      = ss_aes_setkey, \
		.encrypt     = ss_##ltype##_##lmode##_encrypt, \
		.decrypt     = ss_##ltype##_##lmode##_decrypt, \
		.min_keysize = utype##_MAX_KEY_SIZE, \
		.max_keysize = utype##_MAX_KEY_SIZE * 2, \
		.ivsize	     = iv_size, \
	} \
}
#endif

#define DECLARE_SS_ASYM_ALG(type, bitwidth, key_size, iv_size) \
{ \
	.cra_name	 = #type"("#bitwidth")", \
	.cra_driver_name = "ss-"#type"-"#bitwidth, \
	.cra_flags	 = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC, \
	.cra_type	 = &crypto_ablkcipher_type, \
	.cra_blocksize	 = key_size%AES_BLOCK_SIZE == 0 ? AES_BLOCK_SIZE : 4, \
	.cra_alignmask	 = key_size%AES_BLOCK_SIZE == 0 ? 31 : 3, \
	.cra_u.ablkcipher = { \
		.setkey      = ss_aes_setkey, \
		.encrypt     = ss_##type##_encrypt, \
		.decrypt     = ss_##type##_decrypt, \
		.min_keysize = key_size, \
		.max_keysize = key_size, \
		.ivsize      = iv_size, \
	}, \
}
#ifndef SS_SUPPORT_CE_V3_2
#define DECLARE_SS_RSA_ALG(type, bitwidth) \
		DECLARE_SS_ASYM_ALG(type, bitwidth, (bitwidth/8), (bitwidth/8))
#else
#define DECLARE_SS_RSA_ALG(type, bitwidth) \
		DECLARE_SS_ASYM_ALG(type, bitwidth, (bitwidth/8), 0)
#endif
#define DECLARE_SS_DH_ALG(type, bitwidth) DECLARE_SS_RSA_ALG(type, bitwidth)

#ifdef SS_GCM_MODE_ENABLE
static struct aead_alg sunxi_aes_gcm_alg = {
	.setkey		= sunxi_aes_gcm_setkey,
	.setauthsize	= sunxi_aes_gcm_setauthsize,
	.encrypt	= sunxi_aes_gcm_encrypt,
	.decrypt	= sunxi_aes_gcm_decrypt,
	.init		= sunxi_aes_gcm_init,
	.exit		= sunxi_aes_gcm_exit,
	.ivsize		= AES_MIN_KEY_SIZE,
	.maxauthsize	= AES_BLOCK_SIZE,

	.base = {
		.cra_name		= "gcm(aes)",
		.cra_driver_name	= "ss-gcm-aes",
		.cra_priority		= SS_ALG_PRIORITY,
		.cra_flags		= CRYPTO_ALG_ASYNC,
		.cra_blocksize		= AES_BLOCK_SIZE,
		.cra_ctxsize		= sizeof(ss_aead_ctx_t),
		.cra_alignmask		= 31,
		.cra_module		= THIS_MODULE,
	},
};
#endif

static struct crypto_alg sunxi_ss_algs[] = {
	DECLARE_SS_AES_ALG(AES, aes, ecb, AES_BLOCK_SIZE, 0),
	DECLARE_SS_AES_ALG(AES, aes, cbc, AES_BLOCK_SIZE, AES_MIN_KEY_SIZE),
#ifdef SS_CTR_MODE_ENABLE
	DECLARE_SS_AES_ALG(AES, aes, ctr, AES_BLOCK_SIZE, AES_MIN_KEY_SIZE),
#endif
#ifdef SS_CTS_MODE_ENABLE
	DECLARE_SS_AES_ALG(AES, aes, cts, AES_BLOCK_SIZE, AES_MIN_KEY_SIZE),
#endif
#ifdef SS_XTS_MODE_ENABLE
	DECLARE_SS_AES_XTS_ALG(AES, aes, xts, AES_BLOCK_SIZE, AES_MIN_KEY_SIZE),
#endif
#ifdef SS_OFB_MODE_ENABLE
	DECLARE_SS_AES_ALG(AES, aes, ofb, AES_BLOCK_SIZE, AES_MIN_KEY_SIZE),
#endif
#ifdef SS_CFB_MODE_ENABLE
	DECLARE_SS_AES_ALG(AES, aes, cfb1, AES_BLOCK_SIZE, AES_MIN_KEY_SIZE),
	DECLARE_SS_AES_ALG(AES, aes, cfb8, AES_BLOCK_SIZE, AES_MIN_KEY_SIZE),
	DECLARE_SS_AES_ALG(AES, aes, cfb64, AES_BLOCK_SIZE, AES_MIN_KEY_SIZE),
	DECLARE_SS_AES_ALG(AES, aes, cfb128, AES_BLOCK_SIZE, AES_MIN_KEY_SIZE),
#endif
	DECLARE_SS_AES_ALG(DES, des, ecb, DES_BLOCK_SIZE, 0),
	DECLARE_SS_AES_ALG(DES, des, cbc, DES_BLOCK_SIZE, DES_KEY_SIZE),
	DECLARE_SS_AES_ALG(DES3, des3, ecb, DES3_EDE_BLOCK_SIZE, 0),
	DECLARE_SS_AES_ALG(DES3, des3, cbc, DES3_EDE_BLOCK_SIZE, DES_KEY_SIZE),
#ifdef SS_RSA512_ENABLE
	DECLARE_SS_RSA_ALG(rsa, 512),
#endif
#ifdef SS_RSA1024_ENABLE
	DECLARE_SS_RSA_ALG(rsa, 1024),
#endif
#ifdef SS_RSA2048_ENABLE
	DECLARE_SS_RSA_ALG(rsa, 2048),
#endif
#ifdef SS_RSA3072_ENABLE
	DECLARE_SS_RSA_ALG(rsa, 3072),
#endif
#ifdef SS_RSA4096_ENABLE
	DECLARE_SS_RSA_ALG(rsa, 4096),
#endif
#ifdef SS_DH512_ENABLE
	DECLARE_SS_DH_ALG(dh, 512),
#endif
#ifdef SS_DH1024_ENABLE
	DECLARE_SS_DH_ALG(dh, 1024),
#endif
#ifdef SS_DH2048_ENABLE
	DECLARE_SS_DH_ALG(dh, 2048),
#endif
#ifdef SS_DH3072_ENABLE
	DECLARE_SS_DH_ALG(dh, 3072),
#endif
#ifdef SS_DH4096_ENABLE
	DECLARE_SS_DH_ALG(dh, 4096),
#endif
#ifdef SS_ECC_ENABLE
#ifndef SS_SUPPORT_CE_V3_2
	DECLARE_SS_ASYM_ALG(ecdh, 160, 160/8, 160/8),
	DECLARE_SS_ASYM_ALG(ecdh, 224, 224/8, 224/8),
	DECLARE_SS_ASYM_ALG(ecdh, 256, 256/8, 256/8),
	DECLARE_SS_ASYM_ALG(ecdh, 521, ((521+31)/32)*4, ((521+31)/32)*4),
	DECLARE_SS_ASYM_ALG(ecc_sign, 160, 160/8, (160/8)*2),
	DECLARE_SS_ASYM_ALG(ecc_sign, 224, 224/8, (224/8)*2),
	DECLARE_SS_ASYM_ALG(ecc_sign, 256, 256/8, (256/8)*2),
	DECLARE_SS_ASYM_ALG(ecc_sign, 521, ((521+31)/32)*4, ((521+31)/32)*4*2),
#else
	DECLARE_SS_ASYM_ALG(ecdh, 160, 160/8, 0),
	DECLARE_SS_ASYM_ALG(ecdh, 224, 224/8, 0),
	DECLARE_SS_ASYM_ALG(ecdh, 256, 256/8, 0),
	DECLARE_SS_ASYM_ALG(ecdh, 521, ((521+31)/32)*4, 0),
	DECLARE_SS_ASYM_ALG(ecc_sign, 160, 160/8, 0),
	DECLARE_SS_ASYM_ALG(ecc_sign, 224, 224/8, 0),
	DECLARE_SS_ASYM_ALG(ecc_sign, 256, 256/8, 0),
	DECLARE_SS_ASYM_ALG(ecc_sign, 521, ((521+31)/32)*4, 0),
#endif
	DECLARE_SS_RSA_ALG(ecc_verify, 512),
	DECLARE_SS_RSA_ALG(ecc_verify, 1024),
#endif

#ifdef SS_HMAC_SHA1_ENABLE
	{
		.cra_name	 = "hmac-sha1",
		.cra_driver_name = "ss-hmac-sha1",
		.cra_flags	= CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_type	 = &crypto_ablkcipher_type,
		.cra_blocksize	 = 4,
		.cra_alignmask	 = 3,
		.cra_u.ablkcipher = {
			.setkey	     = ss_aes_setkey,
			.encrypt     = ss_hmac_sha1_encrypt,
			.decrypt     = NULL,
			.min_keysize = SHA1_BLOCK_SIZE,
			.max_keysize = SHA1_BLOCK_SIZE,
			.ivsize	     = 0,
		}
	},
#endif
#ifdef SS_HMAC_SHA256_ENABLE
	{
		.cra_name	 = "hmac-sha256",
		.cra_driver_name = "ss-hmac-sha256",
		.cra_flags	 = CRYPTO_ALG_TYPE_ABLKCIPHER|CRYPTO_ALG_ASYNC,
		.cra_type	 = &crypto_ablkcipher_type,
		.cra_blocksize	 = 4,
		.cra_alignmask	 = 3,
		.cra_u.ablkcipher = {
			.setkey      = ss_aes_setkey,
			.encrypt     = ss_hmac_sha256_encrypt,
			.decrypt     = NULL,
			.min_keysize = SHA256_BLOCK_SIZE,
			.max_keysize = SHA256_BLOCK_SIZE,
			.ivsize	     = 0,
		}
	},
#endif
};

#define DECLARE_SS_RNG_ALG(ltype) \
{ \
	.generate = ss_##ltype##_get_random, \
	.seed	  = ss_rng_reset, \
	.seedsize = SS_SEED_SIZE, \
	.base	  = { \
		.cra_name = #ltype, \
		.cra_driver_name = "ss-"#ltype, \
		.cra_flags = CRYPTO_ALG_TYPE_RNG, \
		.cra_priority = SS_ALG_PRIORITY, \
		.cra_ctxsize = sizeof(ss_aes_ctx_t), \
		.cra_module = THIS_MODULE, \
		.cra_init = sunxi_ss_cra_rng_init, \
		.cra_exit = sunxi_ss_cra_exit, \
	} \
}

#ifdef SS_DRBG_MODE_ENABLE
#define DECLARE_SS_DRBG_ALG(ltype) \
{ \
	.generate = ss_drbg_##ltype##_get_random, \
	.seed	  = ss_drbg_reset, \
	.set_ent	= ss_drbg_set_ent, \
	.seedsize = SS_SEED_SIZE, \
	.base	  = { \
		.cra_name = "drbg-"#ltype, \
		.cra_driver_name = "ss-drbg-"#ltype, \
		.cra_flags = CRYPTO_ALG_TYPE_RNG, \
		.cra_priority = SS_ALG_PRIORITY, \
		.cra_ctxsize = sizeof(ss_drbg_ctx_t), \
		.cra_module = THIS_MODULE, \
		.cra_init = sunxi_ss_cra_rng_init, \
		.cra_exit = sunxi_ss_cra_exit, \
	} \
}
#endif

static struct rng_alg sunxi_ss_algs_rng[] = {
#ifdef SS_TRNG_ENABLE
	DECLARE_SS_RNG_ALG(trng),
#endif
	DECLARE_SS_RNG_ALG(prng),
#ifdef SS_DRBG_MODE_ENABLE
	DECLARE_SS_DRBG_ALG(sha1),
	DECLARE_SS_DRBG_ALG(sha256),
	DECLARE_SS_DRBG_ALG(sha512),
#endif
};

#define MD5_BLOCK_SIZE	MD5_HMAC_BLOCK_SIZE
#define sha224_state   sha256_state
#define sha384_state   sha512_state
#define DECLARE_SS_AHASH_ALG(ltype, utype) \
	{ \
		.init		= ss_##ltype##_init, \
		.update		= ss_hash_update, \
		.final		= ss_hash_final, \
		.finup		= ss_hash_finup, \
		.digest		= ss_hash_digest, \
		.halg = { \
			.digestsize = utype##_DIGEST_SIZE, \
			.statesize = sizeof(struct ltype##_state), \
			.base	= { \
			.cra_name        = #ltype, \
			.cra_driver_name = "ss-"#ltype, \
			.cra_priority = SS_ALG_PRIORITY, \
			.cra_flags  = CRYPTO_ALG_TYPE_AHASH|CRYPTO_ALG_ASYNC, \
			.cra_blocksize	 = utype##_BLOCK_SIZE, \
			.cra_ctxsize	 = sizeof(ss_hash_ctx_t), \
			.cra_alignmask	 = 3, \
			.cra_module	 = THIS_MODULE, \
			.cra_init	 = sunxi_ss_cra_hash_init, \
			.cra_exit	 = sunxi_ss_cra_exit, \
			} \
		} \
	}

static struct ahash_alg sunxi_ss_algs_hash[] = {
	DECLARE_SS_AHASH_ALG(md5, MD5),
	DECLARE_SS_AHASH_ALG(sha1, SHA1),
#ifdef SS_SHA224_ENABLE
	DECLARE_SS_AHASH_ALG(sha224, SHA224),
#endif
#ifdef SS_SHA256_ENABLE
	DECLARE_SS_AHASH_ALG(sha256, SHA256),
#endif
#ifdef SS_SHA384_ENABLE
	DECLARE_SS_AHASH_ALG(sha384, SHA384),
#endif
#ifdef SS_SHA512_ENABLE
	DECLARE_SS_AHASH_ALG(sha512, SHA512),
#endif
};

/* Requeset the resource: IRQ, mem */
static int sunxi_ss_res_request(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *pnode = pdev->dev.of_node;
	sunxi_ss_t *sss = platform_get_drvdata(pdev);
#ifdef SS_IDMA_ENABLE
	int i;

	for (i = 0; i < SS_FLOW_NUM; i++) {
		sss->flows[i].buf_src =	kmalloc(SS_DMA_BUF_SIZE, GFP_KERNEL);
		if (sss->flows[i].buf_src == NULL) {
			SS_ERR("Can not allocate DMA source buffer\n");
			return -ENOMEM;
		}
		sss->flows[i].buf_src_dma = virt_to_phys(sss->flows[i].buf_src);

		sss->flows[i].buf_dst = kmalloc(SS_DMA_BUF_SIZE, GFP_KERNEL);
		if (sss->flows[i].buf_dst == NULL) {
			SS_ERR("Can not allocate DMA source buffer\n");
			return -ENOMEM;
		}
		sss->flows[i].buf_dst_dma = virt_to_phys(sss->flows[i].buf_dst);
		init_completion(&sss->flows[i].done);
	}
#endif
	sss->irq = irq_of_parse_and_map(pnode, SS_RES_INDEX);
	if (sss->irq == 0) {
		SS_ERR("Failed to get the SS IRQ.\n");
		return -EINVAL;
	}

	ret = request_irq(sss->irq,
			sunxi_ss_irq_handler, 0, sss->dev_name, sss);
	if (ret != 0) {
		SS_ERR("Cannot request IRQ\n");
		return ret;
	}

#ifdef CONFIG_OF
	sss->base_addr = of_iomap(pnode, SS_RES_INDEX);
	if (sss->base_addr == NULL) {
		SS_ERR("Unable to remap IO\n");
		return -ENXIO;
	}
#endif

	return 0;
}

/* Release the resource: IRQ, mem */
static int sunxi_ss_res_release(sunxi_ss_t *sss)
{
#ifdef SS_IDMA_ENABLE
	int i;
#endif

	iounmap(sss->base_addr);

#ifdef SS_IDMA_ENABLE
	for (i = 0; i < SS_FLOW_NUM; i++) {
		kfree(sss->flows[i].buf_src);
		kfree(sss->flows[i].buf_dst);
	}
#endif

	free_irq(sss->irq, sss);
	return 0;
}


static int sunxi_ss_hw_init(sunxi_ss_t *sss)
{
#ifdef CONFIG_EVB_PLATFORM
	int ret = 0;
#endif
	struct clk *pclk = NULL;
	struct device_node *pnode = sss->pdev->dev.of_node;

	pclk = of_clk_get(pnode, 1);
	if (IS_ERR_OR_NULL(pclk)) {
		SS_ERR("Unable to get pll clock, return %x\n", PTR_RET(pclk));
		return PTR_RET(pclk);
	}

	sss->mclk = of_clk_get(pnode, 0);
	if (IS_ERR_OR_NULL(sss->mclk)) {
		SS_ERR("Fail to get module clk, ret %x\n", PTR_RET(sss->mclk));
		return PTR_RET(sss->mclk);
	}

#ifdef SS_RSA_CLK_ENABLE
	if (of_property_read_u32_array(pnode, "clock-frequency",
		&sss->gen_clkrate, 2)) {
#else
	if (of_property_read_u32(pnode, "clock-frequency", &sss->gen_clkrate)) {
#endif
		SS_ERR("Unable to get clock-frequency.\n");
		return -EINVAL;
	}
	SS_DBG("The clk freq: %d, %d\n", sss->gen_clkrate, sss->rsa_clkrate);

#ifdef CONFIG_EVB_PLATFORM
	ret = clk_set_parent(sss->mclk, pclk);
	if (ret != 0) {
		SS_ERR("clk_set_parent() failed! return %d\n", ret);
		return ret;
	}

	ret = clk_set_rate(sss->mclk, sss->gen_clkrate);
	if (ret != 0) {
		SS_ERR("Set rate(%d) failed! ret %d\n", sss->gen_clkrate, ret);
		return ret;
	}
#endif
	SS_DBG("SS mclk %luMHz, pclk %luMHz\n", clk_get_rate(sss->mclk)/1000000,
			clk_get_rate(pclk)/1000000);

	if (clk_prepare_enable(sss->mclk)) {
		SS_ERR("Couldn't enable module clock\n");
		return -EBUSY;
	}

	clk_put(pclk);
	return 0;
}

static int sunxi_ss_hw_exit(sunxi_ss_t *sss)
{
	clk_disable_unprepare(sss->mclk);
	clk_put(sss->mclk);
	sss->mclk = NULL;
	return 0;
}

static int sunxi_ss_alg_register(void)
{
	int i;
	int ret = 0;

	for (i = 0; i < ARRAY_SIZE(sunxi_ss_algs); i++) {
		INIT_LIST_HEAD(&sunxi_ss_algs[i].cra_list);
		SS_DBG("Add %s...\n", sunxi_ss_algs[i].cra_name);
		sunxi_ss_algs[i].cra_priority = SS_ALG_PRIORITY;
		sunxi_ss_algs[i].cra_ctxsize = sizeof(ss_aes_ctx_t);
		sunxi_ss_algs[i].cra_module = THIS_MODULE;
		sunxi_ss_algs[i].cra_exit = sunxi_ss_cra_exit;
		sunxi_ss_algs[i].cra_init = sunxi_ss_cra_init;

		ret = crypto_register_alg(&sunxi_ss_algs[i]);
		if (ret != 0) {
			SS_ERR("crypto_register_alg(%s) failed! return %d\n",
				sunxi_ss_algs[i].cra_name, ret);
			return ret;
		}
	}

	for (i = 0; i < ARRAY_SIZE(sunxi_ss_algs_rng); i++) {
		SS_DBG("Add %s...\n", sunxi_ss_algs_rng[i].base.cra_name);
		ret = crypto_register_rng(&sunxi_ss_algs_rng[i]);
		if (ret != 0) {
			SS_ERR("crypto_register_rng(%s) failed! return %d\n",
				sunxi_ss_algs_rng[i].base.cra_name, ret);
			return ret;
		}
	}

	for (i = 0; i < ARRAY_SIZE(sunxi_ss_algs_hash); i++) {
		SS_DBG("Add %s...\n", sunxi_ss_algs_hash[i].halg.base.cra_name);
		ret = crypto_register_ahash(&sunxi_ss_algs_hash[i]);
		if (ret != 0) {
			SS_ERR("crypto_register_ahash(%s) failed! return %d\n",
				sunxi_ss_algs_hash[i].halg.base.cra_name, ret);
			return ret;
		}
	}

#ifdef SS_GCM_MODE_ENABLE
	ret = crypto_register_aead(&sunxi_aes_gcm_alg);
	if (ret != 0) {
		SS_ERR("crypto_register_aead(%s) failed! return %d\n",
			sunxi_aes_gcm_alg.base.cra_name, ret);
		return ret;
	}
#endif

	return 0;
}

static void sunxi_ss_alg_unregister(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sunxi_ss_algs); i++)
		crypto_unregister_alg(&sunxi_ss_algs[i]);

	for (i = 0; i < ARRAY_SIZE(sunxi_ss_algs_rng); i++)
		crypto_unregister_rng(&sunxi_ss_algs_rng[i]);

	for (i = 0; i < ARRAY_SIZE(sunxi_ss_algs_hash); i++)
		crypto_unregister_ahash(&sunxi_ss_algs_hash[i]);
}

static ssize_t sunxi_ss_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev =
		container_of(dev, struct platform_device, dev);
	sunxi_ss_t *sss = platform_get_drvdata(pdev);

	return snprintf(buf, PAGE_SIZE,
		"pdev->id   = %d\n"
		"pdev->name = %s\n"
		"pdev->num_resources = %u\n"
		"pdev->resource.irq = %d\n"
		"SS module clk rate = %ld Mhz\n"
		"IO membase = 0x%p\n",
		pdev->id, pdev->name, pdev->num_resources,
		sss->irq,
		(clk_get_rate(sss->mclk)/1000000), sss->base_addr);
}
static struct device_attribute sunxi_ss_info_attr =
	__ATTR(info, S_IRUGO, sunxi_ss_info_show, NULL);

static ssize_t sunxi_ss_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i;
	struct platform_device *pdev =
			container_of(dev, struct platform_device, dev);
	sunxi_ss_t *sss = platform_get_drvdata(pdev);
	static char const *avail[] = {"Available", "Unavailable"};

	if (sss == NULL)
		return snprintf(buf, PAGE_SIZE, "%s\n", "sunxi_ss is NULL!");

	buf[0] = 0;
	for (i = 0; i < SS_FLOW_NUM; i++) {
		snprintf(buf+strlen(buf), PAGE_SIZE-strlen(buf),
			"The flow %d state: %s\n"
#ifdef SS_IDMA_ENABLE
			"    Src: 0x%p / 0x%08x\n"
			"    Dst: 0x%p / 0x%08x\n"
#endif
			, i, avail[sss->flows[i].available]
#ifdef SS_IDMA_ENABLE
			, sss->flows[i].buf_src, sss->flows[i].buf_src_dma
			, sss->flows[i].buf_dst, sss->flows[i].buf_dst_dma
#endif
		);
	}

	return strlen(buf)
		+ ss_reg_print(buf + strlen(buf), PAGE_SIZE - strlen(buf));
}

static struct device_attribute sunxi_ss_status_attr =
	__ATTR(status, S_IRUGO, sunxi_ss_status_show, NULL);

static void sunxi_ss_sysfs_create(struct platform_device *_pdev)
{
	device_create_file(&_pdev->dev, &sunxi_ss_info_attr);
	device_create_file(&_pdev->dev, &sunxi_ss_status_attr);
}

static void sunxi_ss_sysfs_remove(struct platform_device *_pdev)
{
	device_remove_file(&_pdev->dev, &sunxi_ss_info_attr);
	device_remove_file(&_pdev->dev, &sunxi_ss_status_attr);
}

static u64 sunxi_ss_dma_mask = DMA_BIT_MASK(64);

static int sunxi_ss_probe(struct platform_device *pdev)
{
	int ret = 0;
	sunxi_ss_t *sss = NULL;

	sss = devm_kzalloc(&pdev->dev, sizeof(sunxi_ss_t), GFP_KERNEL);
	if (sss == NULL) {
		SS_ERR("Unable to allocate sunxi_ss_t\n");
		return -ENOMEM;
	}

#ifdef CONFIG_OF
	pdev->dev.dma_mask = &sunxi_ss_dma_mask;
	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
#endif

	snprintf(sss->dev_name, sizeof(sss->dev_name), SUNXI_SS_DEV_NAME);
	platform_set_drvdata(pdev, sss);

	ret = sunxi_ss_res_request(pdev);
	if (ret != 0)
		goto err0;

	sss->pdev = pdev;

	ret = sunxi_ss_hw_init(sss);
	if (ret != 0) {
		SS_ERR("SS hw init failed!\n");
		goto err1;
	}

	ss_dev = sss;
	ret = sunxi_ss_alg_register();
	if (ret != 0) {
		SS_ERR("sunxi_ss_alg_register() failed! return %d\n", ret);
		goto err2;
	}

	sunxi_ss_sysfs_create(pdev);

	SS_DBG("SS is inited, base 0x%p, irq %d!\n", sss->base_addr, sss->irq);
	return 0;

err2:
	sunxi_ss_hw_exit(sss);
err1:
	sunxi_ss_res_release(sss);
err0:
	platform_set_drvdata(pdev, NULL);
	return ret;
}

static int sunxi_ss_remove(struct platform_device *pdev)
{
	sunxi_ss_t *sss = platform_get_drvdata(pdev);

	ss_wait_idle();
	sunxi_ss_sysfs_remove(pdev);

	sunxi_ss_alg_unregister();
	sunxi_ss_hw_exit(sss);
	sunxi_ss_res_release(sss);

	platform_set_drvdata(pdev, NULL);
	ss_dev = NULL;
	return 0;
}

#ifdef CONFIG_PM
static int sunxi_ss_suspend(struct device *dev)
{
#ifdef CONFIG_EVB_PLATFORM
	struct platform_device *pdev = to_platform_device(dev);
	sunxi_ss_t *sss = platform_get_drvdata(pdev);
	unsigned long flags = 0;

	SS_ENTER();

	/* Wait for the completion of SS operation. */
	ss_dev_lock();

	spin_lock_irqsave(&ss_dev->lock, flags);
	sss->suspend = 1;
	spin_unlock_irqrestore(&sss->lock, flags);

	sunxi_ss_hw_exit(sss);
	ss_dev_unlock();
#endif

	return 0;
}

static int sunxi_ss_resume(struct device *dev)
{
	int ret = 0;
#ifdef CONFIG_EVB_PLATFORM
	struct platform_device *pdev = to_platform_device(dev);
	sunxi_ss_t *sss = platform_get_drvdata(pdev);
	unsigned long flags = 0;

	SS_ENTER();
	ret = sunxi_ss_hw_init(sss);
	spin_lock_irqsave(&ss_dev->lock, flags);
	sss->suspend = 0;
	spin_unlock_irqrestore(&sss->lock, flags);
#endif
	return ret;
}

static const struct dev_pm_ops sunxi_ss_dev_pm_ops = {
	.suspend = sunxi_ss_suspend,
	.resume  = sunxi_ss_resume,
};

#define SUNXI_SS_DEV_PM_OPS (&sunxi_ss_dev_pm_ops)
#else
#define SUNXI_SS_DEV_PM_OPS NULL
#endif /* CONFIG_PM */

static struct platform_driver sunxi_ss_driver = {
	.probe   = sunxi_ss_probe,
	.remove  = sunxi_ss_remove,
	.driver = {
	.name	= SUNXI_SS_DEV_NAME,
		.owner	= THIS_MODULE,
		.pm		= SUNXI_SS_DEV_PM_OPS,
		.of_match_table = sunxi_ss_of_match,
	},
};

static int sunxi_ce_open(struct inode *inode, struct file *file)
{
	file->private_data = ce_cdev;
	return 0;
}

static int sunxi_ce_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static ssize_t sunxi_ce_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	return 0;
}

static ssize_t sunxi_ce_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	return 0;
}

static int sunxi_ce_mmap(struct file *file, struct vm_area_struct *vma)
{
	return 0;
}

static int sunxi_copy_from_user(u8 **src, u32 size)
{
	int ret = -1;
	u8 *tmp = NULL;

	tmp = kzalloc(size, GFP_KERNEL);
	if (!tmp) {
		SS_ERR("kzalloc fail\n");
		return -ENOMEM;
	}

	ret = copy_from_user(tmp, *src, size);
	if (ret) {
		SS_ERR("copy_from_user fail\n");
		return -EAGAIN;
	}

	*src = tmp;

	return 0;
}

static int ce_channel_request(void)
{
	int i;
	unsigned long flags;

	spin_lock_irqsave(&ce_cdev->lock, flags);
	for (i = 0; i < SS_FLOW_NUM; i++) {
		if (ce_cdev->flows[i].available == SS_FLOW_AVAILABLE) {
			ce_cdev->flows[i].available = SS_FLOW_UNAVAILABLE;
			SS_DBG("The flow %d is available.\n", i);
			break;
		}
	}
	ce_cdev->flag = 1;
	spin_unlock_irqrestore(&ce_cdev->lock, flags);

	if (i == SS_FLOW_NUM) {
		SS_ERR("Failed to get an available flow.\n");
		i = -1;
	}
	return i;
}

static void ce_channel_free(int id)
{
	unsigned long flags;

	spin_lock_irqsave(&ce_cdev->lock, flags);
	ce_cdev->flows[id].available = SS_FLOW_AVAILABLE;
	ce_cdev->flag = 0;
	SS_DBG("The flow %d is available.\n", id);
	spin_unlock_irqrestore(&ce_cdev->lock, flags);

	return;
}

static long sunxi_ce_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	crypto_aes_req_ctx_t *aes_req_ctx;
	int channel_id = -1;
	int ret;
	phys_addr_t usr_key_addr;
	phys_addr_t usr_dst_addr;

	SS_DBG("cmd = %u\n", cmd);
	switch (cmd) {
	case CE_IOC_REQUEST:
	{
		channel_id = ce_channel_request();
		if (channel_id < 0) {
			SS_ERR("Failed to ce_channel_request\n");
			return -EAGAIN;
		}
		SS_DBG("channel_id = %d\n", channel_id);
		ret = put_user(channel_id, (int *)arg);
		if (ret < 0) {
			SS_ERR("Failed to put_user\n");
			return -EAGAIN;
		}

		break;
	}

	case CE_IOC_FREE:
	{
		ret = get_user(channel_id, (int *)arg);
		if (ret < 0) {
			SS_ERR("Failed to get_user\n");
			return -EAGAIN;
		}
		ce_channel_free(channel_id);

		break;
	}

	case CE_IOC_AES_CRYPTO:
	{
		SS_DBG("arg_size = %d\n", _IOC_SIZE(cmd));
		if (_IOC_SIZE(cmd) != sizeof(crypto_aes_req_ctx_t)) {
			SS_DBG("arg_size != sizeof(crypto_aes_req_ctx_t)\n");
			return -EINVAL;
		}

		aes_req_ctx = kzalloc(sizeof(crypto_aes_req_ctx_t), GFP_KERNEL);
		if (!aes_req_ctx) {
			SS_ERR("kzalloc aes_req_ctx fail\n");
			return -ENOMEM;
		}

		ret = copy_from_user(aes_req_ctx, (crypto_aes_req_ctx_t *)arg, sizeof(crypto_aes_req_ctx_t));
		if (ret) {
			SS_ERR("copy_from_user fail\n");
			kfree(aes_req_ctx);
			return -EAGAIN;
		}
		usr_key_addr = (phys_addr_t)aes_req_ctx->key_buffer;
		usr_dst_addr = (phys_addr_t)aes_req_ctx->dst_buffer;
		SS_DBG("usr_key_addr = 0x%px usr_dst_addr = 0x%px\n", (void *)usr_key_addr,
															(void *)usr_dst_addr);
		ret = sunxi_copy_from_user(&aes_req_ctx->key_buffer, aes_req_ctx->key_length);
		if (ret) {
			SS_ERR("key_buffer copy_from_user fail\n");
			return -EAGAIN;
		}

		SS_DBG("aes_req_ctx->key_buffer = 0x%px\n", aes_req_ctx->key_buffer);
		ret = sunxi_copy_from_user(&aes_req_ctx->iv_buf, aes_req_ctx->iv_length);
		if (ret) {
			SS_ERR("iv_buffer copy_from_user fail\n");
			return -EAGAIN;
		}

		if (aes_req_ctx->ion_flag) {
			SS_DBG("src_phy = 0x%lx, dst_phy = 0x%lx\n", aes_req_ctx->src_phy, aes_req_ctx->dst_phy);
		} else {
			ret = sunxi_copy_from_user(&aes_req_ctx->src_buffer, aes_req_ctx->src_length);
			if (ret) {
				SS_ERR("src_buffer copy_from_user fail\n");
				return -EAGAIN;
			}

			ret = sunxi_copy_from_user(&aes_req_ctx->dst_buffer, aes_req_ctx->dst_length);
			if (ret) {
				SS_ERR("dst_buffer copy_from_user fail\n");
				return -EAGAIN;
			}
		}

		SS_ERR("do_aes_crypto start\n");
		ret = do_aes_crypto(aes_req_ctx);
		if (ret) {
			kfree(aes_req_ctx);
			SS_ERR("do_aes_crypto fail\n");
			return -3;
		}

		if (aes_req_ctx->ion_flag) {
		} else {
			ret = copy_to_user((u8 *)usr_dst_addr, aes_req_ctx->dst_buffer, aes_req_ctx->dst_length);
			if (ret) {
				SS_ERR(" dst_buffer copy_from_user fail\n");
				kfree(aes_req_ctx);
				return -EAGAIN;
			}
		}
		break;
	}

	default:
		return -EINVAL;
		break;

	}
	return 0;
}

static const struct file_operations ce_fops = {
	.owner          = THIS_MODULE,
	.open           = sunxi_ce_open,
	.release        = sunxi_ce_release,
	.write          = sunxi_ce_write,
	.read           = sunxi_ce_read,
	.unlocked_ioctl = sunxi_ce_ioctl,
	.mmap           = sunxi_ce_mmap,
};


static int ce_setup_cdev(void)
{
	int ret = 0;

	ce_cdev = kzalloc(sizeof(sunxi_ce_cdev_t), GFP_KERNEL);
	if (!ce_cdev) {
		SS_ERR("kzalloc fail\n");
		return -ENOMEM;
	}

	/*alloc devid*/
	ret = alloc_chrdev_region(&ce_cdev->devid, 0, 1, SUNXI_SS_DEV_NAME);
	if (ret < 0) {
		SS_ERR("alloc_chrdev_region fail\n");
		kfree(ce_cdev);
		return -1;
	}

	SS_DBG("ce_cdev->devid = %d\n", ce_cdev->devid & 0xfff00000);
	/*alloc pcdev*/
	ce_cdev->pcdev = cdev_alloc();
	if (!ce_cdev->pcdev) {
		SS_ERR("cdev_alloc fail\n");
		ret = -ENOMEM;
		goto err0;
	}

	/*bing ce_fops*/
	cdev_init(ce_cdev->pcdev, &ce_fops);
	ce_cdev->pcdev->owner = THIS_MODULE;

	/*register cdev*/
	ret = cdev_add(ce_cdev->pcdev, ce_cdev->devid, 1);
	if (ret) {
		SS_ERR("cdev_add fail\n");
		ret = -1;
		goto err0;
	}

	ce_cdev->pclass = class_create(THIS_MODULE, SUNXI_SS_DEV_NAME);
	if (IS_ERR(ce_cdev->pclass)) {
		SS_ERR("class_create fail\n");
		ret = -1;
		goto err0;
	}

	ce_cdev->pdevice = device_create(ce_cdev->pclass, NULL, ce_cdev->devid, NULL,
						SUNXI_SS_DEV_NAME);

	/*init task_pool*/
	ce_cdev->task_pool = dma_pool_create("task_pool", ce_cdev->pdevice,
			sizeof(struct ce_task_desc), 4, 0);
	if (ce_cdev->task_pool == NULL)
		return -ENOMEM;

#ifdef CONFIG_OF
	ce_cdev->pdevice->dma_mask = &sunxi_ss_dma_mask;
	ce_cdev->pdevice->coherent_dma_mask = DMA_BIT_MASK(32);
#endif

	return 0;

err0:
	if (ce_cdev)
	kfree(ce_cdev);
	unregister_chrdev_region(ce_cdev->devid, 1);
	return ret;

}

static int ce_exit_cdev(void)
{
	device_destroy(ce_cdev->pclass, ce_cdev->devid);
	class_destroy(ce_cdev->pclass);

	unregister_chrdev_region(ce_cdev->devid, 1);

	cdev_del(ce_cdev->pcdev);
	kfree(ce_cdev);

	if (ce_cdev->task_pool)
		dma_pool_destroy(ce_cdev->task_pool);

	return 0;
}

static int __init sunxi_ce_module_init(void)
{
	int ret = 0;

	SS_ERR("sunxi_ce_module_init\n");
	ret = platform_driver_register(&sunxi_ss_driver);
	if (ret < 0) {
		SS_ERR("platform_driver_register() failed, return %d\n", ret);
		return ret;
	}

	ce_setup_cdev();

	return ret;
}

static void __exit sunxi_ce_module_exit(void)
{
	platform_driver_unregister(&sunxi_ss_driver);

	ce_exit_cdev();
}

module_init(sunxi_ce_module_init);
module_exit(sunxi_ce_module_exit);

MODULE_AUTHOR("mintow");
MODULE_DESCRIPTION("SUNXI SS Controller Driver");
MODULE_ALIAS("platform:"SUNXI_SS_DEV_NAME);
MODULE_LICENSE("GPL");
