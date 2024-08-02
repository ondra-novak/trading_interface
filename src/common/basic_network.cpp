#include "basic_network.h"
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/types.h>
#include <openssl/hmac.h>
#include <sstream>
#include <iostream>
#include <fstream>

namespace trading_api {

using PEvpPKey = std::unique_ptr<EVP_PKEY, decltype([](auto *x){EVP_PKEY_free(x);})>;
using PBIO = std::unique_ptr<BIO, decltype([](auto x){BIO_free(x);})>;
using PEvpMDCtx = std::unique_ptr<EVP_MD_CTX, decltype([](auto x){EVP_MD_CTX_free(x);})>;

class MyPrivKey: public INetwork::IPrivKey {
public:
     MyPrivKey(EVP_PKEY *key):_key(key) {}
     virtual ~MyPrivKey() {EVP_PKEY_free(_key);}
     operator EVP_PKEY *() const {return _key;}
protected:
    EVP_PKEY *_key;
};

std::shared_ptr<MyPrivKey> loadPrivateKeyFromStream(std::istream& keyStream) {
    // Read the stream into a string
    std::stringstream buffer;
    buffer << keyStream.rdbuf();
    std::string keyStr = buffer.str();

    // Create a BIO from the string
    BIO* bio = BIO_new_mem_buf(keyStr.data(), keyStr.size());
    if (!bio) return nullptr;

    // Read the private key from the BIO
    EVP_PKEY* pkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
    // Free the BIO
    BIO_free(bio);
    
    return pkey?std::make_shared<MyPrivKey>(pkey):nullptr;
}

std::basic_string<unsigned char> BasicNetwork::calc_hmac256(std::string_view key, std::string_view msg) const
{
    unsigned char md[100]; //should be enough (expected 32 bytes)
    unsigned int md_len =sizeof(md);
    HMAC(EVP_sha256(), reinterpret_cast<const unsigned char *>(key.data()),
                key.length(),
                reinterpret_cast<const unsigned char *>(msg.data()),
                msg.length()
                , md, &md_len);
    return std::basic_string<unsigned char>(md, md_len);
}

BasicNetwork::PrivKey BasicNetwork::priv_key_from_file(std::string_view file_name) const
{

    std::ifstream in((std::string(file_name)));
    if (!in) {
        int e = errno;
        std::string msg = "Unable to open private key:";
        msg.append(file_name);
        throw std::system_error(e,std::system_category(), msg);
    }
    auto pk = loadPrivateKeyFromStream(in);
    if (pk == nullptr) {
        std::string msg = "Can't parse private key:";
        msg.append(file_name);
        throw std::runtime_error(msg);
    }
    return PrivKey(pk);
}

BasicNetwork::PrivKey BasicNetwork::priv_key_from_string(std::string_view priv_key_str) const
{
    std::istringstream in((std::string(priv_key_str)));
    auto pk = loadPrivateKeyFromStream(in);
    if (pk == nullptr) {
        throw std::runtime_error("failed to parse private key");
    }
    return PrivKey(pk);
}

void handle_errors() {
    char buff[200] = "OpenSSL:";
    char *app = buff+strlen(buff);
    auto remain = sizeof(buff) - (app - buff);
    ERR_error_string_n(ERR_get_error(),app, remain);
    throw std::runtime_error(buff);
}


std::basic_string<unsigned char> BasicNetwork::sign_message(std::string_view message, const PrivKey &pk) const
{
    const MyPrivKey &pkey = dynamic_cast<const MyPrivKey &>(*pk);

    const unsigned char *message_bytes = reinterpret_cast<const unsigned char *>(message.data());
    size_t message_len = message.length();
    PEvpMDCtx mdctx(EVP_MD_CTX_new());
    if (EVP_DigestSignInit(mdctx.get(), nullptr, nullptr, nullptr, pkey) <= 0) {
        handle_errors();
    }
    size_t sig_len;
    if (EVP_DigestSign(mdctx.get(), nullptr, &sig_len, message_bytes, message_len) <= 0) {
        handle_errors();
    }
    std::basic_string<unsigned char> sig;
    sig.resize(sig_len);
    if (EVP_DigestSign(mdctx.get(), sig.data(), &sig_len, message_bytes, message_len) <= 0) {
        handle_errors();
    }
    sig.resize(sig_len);
    return sig;
}

constexpr const char *hex_chars = "0123456789ABCDEF";
template<typename InputIterator, typename OutputIterator>
constexpr auto url_encode(InputIterator begin, InputIterator end, OutputIterator out) {
    for (auto it = begin; it != end; ++it) {
        char c = *it;
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == '@' /*binance*/) {
            *out++ = c;
        } else {
            *out++ = '%';
            *out++ = hex_chars[(c >> 4) & 0x0F];
            *out++ = hex_chars[c & 0x0F];
        }
    }
    return out;
}


std::string BasicNetwork::make_query(std::span<const std::pair<std::string_view, std::string_view>> fields) const
{
    std::ostringstream out;
    auto b = fields.begin();
    auto e = fields.end();
    if (b != e) {
        url_encode(b->first.begin(), b->first.end(), std::ostreambuf_iterator(out));
        out << '=';
        url_encode(b->second.begin(), b->second.end(), std::ostreambuf_iterator(out));
        ++b;
        while (b != e) {
            out << '&';
            url_encode(b->first.begin(), b->first.end(), std::ostreambuf_iterator(out));
            out << '=';
            url_encode(b->second.begin(), b->second.end(), std::ostreambuf_iterator(out));
            ++b;
        }
    }
    return std::move(out).str();
}
}