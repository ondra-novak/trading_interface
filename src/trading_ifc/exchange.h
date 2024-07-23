#pragma once
#include "instrument.h"
#include "function.h"

#include "wrapper.h"
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace trading_api {

class Instrument;
struct Ticker;
class OrderBook;


class IExchange {
public:

    struct Icon {
        ///icon data (binary)
        std::string_view data;
        ///content type - for example image/png, image/jpeg, etc
        std::string_view content_type;
    };


    virtual ~IExchange() = default;
    virtual std::string get_id() const = 0;
    virtual std::string get_label() const = 0;
    virtual std::string get_name() const = 0;
    virtual std::optional<Icon> get_icon() const = 0;
    virtual bool get_last_ticker(const Instrument &instrument, Ticker &tk) const = 0;
    virtual bool get_last_orderbook(const Instrument &instrument, OrderBook &ordb) const = 0;
    class Null;
};


class IExchange::Null: public IExchange {
public:
    virtual std::string get_label() const override {return {};}
    virtual std::string get_name() const override  {return {};}
    virtual std::string get_id() const override  {return {};}
    virtual std::optional<Icon> get_icon() const override {return {};}
    virtual bool get_last_ticker(const Instrument &, Ticker &) const override {return false;}
    virtual bool get_last_orderbook(const Instrument &, OrderBook &) const override {return false;}
};


///Information about exchange
/**
 * Contains various informations about exchange
 * You can use this instance to retrieve connection between account
 * and instrument.
 *
 * You can only trade account and instrument at the same exchange
 *
 *
 */
class Exchange: public Wrapper<IExchange> {
public:

    using Icon = IExchange::Icon;

    using Wrapper<IExchange>::Wrapper;


    ///Retrieve user defined label (configured for this exchange)
    std::string get_label() const {return _ptr->get_label();}
    ///Retrieve name of exchange
    std::string get_name() const {return _ptr->get_name();}
    ///Retrieves internal ID of the exchange (can be empty)
    std::string get_id() const  {return _ptr->get_id();}
    ///Retrieves icon. This feature is optional.
    std::optional<Icon> get_icon() const {return _ptr->get_icon();}

    ///Retrieve last ticker synchronously
    /**
     * @param instrument instrument object
     * @param tk variable which receives ticker
     * @retval true received
     * @retval false there is no last ticker stored
     *
     * @note this function is intended for internal use. You need to use
     * context to subscribe instrument's data stream
     *
     */

    bool get_last_ticker(const Instrument &instrument, Ticker &tk) const {
        return _ptr->get_last_ticker(instrument, tk);
    }

    ///Retrieve last orderbook state synchronously
    /**
     * @param instrument instrument object
     * @param ordb variable which receives orderbook
     * @retval true received
     * @retval false cannot be retrieved synchronously
     * @note this function is intended for internal use. You need to use
     * context to subscribe instrument's data stream
     */
    bool get_last_orderbook(const Instrument &instrument, OrderBook &ordb) const {
        return _ptr->get_last_orderbook(instrument, ordb);
    }
};


}

