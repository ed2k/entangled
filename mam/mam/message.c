/*
 * Copyright (c) 2018 IOTA Stiftung
 * https://github.com/iotaledger/entangled
 *
 * MAM is based on an original implementation & specification by apmi.bsu.by
 * [ITSec Lab]
 *
 * Refer to the LICENSE file for licensing information
 */

#include <stdlib.h>
#include <string.h>

#include "mam/mam/message.h"
#include "mam/ntru/mam_ntru_sk_t_set.h"
#include "mam/pb3/pb3.h"

/* MAC, MSSig, SignedId */

static size_t mam_msg_wrap_mac_size() {
  return 0
         /*  commit; */
         /*  squeeze tryte mac[81]; */
         + pb3_sizeof_ntrytes(81);
}

static void mam_msg_wrap_mac(mam_spongos_t *s, trits_t *b) {
  MAM_ASSERT(mam_msg_wrap_mac_size() <= trits_size(*b));
  /*  commit; */
  mam_spongos_commit(s);
  /*  squeeze tryte mac[81]; */
  pb3_wrap_squeeze_ntrytes(s, b, 81);
}

static retcode_t mam_msg_unwrap_mac(mam_spongos_t *s, trits_t *b) {
  retcode_t e = RC_OK;

  /*  commit; */
  mam_spongos_commit(s);
  /*  squeeze tryte mac[81]; */
  ERR_BIND_RETURN(pb3_unwrap_squeeze_ntrytes(s, b, 81), e);

  return e;
}

static size_t mam_msg_wrap_mssig_size(mam_mss_t *m) {
  size_t const sz = MAM_MSS_SIG_SIZE(m->height) / 3;
  return 0
         /*  commit; */
         /*  external squeeze tryte mac[78]; */
         + 0
         /*  absorb size_t sz; */
         + pb3_sizeof_size_t(sz)
         /*  skip tryte sig[sz]; */
         + pb3_sizeof_ntrytes(sz);
}

static void mam_msg_wrap_mssig(mam_spongos_t *s, trits_t *b, mam_mss_t *m) {
  MAM_TRITS_DEF0(mac, MAM_MSS_HASH_SIZE);
  size_t const sz = MAM_MSS_SIG_SIZE(m->height) / 3;
  mac = MAM_TRITS_INIT(mac, MAM_MSS_HASH_SIZE);

  MAM_ASSERT(mam_msg_wrap_mssig_size(m) <= trits_size(*b));

  /*  commit; */
  mam_spongos_commit(s);
  /*  external squeeze tryte mac[78]; */
  pb3_squeeze_external_ntrytes(s, mac);
  /*  absorb size_t sz; */
  pb3_wrap_absorb_size_t(s, b, sz);
  /*  skip tryte sig[sz]; */
  mam_mss_sign(m, mac, pb3_trits_take(b, pb3_sizeof_ntrytes(sz)));
}

static retcode_t mam_msg_unwrap_mssig(mam_spongos_t *s, trits_t *b,
                                      mam_spongos_t *ms, mam_spongos_t *ws,
                                      trits_t pk) {
  retcode_t e = RC_OK;

  MAM_TRITS_DEF0(mac, MAM_MSS_HASH_SIZE);
  size_t sz;
  mac = MAM_TRITS_INIT(mac, MAM_MSS_HASH_SIZE);

  /*  commit; */
  mam_spongos_commit(s);
  /*  external squeeze tryte mac[78]; */
  pb3_squeeze_external_ntrytes(s, mac);
  /*  absorb size_t sz; */
  ERR_BIND_RETURN(pb3_unwrap_absorb_size_t(s, b, &sz), e);
  /*  skip tryte sig[sz]; */
  ERR_GUARD_RETURN(pb3_sizeof_ntrytes(sz) <= trits_size(*b), RC_MAM_PB3_EOF);
  ERR_GUARD_RETURN(
      mam_mss_verify(ms, ws, mac, pb3_trits_take(b, pb3_sizeof_ntrytes(sz)),
                     pk),
      RC_MAM_PB3_BAD_SIG);

  return e;
}

static size_t mam_msg_wrap_signedid_size(mam_mss_t *m) {
  return 0
         /*  absorb tryte id[81]; */
         + pb3_sizeof_ntrytes(81)
         /*  MSSig mssig; */
         + mam_msg_wrap_mssig_size(m);
}

static void mam_msg_wrap_signedid(mam_spongos_t *s, trits_t *b, trits_t id,
                                  mam_mss_t *m) {
  MAM_ASSERT(mam_msg_wrap_signedid_size(m) <= trits_size(*b));
  MAM_ASSERT(pb3_sizeof_ntrytes(81) == trits_size(id));

  /*  absorb tryte id[81]; */
  pb3_wrap_absorb_ntrytes(s, b, id);
  /*  MSSig mssig; */
  mam_msg_wrap_mssig(s, b, m);
}

