#pragma once
#include "rpc_client.h"

typedef struct evp_pkey_st EVP_PKEY;

class SecureRPCClient: public RPCClient {
public:
    struct KeyDeleter {
        void operator ()(EVP_PKEY *);
    };

    ///contains autorization keys to an account
    struct Account {

        ///constructor
        /**
         * @param api_key api key identifier
         * @param secret PEM format of private key
         */
        Account(std::string api_key, std::string secret);

        std::string _api_key_name;
        std::unique_ptr<EVP_PKEY, KeyDeleter> _key;
    };


    using RPCClient::RPCClient;

    using RPCClient::operator();
    using RPCClient::call;
    ///perform signed RPC call
    /**
     * @param acc account
     * @param method method
     * @param params params
     * @return asynchronous result
     */
    AsyncResult operator()(const Account &acc, std::string_view method, json::value params);
    ///perform signed RPC call
    /**
     * @param acc account
     * @param method method
     * @param params params
     * @return asynchronous result
     */
    AsyncResult call(const Account &acc,std::string_view method, json::value params);




protected:
    std::uint64_t get_cur_time() const;
};