#pragma once
#include "wrapper.h"
#include <span>
#include <string_view>
#include <string>
#include <vector>

namespace trading_api {

using binary_string_view = std::basic_string_view<unsigned char>;

class IWebSocketClient{
public:
    virtual ~IWebSocketClient() = default;

    ///Events sent to the user code 
    class IEvents {
    public:
        ///Text message has been received
        /**
         * @param msg contains text message. The string view is valid only
         * in context of this message
         * Execution of this function blocks receiving of other messages 
         */
        virtual void on_message(std::string_view msg) noexcept = 0;
        ///Binary message has been received
        /**
         * @param msg contains binary message. The string view is valid only
         * in context of this message
         * Execution of this function blocks receiving of other messages 
         */
        virtual void on_message(binary_string_view msg) noexcept = 0;
        ///called once the connection is established
        /** 
         * You can send initial messages here 
         */
        virtual void on_open() noexcept = 0;
        ///called when stream is closed/disconnected
        /** 
         * This event is generated when stream is closed by peer. If the reconnect is allowed, this
         * event only notifies about lost context. After reconnect is successful, on_open si called
         * 
         */
        virtual void on_close() noexcept = 0;   
        ///called when client is destroyed
        virtual void on_destroy() noexcept = 0;
    };

    virtual bool send(std::string_view msg) = 0;
    virtual bool send(binary_string_view msg) = 0;
    virtual bool close() = 0;
};

///Web socket client
class WebSocketClient {
public:
    using IEvents = IWebSocketClient::IEvents;

    WebSocketClient(std::shared_ptr<IWebSocketClient> ptr):_ptr(std::move(ptr)) {}
    ///send text message
    /** @param msg message to send
     * @retval true sent
     * @retval false connection is currently unavailable. Message lost. You can send it
     *  during on_open event(). 
     */
    bool send(std::string_view msg) {return _ptr->send(msg);}
    ///send binary message
    /** @param msg message to send
     * @retval true sent
     * @retval false connection is currently unavailable. Message lost. You can send it
     *  during on_open event(). 
     */
    bool send(binary_string_view msg) {return _ptr->send(msg);}
    ///close connection explicitly
    /** when connection is closed, reconnect feature is disabled
     * @retval true close is enqueued
     * @retval false connection is currently unavailable. You need to send close on_open
     */
    bool close() {return _ptr->close();}
protected:
    std::shared_ptr<IWebSocketClient> _ptr;
};


class IRestClient {
public:
    virtual ~IRestClient() = default;

    ///Http methods
    enum class Method {
        GET, PUT, POST, DELETE
    };


    using Headers = std::vector<std::pair<std::string_view, std::string_view> >;
    using HeadersIList = std::initializer_list<std::pair<std::string_view, std::string_view> >;

    struct Status {
        int code;
        std::string_view message;
    };

    class IEvents {
    public:
        ///response received
        /**
         * @param status response status. If status == -1, connection has been lost
         * @param headers response headers
         * @param body response body 
         */
        virtual void on_response(const Status &status, 
                                const Headers &headers,
                                const std::string_view &body) noexcept = 0;
        
        ///RestClient is destroyed
        /**
         * This is last event, you can destroy the implementation object 
         */
        virtual void on_destroy() noexcept = 0;
    };


    virtual void request(std::string_view path, const HeadersIList &hdrs) = 0;
    virtual void request(Method m, std::string_view path, const HeadersIList &hdrs, std::string_view body) = 0;

};

inline constexpr std::string_view to_string(IRestClient::Method m) {
    switch (m) {
        case IRestClient::Method::GET: return "GET";
        case IRestClient::Method::POST: return "POST";
        case IRestClient::Method::PUT: return "PUT";
        case IRestClient::Method::DELETE: return "DELETE";
        default: return "<unknown>";
    }
} 

///HTTP Rest client
/** Contains single connection to HTTP REST interface. It can process multiple requests
 *  You can pipe-line requests (however they are queued on local side). All responses
 * are passed througgh IEvents interface */
class RestClient {
public:
    using Method = IRestClient::Method;
    using IEvents = IRestClient::IEvents;
    using Headers = IRestClient::Headers;
    using HeadersIList = IRestClient::HeadersIList;

    RestClient(std::shared_ptr<IRestClient> ptr):_ptr(ptr) {};

    ///Send GET request
    /**
     * @param path relative path (relative to base address). Can contain query ?
     * @param hdrs optional headers. Header is defined as list of pairs {key, value}, all strings
     * @note response is passed through IEvents. It is possible to enqueue multiple requests, they
     * are returned in order of creation */
    void request(std::string_view path, const HeadersIList &hdrs = {}) {
                    _ptr->request(path, hdrs);
    }

    ///Send generic request with a body
    /** 
     * @param m method
     * @param path relative path (relative to base address). Can contain query ?
     * @param hdrs optional headers. Header is defined as list of pairs {key, value}, all strings
     * @param body request body
     * @note response is passed through IEvents. It is possible to enqueue multiple requests, they
     * are returned in order of creation */
    void request(Method m, std::string_view path, const HeadersIList &hdrs, std::string_view body) {
                    _ptr->request(m, path, hdrs, body);
    }

protected:
    std::shared_ptr<IRestClient> _ptr;
};

struct WebSocketConfig {
        ///optional, specify requested protocols
        std::string protocols = {};
        ///specifies ping interval in seconds
        unsigned int ping_interval = 10;
        ///if set true, then ping is always send in specified interval. 
        ///If false, ping is send only if there are no incomning data
        bool force_ping = false;
        ///Set true to automatically reconnect when connection is lost
        /**if reconnect is false, lost connection is auto closed */
        bool reconnect = true;
    };


class INetwork {
public:
    virtual ~INetwork() = default;