static retcode_t mam_msg_unwrap_signedid(mam_spongos_t *s, trits_t *b,
                                         trits_t id, mam_spongos_t *ms,
                                         mam_spongos_t *ws, trits_t pk) {
  retcode_t e = RC_OK;

  /*  absorb tryte id[81]; */
  MAM_ASSERT(pb3_sizeof_ntrytes(81) == trits_size(id));
  ERR_BIND_RETURN(pb3_unwrap_absorb_ntrytes(s, b, id), e);
  /*  MSSig mssig; */
  ERR_BIND_RETURN(mam_msg_unwrap_mssig(s, b, ms, ws, pk), e);

  return e;
}

/* Channel */

static size_t mam_msg_channel_wrap_size() {
  // absorb tryte version + absorb external tryte channel_id[81]
  return pb3_sizeof_tryte() + 0;
}

static void mam_msg_channel_wrap(mam_spongos_t *const spongos,
                                 trits_t *const buffer, tryte_t const version,
                                 trits_t const channel_id) {
  MAM_ASSERT(mam_msg_channel_wrap_size() <= trits_size(*buffer));
  MAM_ASSERT(pb3_sizeof_ntrytes(81) == trits_size(channel_id));

  // absorb tryte version
  pb3_wrap_absorb_tryte(spongos, buffer, version);
  // absorb external tryte channel_id[81]
  pb3_absorb_external_ntrytes(spongos, channel_id);
}

static retcode_t mam_msg_channel_unwrap(mam_spongos_t *const spongos,
                                        trits_t *const buffer,
                                        tryte_t *const version,
                                        trits_t channel_id) {
  MAM_ASSERT(pb3_sizeof_ntrytes(81) == trits_size(channel_id));

  retcode_t ret = RC_OK;

  // absorb tryte version
  if ((ret = pb3_unwrap_absorb_tryte(spongos, buffer, version)) != RC_OK) {
    return ret;
  }

  // absorb external tryte channel_id[81]
  pb3_absorb_external_ntrytes(spongos, channel_id);

  return ret;
}

/* Endpoint */

static size_t mam_msg_wrap_pubkey_chid_size() {
  return 0
         /*  absorb null chid; */
         + 0;
}

static void mam_msg_wrap_pubkey_chid(mam_spongos_t *s,
                                     trits_t *b) { /*  absorb null chid; */
}

static retcode_t mam_msg_unwrap_pubkey_chid(mam_spongos_t *s, trits_t *b) {
  /*  absorb null chid; */
  return RC_OK;
}

static size_t mam_msg_wrap_pubkey_epid_size() {
  return 0
         /*  absorb tryte epid[81]; */
         + pb3_sizeof_ntrytes(81);
}

static void mam_msg_wrap_pubkey_epid(mam_spongos_t *s, trits_t *b,
                                     trits_t epid) {
  MAM_ASSERT(mam_msg_wrap_pubkey_epid_size() <= trits_size(*b));
  MAM_ASSERT(pb3_sizeof_ntrytes(81) == trits_size(epid));

  /*  absorb tryte epid[81]; */
  pb3_wrap_absorb_ntrytes(s, b, epid);
}

static retcode_t mam_msg_unwrap_pubkey_epid(mam_spongos_t *s, trits_t *b,
                                            trits_t epid) {
  retcode_t e = RC_OK;

  /*  absorb tryte epid[81]; */
  MAM_ASSERT(pb3_sizeof_ntrytes(81) == trits_size(epid));
  ERR_BIND_RETURN(pb3_unwrap_absorb_ntrytes(s, b, epid), e);

  return e;
}

static size_t mam_msg_wrap_pubkey_chid1_size(mam_mss_t *m) {
  return mam_msg_wrap_signedid_size(m);
}

static void mam_msg_wrap_pubkey_chid1(mam_spongos_t *s, trits_t *b,
                                      trits_t chid1, mam_mss_t *m) {
  mam_msg_wrap_signedid(s, b, chid1, m);
}

static retcode_t mam_msg_unwrap_pubkey_chid1(mam_spongos_t *s, trits_t *b,
                                             trits_t chid1, mam_spongos_t *ms,
                                             mam_spongos_t *ws, trits_t pk) {
  return mam_msg_unwrap_signedid(s, b, chid1, ms, ws, pk);
}

static size_t mam_msg_wrap_pubkey_epid1_size(mam_mss_t *m) {
  return mam_msg_wrap_signedid_size(m);
}

static void mam_msg_wrap_pubkey_epid1(mam_spongos_t *s, trits_t *b,
                                      trits_t epid1, mam_mss_t *m) {
  mam_msg_wrap_signedid(s, b, epid1, m);
}

static retcode_t mam_msg_unwrap_pubkey_epid1(mam_spongos_t *s, trits_t *b,
                                             trits_t epid1, mam_spongos_t *ms,
                                             mam_spongos_t *ws, trits_t pk) {
  return mam_msg_unwrap_signedid(s, b, epid1, ms, ws, pk);
}

/* Keyload */

static size_t mam_msg_wrap_keyload_psk_size() {
  return 0
         /*  absorb tryte id[27]; */
         + pb3_sizeof_ntrytes(27)
         /*  absorb external tryte psk[81]; */
         + 0
         /*  commit; */
         /*  crypt tryte ekey[81]; */
         + pb3_sizeof_ntrytes(81);
}

