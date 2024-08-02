#include "../trading_ifc/network.h"

namespace trading_api {

class BasicNetwork: public INetwork {
public:
    virtual WebSocketClient create_websocket_client(
            WebSocketClient::IEvents &events,std::string url,  WSConfig cfg = {}) const override;
    virtual RestClient create_rest_client(RestClient::IEvents &events,
                                 std::string base_url, unsigned int iotimeout_ms = 10000) const override;
    virtual std::basic_string<unsigned char> calc_hmac256(std::string_view key, std::string_view msg) const override;
    virtual PrivKey priv_key_from_file(std::string_view file_name) const override;
    virtual PrivKey priv_key_from_string(std::string_view priv_key_str) const override;
    virtual std::basic_string<unsigned char> sign_message(std::string_view message,const PrivKey &pk) const override;
    virtual std::string make_query(std::span<const std::pair<std::string_view, std::string_view> > fields) const override;


};

}