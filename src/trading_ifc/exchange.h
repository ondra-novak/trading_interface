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
    virtual void list_instruments(Function<void(const Instrument &)> fn) const = 0;
    class Null;
};


class IExchange::Null: public IExchange {
public:
    virtual void list_instruments(Function<void(const Instrument &)> ) const override {}
    virtual std::string get_label() const override {return {};}
    virtual std::string get_name() const override  {return {};}
    virtual std::string get_id() const override  {return {};}
    virtual std::optional<Icon> get_icon() const override {return {};}
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

    ///List of public instruments
    /**
     * Enumerate list of public instruments available for trading or data
     * on the exchange.
     *
     * @param cb callback function, which retreieves instruments by one by
     *
     * @note you can probably subscribe for data
     * @note this feature is optional, exchange don't need to return any instrument
     * @note it is also possible, that exchange will not return instrument which
     * is configured for the strategy. The strategy should trade only configured
     * instruments,
     *
     */
    template<std::invocable<const Instrument &> Fn>
    void list_instruments(Fn &&cb) {
        _ptr->list_instruments(std::forward<Fn>(cb));
    }

    ///Retrieve user defined label (configured for this exchange)
    std::string get_label() const {return _ptr->get_label();}
    ///Retrieve name of exchange
    std::string get_name() const {return _ptr->get_name();}
    ///Retrieves internal ID of the exchange (can be empty)
    std::string get_id() const  {return _ptr->get_id();}
    ///Retrieves icon. This feature is optional.
    std::optional<Icon> get_icon() const {return _ptr->get_icon();}
};


}