static void mam_msg_wrap_keyload_psk(mam_spongos_t *s, trits_t *b, trits_t key,
                                     trits_t id, trits_t psk) {
  MAM_ASSERT(mam_msg_wrap_keyload_psk_size() <= trits_size(*b));
  MAM_ASSERT(pb3_sizeof_ntrytes(81) == trits_size(key));
  MAM_ASSERT(pb3_sizeof_ntrytes(27) == trits_size(id));
  MAM_ASSERT(pb3_sizeof_ntrytes(81) == trits_size(psk));

  /*  absorb tryte id[27]; */
  pb3_wrap_absorb_ntrytes(s, b, id);
  /*  absorb external tryte psk[81]; */
  pb3_absorb_external_ntrytes(s, psk);
  /*  commit; */
  mam_spongos_commit(s);
  /*  crypt tryte ekey[81]; */
  pb3_wrap_crypt_ntrytes(s, b, key);
}

static retcode_t mam_msg_unwrap_keyload_psk(mam_spongos_t *s, trits_t *b,
                                            trits_t key, bool *key_found,
                                            mam_psk_t_set_t p) {
  retcode_t e = RC_OK;
  MAM_TRITS_DEF0(id, MAM_PSK_ID_SIZE);
  id = MAM_TRITS_INIT(id, MAM_PSK_ID_SIZE);
  trit_t key2_trits[MAM_SPONGE_KEY_SIZE];
  trits_t key2 = trits_from_rep(MAM_SPONGE_KEY_SIZE, key2_trits);

  MAM_ASSERT(key_found);
  MAM_ASSERT(pb3_sizeof_ntrytes(81) == trits_size(key));

  /*  absorb tryte id[27]; */
  ERR_BIND_RETURN(pb3_unwrap_absorb_ntrytes(s, b, id), e);

  mam_psk_t_set_entry_t *entry = NULL;
  mam_psk_t_set_entry_t *tmp = NULL;
  bool psk_found = false;
  HASH_ITER(hh, p, entry, tmp) {
    if (trits_cmp_eq(id, mam_psk_id(&entry->value))) {
      psk_found = true;
      break;
    }
  }

  if (psk_found) {
    /*  absorb external tryte psk[81]; */
    pb3_absorb_external_ntrytes(s, mam_psk_trits(&entry->value));
    /*  commit; */
    mam_spongos_commit(s);
    /*  crypt tryte ekey[81]; */
    ERR_BIND_RETURN(pb3_unwrap_crypt_ntrytes(s, b, key2), e);

    if (*key_found) {
      ERR_GUARD_RETURN(trits_cmp_eq(key, key2), RC_MAM_KEYLOAD_OVERLOADED);
    } else {
      trits_copy(key2, key);
      *key_found = true;
    }

  } else { /* skip */
    ERR_GUARD_RETURN(MAM_SPONGE_KEY_SIZE <= trits_size(*b), RC_MAM_PB3_EOF);
    pb3_trits_take(b, MAM_SPONGE_KEY_SIZE);
    /* spongos state is corrupted */
  }

  return e;
}

static size_t mam_msg_wrap_keyload_ntru_size() {
  return 0
         /*  absorb tryte id[27]; */
         + pb3_sizeof_ntrytes(27)
         /*  absorb tryte ekey[3072]; */
         + pb3_sizeof_ntrytes(3072);
}

static void mam_msg_wrap_keyload_ntru(mam_spongos_t *s, trits_t *b, trits_t key,
                                      trits_t pk, mam_prng_t *p,
                                      mam_spongos_t *ns, trits_t N) {
  trits_t ekey;

  MAM_ASSERT(mam_msg_wrap_keyload_ntru_size() <= trits_size(*b));
  MAM_ASSERT(MAM_NTRU_KEY_SIZE == trits_size(key));
  MAM_ASSERT(MAM_NTRU_PK_SIZE == trits_size(pk));

  /*  absorb tryte id[27]; */
  pb3_wrap_absorb_ntrytes(s, b, trits_take(pk, pb3_sizeof_ntrytes(27)));
  /*  absorb tryte ekey[3072]; */
  ekey = pb3_trits_take(b, MAM_NTRU_EKEY_SIZE);
  ntru_encr(pk, p, ns, key, N, ekey);
  mam_spongos_absorb(s, ekey);
}