    using WSConfig = WebSocketConfig;

   
    class IPrivKey {
    public:
        virtual ~IPrivKey() = default;
    };

    ///Abstract class to hold a private key
    using PrivKey = std::shared_ptr<const IPrivKey>;

    ///create a websocket client
    /**
     * @param events reference to instance of class implementing IEvents. This reference must be valid
     *               until on_close() is called
     * @param url url to websocket server (ws:// or wss://)
     * @param cfg optional configuration */
    virtual WebSocketClient create_websocket_client(
            WebSocketClient::IEvents &events,std::string url,  WSConfig cfg = {}) const = 0;
    
    ///create a rest client 
    /**
     * @param events reference to instance of class implementing IEvents. This reference must be valid
     *               until on_close() is called
     * @param base_url base url (https://<domain:port>). The last / is optional, however it affect how
     *  path is combined with base url. If the base url ends with /, then path should not start with /
     * @param iotimeout_ms specifies I/O timeout.
     */
    virtual RestClient create_rest_client(RestClient::IEvents &events,
                                 std::string base_url, unsigned int iotimeout_ms = 10000) const = 0;   
    
    ///calculate hmac256 of message and key
    /** 
     * @param key key
     * @param msg message
     * @return digest
     */
    virtual std::basic_string<unsigned char> calc_hmac256(std::string_view key, std::string_view msg) const = 0;
    
    ///reads private key from file (PEM)
    /** 
     * @param file_name name of file
     * @return Private key object 
     */
    virtual PrivKey priv_key_from_file(std::string_view file_name) const = 0;
    ///reads private key from string (PEM)
    /** 
     * @param file_name string containing key in PEM
     * @return Private key object 
     */
    virtual PrivKey priv_key_from_string(std::string_view priv_key_str) const = 0;

    ///Sign a message
    /** 
     * @param message message to sign
     * @param pk private key;
     * @return signature
     */
    virtual std::basic_string<unsigned char> sign_message(std::string_view message,const PrivKey &pk) const = 0;

    virtual std::string make_query(std::span<const std::pair<std::string_view, std::string_view> > fields) const = 0;

    class Null;
};

class INetwork::Null: public INetwork {
public:
    [[noreturn]] void error() const {throw std::runtime_error("not implemented");}
    
    virtual WebSocketClient create_websocket_client(WebSocketClient::IEvents &,std::string,  WSConfig) const override {error();}
    virtual RestClient create_rest_client(RestClient::IEvents &,std::string , unsigned int) const override {error();}   
    virtual std::basic_string<unsigned char> calc_hmac256(std::string_view , std::string_view) const override {error();}   
    virtual PrivKey priv_key_from_file(std::string_view) const override {error();} 
    virtual PrivKey priv_key_from_string(std::string_view) const override {error();}
    virtual std::basic_string<unsigned char> sign_message(std::string_view ,const PrivKey &) const override {error();}
    virtual std::string make_query(std::span<const std::pair<std::string_view, std::string_view> > ) const override {error();}
};

class Network: public Wrapper<INetwork> {
public:
    using Wrapper<INetwork>::Wrapper;

    using WSConfig = INetwork::WSConfig;
    using PrivKey = INetwork::PrivKey;


    ///create a websocket client
    /**
     * @param events reference to instance of class implementing IEvents. This reference must be valid
     *               until on_close() is called
     * @param url url to websocket server (ws:// or wss://)
     * @param cfg optional configuration */
    WebSocketClient create_websocket_client(
            WebSocketClient::IEvents &events,std::string url,  WSConfig cfg = {}) const {
                return _ptr->create_websocket_client(events,std::move(url), std::move(cfg));
            }
    
    ///create a rest client 
    /**
     * @param events reference to instance of class implementing IEvents. This reference must be valid
     *               until on_close() is called
     * @param base_url base url (https://<domain:port>). The last / is optional, however it affect how
     *  path is combined with base url. If the base url ends with /, then path should not start with /
     * @param iotimeout_ms specifies I/O timeout.
     */
    RestClient create_rest_client(RestClient::IEvents &events,
                                 std::string base_url, unsigned int iotimeout_ms = 10000) const {
                                    return _ptr->create_rest_client(events, std::move(base_url), iotimeout_ms);
                                 }
    
    ///calculate hmac256 of message and key
    /** 
     * @param key key
     * @param msg message
     * @return digest
     */
    std::basic_string<unsigned char> calc_hmac256(std::string_view key, std::string_view msg) const {
        return _ptr->calc_hmac256(key, msg);
    }
    
    ///reads private key from file (PEM)
    /** 
     * @param file_name name of file
     * @return Private key object 
     */
    PrivKey priv_key_from_file(std::string_view file_name) const {
        return _ptr->priv_key_from_file(file_name);
    }
    ///reads private key from string (PEM)
    /** 
     * @param file_name string containing key in PEM
     * @return Private key object 
     */
    PrivKey priv_key_from_string(std::string_view priv_key_str) const {
        return _ptr->priv_key_from_string(priv_key_str);
    }

    ///Sign a message
    /** 
     * @param message message to sign
     * @param pk private key;
     * @return signature
     */
    std::basic_string<unsigned char> sign_message(std::string_view message,const PrivKey &pk) const {
        return _ptr->sign_message(message, pk);
    }

    std::string make_query(std::initializer_list<std::pair<std::string_view, std::string_view> > fields) const {
        return _ptr->make_query({fields.begin(), fields.size()});
    }


};

}