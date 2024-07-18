#include "secure_rpc.h"
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/types.h>
#include <sstream>
#include <iterator>





using PEvpPKey = std::unique_ptr<EVP_PKEY, decltype([](auto *x){EVP_PKEY_free(x);})>;
using PBIO = std::unique_ptr<BIO, decltype([](auto x){BIO_free(x);})>;
using PEvpMDCtx = std::unique_ptr<EVP_MD_CTX, decltype([](auto x){EVP_MD_CTX_free(x);})>;


void SecureRPCClient::KeyDeleter::operator ()(EVP_PKEY *x) {
    EVP_PKEY_free(x);
}


SecureRPCClient::Account::Account(std::string api_key, std::string secret) {
    PBIO bio ( BIO_new_mem_buf(secret.data(), secret.length()));
    if (!bio) throw std::runtime_error("Unable to create BIO for private key");
    EVP_PKEY *k = PEM_read_bio_PrivateKey(bio.get(), NULL, NULL, NULL);
    if (!k) throw std::runtime_error("Error reading private key from string");
    _key.reset(k);
    _api_key_name = std::move(api_key);
}

void handle_errors() {
    char buff[200] = "OpenSSL:";
    char *app = buff+strlen(buff);
    auto remain = sizeof(buff) - (app - buff);
    ERR_error_string_n(ERR_get_error(),app, remain);
    throw std::runtime_error(buff);
}

template<std::input_iterator Input, std::output_iterator<char> Output>
static Output bin_to_hex(Input in, Input end, Output out) {
    while (in != end) {
        unsigned char c = *in;
        unsigned char d = c >> 4;
        c = c & 0xF;
        *out = d>9?'a'+d-10:'0'+d;
        ++out;
        *out = c>9?'a'+c-10:'0'+c;
        ++out;
        ++in;
    }
    return out;
}

SecureRPCClient::AsyncResult SecureRPCClient::operator ()(const Account &acc,
        std::string_view method, json::value params) {
    return call(acc, method, params);
}

SecureRPCClient::AsyncResult SecureRPCClient::call(const Account &acc,std::string_view method, json::value params) {
    std::ostringstream msg;
    params.set({
        {"apiKey",acc._api_key_name},
        {"timestamp", get_cur_time()}
    });
    bool sep = false;
    for (const auto &[k, v]: params.as<json::object_view>()) {
        if (sep) msg << "&"; else sep = true;
        msg << k << "=" << v.to_string();
    }



    std::string_view message = msg.view();
    const unsigned char *message_bytes = reinterpret_cast<const unsigned char *>(message.data());
    size_t message_len = message.length();
    PEvpMDCtx mdctx(EVP_MD_CTX_new());
    if (EVP_DigestSignInit(mdctx.get(), NULL, NULL, NULL, acc._key.get()) <= 0) {
        handle_errors();
    }
    size_t sig_len;
    if (EVP_DigestSign(mdctx.get(), NULL, &sig_len, message_bytes, message_len) <= 0) {
        handle_errors();
    }
    unsigned char *sig = reinterpret_cast<unsigned char *>(alloca(sig_len));
    if (EVP_DigestSign(mdctx.get(), sig, &sig_len, message_bytes, message_len) <= 0) {
        handle_errors();
    }
    std::string signature;
    json::base64.encode(sig, sig+sig_len, std::back_inserter(signature));

    params.set("signature",signature);
    return call(method, params);
}

std::uint64_t SecureRPCClient::get_cur_time() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
}