static retcode_t mam_msg_unwrap_keyload_ntru(mam_spongos_t *s, trits_t *b,
                                             trits_t key, bool *key_found,
                                             mam_ntru_sk_t_set_t n,
                                             mam_spongos_t *ns) {
  retcode_t e = RC_OK;
  trits_t ekey;
  MAM_TRITS_DEF0(id, 81);
  id = MAM_TRITS_INIT(id, 81);
  trit_t key2_trits[MAM_SPONGE_KEY_SIZE];
  trits_t key2 = trits_from_rep(MAM_SPONGE_KEY_SIZE, key2_trits);

  MAM_ASSERT(MAM_NTRU_KEY_SIZE == trits_size(key));

  /*  absorb tryte id[27]; */
  ERR_BIND_RETURN(pb3_unwrap_absorb_ntrytes(s, b, id), e);

  mam_ntru_sk_t_set_entry_t *entry = NULL;
  mam_ntru_sk_t_set_entry_t *tmp = NULL;
  bool ntru_found = false;
  HASH_ITER(hh, n, entry, tmp) {
    if (trits_cmp_eq(id, mam_ntru_pk_id(&entry->value.public_key))) {
      ntru_found = true;
      break;
    }
  }

  if (ntru_found) {
    /*  absorb tryte ekey[3072]; */
    ERR_GUARD_RETURN(MAM_NTRU_EKEY_SIZE <= trits_size(*b), RC_MAM_PB3_EOF);
    ekey = pb3_trits_take(b, MAM_NTRU_EKEY_SIZE);
    ERR_GUARD_RETURN(ntru_decr(&entry->value, ns, ekey, key2),
                     RC_MAM_PB3_BAD_EKEY);
    mam_spongos_absorb(s, ekey);

    if (*key_found) {
      ERR_GUARD_RETURN(trits_cmp_eq(key, key2), RC_MAM_KEYLOAD_OVERLOADED);
    } else {
      trits_copy(key2, key);
      *key_found = true;
    }

  } else { /* skip */
    ERR_GUARD_RETURN(MAM_NTRU_EKEY_SIZE <= trits_size(*b), RC_MAM_PB3_EOF);
    pb3_trits_take(b, MAM_NTRU_EKEY_SIZE);
    /* spongos state is corrupted */
  }

  return e;
}

/* Packet */

static size_t mam_msg_wrap_checksum_none_size() {
  return 0
         /*  absorb null none; */
         + 0;
}

static void mam_msg_wrap_checksum_none(mam_spongos_t *s, trits_t *b) {
  /*  absorb null none; */
}

static retcode_t mam_msg_unwrap_checksum_none(mam_spongos_t *s, trits_t *b) {
  /*  absorb null none; */
  return RC_OK;
}

static size_t mam_msg_wrap_checksum_mac_size() {
  return mam_msg_wrap_mac_size();
}

static void mam_msg_wrap_checksum_mac(mam_spongos_t *s, trits_t *b) {
  mam_msg_wrap_mac(s, b);
}

static retcode_t mam_msg_unwrap_checksum_mac(mam_spongos_t *s, trits_t *b) {
  return mam_msg_unwrap_mac(s, b);
}

static size_t mam_msg_wrap_checksum_mssig_size(mam_mss_t *m) {
  return mam_msg_wrap_mssig_size(m);
}

static void mam_msg_wrap_checksum_mssig(mam_spongos_t *s, trits_t *b,
                                        mam_mss_t *m) {
  mam_msg_wrap_mssig(s, b, m);
}

static retcode_t mam_msg_unwrap_checksum_mssig(mam_spongos_t *s, trits_t *b,
                                               mam_spongos_t *ms,
                                               mam_spongos_t *ws, trits_t pk) {
  return mam_msg_unwrap_mssig(s, b, ms, ws, pk);
}

size_t mam_msg_send_size(mam_channel_t *ch, mam_endpoint_t *ep,
                         mam_channel_t *ch1, mam_endpoint_t *ep1,
                         mam_psk_t_set_t psks, mam_ntru_pk_t_set_t ntru_pks) {
  size_t sz = 0;

  MAM_ASSERT(ch);

  /* channel */
  sz += mam_msg_channel_wrap_size();

  /* endpoint */
  /*  absorb oneof pubkey */
  sz += pb3_sizeof_oneof();

  if (ch1) {
    // SignedId chid1 = 2;
    sz += mam_msg_wrap_pubkey_chid1_size(&ch->mss);
  } else if (ep1) {
    // SignedId epid1 = 3;
    sz += mam_msg_wrap_pubkey_epid1_size(&ch->mss);
  } else if (ep) {
    // absorb tryte epid[81] = 1;
    sz += mam_msg_wrap_pubkey_epid_size();
  } else {
    //  null chid = 0;
    sz += mam_msg_wrap_pubkey_chid_size();
  }

  /* header */
  /* absorb trint typeid; */
  sz += pb3_sizeof_trint();
  {
    size_t keyload_count = 0;

    size_t num_pre_shared_keys = mam_psk_t_set_size(psks);
    keyload_count += num_pre_shared_keys;
    sz += (pb3_sizeof_oneof() + mam_msg_wrap_keyload_psk_size()) *
          num_pre_shared_keys;

    size_t num_pre_ntru_keys = mam_ntru_pk_t_set_size(ntru_pks);
    keyload_count += num_pre_ntru_keys;
    sz += (pb3_sizeof_oneof() + mam_msg_wrap_keyload_ntru_size()) *
          num_pre_ntru_keys;

    /*  absorb repeated */
    sz += pb3_sizeof_repeated(keyload_count);
  }
  /*  external secret tryte key[81]; */
  sz += 0;

  /* packets */
  return sz;
}

void mam_msg_send(mam_msg_send_context_t *ctx, mam_prng_t *prng,
                  mam_channel_t *ch, mam_endpoint_t *ep, mam_channel_t *ch1,
                  mam_endpoint_t *ep1, trits_t msg_id, trint9_t msg_type_id,
                  mam_psk_t_set_t psks, mam_ntru_pk_t_set_t ntru_pks,
                  trits_t *msg) {
  trit_t session_key_trits[MAM_SPONGE_KEY_SIZE];
  trits_t session_key = trits_from_rep(MAM_SPONGE_KEY_SIZE, session_key_trits);

  MAM_TRITS_DEF0(skn, MAM_MSS_SKN_SIZE);
  skn = MAM_TRITS_INIT(skn, MAM_MSS_SKN_SIZE);

  MAM_ASSERT(ctx);
  MAM_ASSERT(ch);
  MAM_ASSERT(msg);

  MAM_ASSERT(!(trits_size(*msg) <
               mam_msg_send_size(ch, ep, ch1, ep1, psks, ntru_pks)));

  if (ep) {
    mam_mss_skn(&ep->mss, skn);
  } else {
    mam_mss_skn(&ch->mss, skn);
  }

  /* generate session key */
  if (psks == NULL && ntru_pks == NULL) {  // public
    trits_set_zero(session_key);
  } else {
    mam_prng_gen3(prng, MAM_PRNG_DST_SEC_KEY, mam_channel_name(ch),
                  ep ? mam_endpoint_name(ep) : trits_null(), skn, session_key);
  }

  /* choose recipient */
  mam_spongos_init(&ctx->spongos);

  /* wrap Channel */
  mam_msg_channel_wrap(&ctx->spongos, msg, 0, mam_channel_id(ch));

  /* wrap Endpoint */
  {
    tryte_t pubkey;

    if (ch1) { /*  SignedId chid1 = 2; */
      pubkey = (tryte_t)mam_msg_pubkey_chid1;
      pb3_wrap_absorb_tryte(&ctx->spongos, msg, pubkey);
      mam_msg_wrap_pubkey_chid1(&ctx->spongos, msg, mam_channel_id(ch1),
                                &ch->mss);
    } else if (ep1) { /*  SignedId epid1 = 3; */
      pubkey = (tryte_t)mam_msg_pubkey_epid1;
      pb3_wrap_absorb_tryte(&ctx->spongos, msg, pubkey);
      mam_msg_wrap_pubkey_epid1(&ctx->spongos, msg, mam_endpoint_id(ep1),
                                &ch->mss);
    } else if (ep) { /*  absorb tryte epid[81] = 1; */
      pubkey = (tryte_t)mam_msg_pubkey_epid;
      pb3_wrap_absorb_tryte(&ctx->spongos, msg, pubkey);
      mam_msg_wrap_pubkey_epid(&ctx->spongos, msg, mam_endpoint_id(ep));
    } else { /*  absorb null chid = 0; */
      pubkey = (tryte_t)mam_msg_pubkey_chid;
      pb3_wrap_absorb_tryte(&ctx->spongos, msg, pubkey);
      mam_msg_wrap_pubkey_chid(&ctx->spongos, msg);
    }
  }

  /* wrap Header */
  {
    /*  absorb tryte msgid[27]; */
    pb3_absorb_external_ntrytes(&ctx->spongos, msg_id);
    /*  absorb trint typeid; */
    pb3_wrap_absorb_trint(&ctx->spongos, msg, msg_type_id);

    {
      size_t keyload_count = 0;
      tryte_t keyload;
      mam_spongos_t spongos_ntru;
      mam_spongos_t spongos_fork;

      keyload_count += mam_psk_t_set_size(psks);
      keyload_count += mam_ntru_pk_t_set_size(ntru_pks);
      /*  repeated */
      pb3_wrap_absorb_size_t(&ctx->spongos, msg, keyload_count);

      mam_psk_t_set_entry_t *curr_entry_psk = NULL;
      mam_psk_t_set_entry_t *tmp_entry_psk = NULL;

      HASH_ITER(hh, psks, curr_entry_psk, tmp_entry_psk) {
        /*  absorb oneof keyload */
        keyload = (tryte_t)mam_msg_keyload_psk;
        pb3_wrap_absorb_tryte(&ctx->spongos, msg, keyload);
        /*  fork; */
        mam_mam_spongos_fork(&ctx->spongos, &spongos_fork);
        /*  KeyloadPSK psk = 1; */
        mam_msg_wrap_keyload_psk(&spongos_fork, msg, session_key,
                                 mam_psk_id(&curr_entry_psk->value),
                                 mam_psk_trits(&curr_entry_psk->value));
      }

      mam_ntru_pk_t_set_entry_t *curr_entry_ntru = NULL;
      mam_ntru_pk_t_set_entry_t *tmp_entry_ntru = NULL;

      HASH_ITER(hh, ntru_pks, curr_entry_ntru, tmp_entry_ntru) {
        /*  absorb oneof keyload */
        keyload = (tryte_t)mam_msg_keyload_ntru;
        pb3_wrap_absorb_tryte(&ctx->spongos, msg, keyload);
        /*  fork; */
        mam_mam_spongos_fork(&ctx->spongos, &spongos_fork);
        /*  KeyloadNTRU ntru = 2; */
        mam_msg_wrap_keyload_ntru(&spongos_fork, msg, session_key,
                                  mam_ntru_pk_trits(&curr_entry_ntru->value),
                                  prng, &spongos_ntru, msg_id);
      }
    }

    /*  absorb external tryte key[81]; */
    pb3_absorb_external_ntrytes(&ctx->spongos, session_key);
    /*  commit; */
    mam_spongos_commit(&ctx->spongos);
  }

  memset_safe(trits_begin(session_key), trits_size(session_key), 0,
              trits_size(session_key));
}

size_t mam_msg_send_packet_size(mam_msg_checksum_t checksum, mam_mss_t *mss,
                                size_t payload_size) {
  size_t sz = 0;
  MAM_ASSERT(0 == payload_size % 3);
  sz = 0
       /*  absorb tryte sz; */
       + pb3_sizeof_size_t(payload_size / 3)
       /*  crypt tryte payload[sz]; */
       + pb3_sizeof_ntrytes(payload_size / 3)
       /*  absorb oneof checksum */
       + pb3_sizeof_oneof();

  if (mam_msg_checksum_none == checksum) {
    // absorb null none = 0;
    sz += mam_msg_wrap_checksum_none_size();
  } else if (mam_msg_checksum_mac == checksum) {
    //  MAC mac = 1;
    sz += mam_msg_wrap_checksum_mac_size();
  } else if (mam_msg_checksum_mssig == checksum) {
    //  MSSig mssig = 2;
    MAM_ASSERT(mss);
    sz += mam_msg_wrap_checksum_mssig_size(mss);
  } else {
    /*  commit; */
    MAM_ASSERT(0);
  }

  return sz;
}

void mam_msg_send_packet(mam_msg_send_context_t *ctx,
                         mam_msg_checksum_t checksum, trits_t payload,
                         trits_t *b) {
  MAM_ASSERT(ctx);
  MAM_ASSERT(b);

  MAM_ASSERT(!(trits_size(*b) < mam_msg_send_packet_size(checksum, ctx->mss,
                                                         trits_size(payload))));

  /*  absorb long trint ord; */
  {
    trit_t ord_trits[18];
    trits_t ord = trits_from_rep(18, ord_trits);
    trits_put18(ord, ctx->ord);
    pb3_absorb_external_ntrytes(&ctx->spongos, ord);
  }

  /*  absorb tryte sz; */
  pb3_wrap_absorb_size_t(&ctx->spongos, b, trits_size(payload) / 3);
  /*  crypt tryte payload[sz]; */
  pb3_wrap_crypt_ntrytes(&ctx->spongos, b, payload);

  /*  absorb oneof checksum */
  pb3_wrap_absorb_tryte(&ctx->spongos, b, (tryte_t)checksum);
  if (mam_msg_checksum_none == checksum) {
    /*    absorb null none = 0; */
    mam_msg_wrap_checksum_none(&ctx->spongos, b);
  } else if (mam_msg_checksum_mac == checksum) {
    /*    MAC mac = 1; */
    mam_msg_wrap_checksum_mac(&ctx->spongos, b);
  } else if (mam_msg_checksum_mssig == checksum) {
    /*    MSSig mssig = 2; */
    mam_msg_wrap_checksum_mssig(&ctx->spongos, b, ctx->mss);
  } else {
    MAM_ASSERT(0);
  }
  /*  commit; */
  mam_spongos_commit(&ctx->spongos);
}

retcode_t mam_msg_recv(mam_msg_recv_context_t *ctx, trits_t const *const msg,
                       mam_psk_t_set_t psks, mam_ntru_sk_t_set_t ntru_sks,
                       trits_t msg_id) {
  retcode_t e = RC_OK;

  MAM_ASSERT(ctx);

  trit_t chid[MAM_CHANNEL_ID_SIZE];
  memcpy(chid, ctx->pk, MAM_CHANNEL_ID_SIZE);

  mam_spongos_init(&ctx->spongos);

  trit_t session_key_trits[MAM_SPONGE_KEY_SIZE];
  trits_t session_key = trits_from_rep(MAM_SPONGE_KEY_SIZE, session_key_trits);

  /* unwrap Channel */
  {
    tryte_t ver = -1;
    ERR_BIND_RETURN(
        mam_msg_channel_unwrap(&ctx->spongos, msg, &ver,
                               trits_from_rep(MAM_CHANNEL_ID_SIZE, chid)),
        e);
    ERR_GUARD_RETURN(0 == ver, RC_MAM_VERSION_NOT_SUPPORTED);
  }

  /* unwrap Endpoint */
  {
    mam_spongos_t spongos_mss;
    mam_spongos_t spongos_wots;
    tryte_t pubkey = -1;
    ERR_BIND_RETURN(pb3_unwrap_absorb_tryte(&ctx->spongos, msg, &pubkey), e);
    ERR_GUARD_RETURN(0 <= pubkey && pubkey <= 3, RC_MAM_PB3_BAD_ONEOF);

    if (mam_msg_pubkey_chid1 == pubkey) { /*  SignedId chid1 = 2; */
      /*TODO: verify chid is trusted */
      ERR_BIND_RETURN(
          mam_msg_unwrap_pubkey_chid1(
              &ctx->spongos, msg, trits_from_rep(MAM_CHANNEL_ID_SIZE, ctx->pk),
              &spongos_mss, &spongos_wots,
              trits_from_rep(MAM_CHANNEL_ID_SIZE, chid)),
          e);
      /*TODO: record new channel/endpoint */
    } else if (mam_msg_pubkey_epid1 == pubkey) { /*  SignedId epid1 = 3; */

      /*TODO: verify chid is trusted */
      ERR_BIND_RETURN(
          mam_msg_unwrap_pubkey_epid1(
              &ctx->spongos, msg, trits_from_rep(MAM_CHANNEL_ID_SIZE, ctx->pk),
              &spongos_mss, &spongos_wots,
              trits_from_rep(MAM_CHANNEL_ID_SIZE, chid)),
          e);
      /*TODO: record new channel/endpoint */
    } else if (mam_msg_pubkey_epid ==
               pubkey) { /*  absorb tryte epid[81] = 1; */
      ERR_BIND_RETURN(
          mam_msg_unwrap_pubkey_epid(
              &ctx->spongos, msg, trits_from_rep(MAM_CHANNEL_ID_SIZE, ctx->pk)),
          e);
    } else if (mam_msg_pubkey_chid == pubkey) { /*  absorb null chid = 0; */
      ERR_BIND_RETURN(mam_msg_unwrap_pubkey_chid(&ctx->spongos, msg), e);
    } else
      MAM_ASSERT(0);
  }

  /* unwrap Header */
  {
    trint9_t msg_type_id;

    /*  absorb tryte msg_id[27]; */
    pb3_absorb_external_ntrytes(&ctx->spongos, msg_id);
    /*  absorb trint typeid; */
    ERR_BIND_RETURN(pb3_unwrap_absorb_trint(&ctx->spongos, msg, &msg_type_id),
                    e);
    // TODO switch case on the msg_type_id
    {
      /*  repeated */
      size_t keyload_count = 0;
      bool key_found = false;
      mam_spongos_t spongos_ntru;

      ERR_BIND_RETURN(
          pb3_unwrap_absorb_size_t(&ctx->spongos, msg, &keyload_count), e);

      if (0 < keyload_count) {
        mam_spongos_t spongos_fork;

        for (e = RC_OK; e == RC_OK && keyload_count--;) {
          tryte_t keyload = -1;

          /*  absorb oneof keyload */
          ERR_BIND_RETURN(pb3_unwrap_absorb_tryte(&ctx->spongos, msg, &keyload),
                          e);
          ERR_GUARD_RETURN(1 <= keyload && keyload <= 2, RC_MAM_PB3_BAD_ONEOF);
          /*  fork; */
          mam_mam_spongos_fork(&ctx->spongos, &spongos_fork);

          if (mam_msg_keyload_psk == keyload) { /*  KeyloadPSK psk = 1; */
            ERR_BIND_RETURN(
                mam_msg_unwrap_keyload_psk(&spongos_fork, msg, session_key,
                                           &key_found, psks),
                e);

          } else if (mam_msg_keyload_ntru ==
                     keyload) { /*  KeyloadNTRU ntru = 2; */
            ERR_BIND_RETURN(mam_msg_unwrap_keyload_ntru(
                                &spongos_fork, msg, session_key, &key_found,
                                ntru_sks, &spongos_ntru),
                            e);

          } else
            ERR_GUARD_RETURN(0, RC_MAM_PB3_BAD_ONEOF);
        }

        ERR_GUARD_RETURN(key_found, RC_MAM_KEYLOAD_IRRELEVANT);
      } else
        /* no recipients => public mode */
        trits_set_zero(session_key);
    }
  }

  /*  absorb external tryte key[81]; */
  pb3_absorb_external_ntrytes(&ctx->spongos, session_key);
  /*  commit; */
  mam_spongos_commit(&ctx->spongos);

  return e;
}

retcode_t mam_msg_recv_packet(mam_msg_recv_context_t *ctx, trits_t *b,
                              trits_t *payload) {
  retcode_t e = RC_OK;
  trits_t p = trits_null();

  MAM_ASSERT(ctx);
  MAM_ASSERT(b);
  MAM_ASSERT(payload);

  size_t sz = 0;
  tryte_t checksum = -1;

  /*  absorb long trint ord; */
  {
    trit_t ord_trits[18];
    trits_t ord = trits_from_rep(18, ord_trits);
    trits_put18(ord, ctx->ord);
    pb3_absorb_external_ntrytes(&ctx->spongos, ord);
  }
  /*TODO: check ord */

  /*  absorb tryte sz; */
  ERR_BIND_RETURN(pb3_unwrap_absorb_size_t(&ctx->spongos, b, &sz), e);
  /*  crypt tryte payload[sz]; */
  if (trits_is_null(*payload)) {
    p = trits_alloc(pb3_sizeof_ntrytes(sz));
    ERR_GUARD_RETURN(!trits_is_null(p), RC_OOM);
  } else {
    ERR_GUARD_GOTO(trits_size(*payload) >= pb3_sizeof_ntrytes(sz),
                   RC_MAM_BUFFER_TOO_SMALL, e, cleanup);
    p = pb3_trits_take(payload, pb3_sizeof_ntrytes(sz));
  }
  ERR_BIND_GOTO(pb3_unwrap_crypt_ntrytes(&ctx->spongos, b, p), e, cleanup);

  /*  absorb oneof checksum */
  ERR_BIND_GOTO(pb3_unwrap_absorb_tryte(&ctx->spongos, b, &checksum), e,
                cleanup);

  if (mam_msg_checksum_none == checksum) {
    /*    absorb null none = 0; */
    ERR_BIND_GOTO(mam_msg_unwrap_checksum_none(&ctx->spongos, b), e, cleanup);
  } else if (mam_msg_checksum_mac == checksum) {
    /*    MAC mac = 1; */
    ERR_BIND_GOTO(mam_msg_unwrap_checksum_mac(&ctx->spongos, b), e, cleanup);
  } else if (mam_msg_checksum_mssig == checksum) {
    mam_spongos_t spongos_mss;
    mam_spongos_t spongos_wots;
    /*    MSSig mssig = 2; */
    ERR_BIND_GOTO(mam_msg_unwrap_checksum_mssig(
                      &ctx->spongos, b, &spongos_mss, &spongos_wots,
                      trits_from_rep(MAM_CHANNEL_ID_SIZE, ctx->pk)),
                  e, cleanup);
  } else {
    ERR_GUARD_GOTO(0, RC_MAM_PB3_BAD_ONEOF, e, cleanup);
  }

  /*  commit; */
  mam_spongos_commit(&ctx->spongos);

  if (trits_is_null(*payload)) *payload = p;
  p = trits_null();

cleanup:
  trits_free(p);
  return e;
}

size_t mam_msg_send_ctx_serialized_size(
    mam_msg_send_context_t const *const ctx) {
  return mam_spongos_serialized_size(&ctx->spongos) + MAM_MSG_ORD_SIZE +
         MAM_MSS_PK_SIZE;
}

void mam_msg_send_ctx_serialize(mam_msg_send_context_t const *const ctx,
                                trits_t *const buffer) {
  mam_spongos_serialize(&ctx->spongos, buffer);
  trits_put18(*buffer, ctx->ord);
  trits_advance(buffer, MAM_MSG_ORD_SIZE);
  pb3_encode_ntrytes(trits_from_rep(MAM_MSS_PK_SIZE, ctx->mss->root), buffer);
}

retcode_t mam_msg_send_ctx_deserialize(trits_t *const buffer,
                                       mam_msg_send_context_t *const ctx) {
  retcode_t ret;
  ERR_BIND_RETURN(mam_spongos_deserialize(buffer, &ctx->spongos), ret);
  ctx->ord = trits_get18(*buffer);
  trits_advance(buffer, MAM_MSG_ORD_SIZE);
  ctx->mss = NULL;
  trits_t root_id = trits_from_rep(MAM_MSS_PK_SIZE, ctx->mss_root);
  ERR_BIND_RETURN(pb3_decode_ntrytes(root_id, buffer), ret);
  return ret;
}

size_t mam_msg_recv_ctx_serialized_size(
    mam_msg_recv_context_t const *const ctx) {
  return mam_spongos_serialized_size(&ctx->spongos) + MAM_CHANNEL_ID_SIZE +
         MAM_MSG_ORD_SIZE;
}

void mam_msg_recv_ctx_serialize(mam_msg_recv_context_t const *const ctx,
                                trits_t *const buffer) {
  mam_spongos_serialize(&ctx->spongos, buffer);
  pb3_encode_ntrytes(trits_from_rep(MAM_CHANNEL_ID_SIZE, ctx->pk), buffer);
  trits_put18(*buffer, ctx->ord);
  trits_advance(buffer, MAM_MSG_ORD_SIZE);
}

retcode_t mam_msg_recv_ctx_deserialize(trits_t *const buffer,
                                       mam_msg_recv_context_t *const ctx) {
  retcode_t ret;
  ERR_BIND_RETURN(mam_spongos_deserialize(buffer, &ctx->spongos), ret);
  ERR_BIND_RETURN(
      pb3_decode_ntrytes(trits_from_rep(MAM_CHANNEL_ID_SIZE, ctx->pk), buffer),
      ret);
  ctx->ord = trits_get18(*buffer);
  trits_advance(buffer, MAM_MSG_ORD_SIZE);
  return ret;
}
