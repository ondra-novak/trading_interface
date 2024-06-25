#pragma once
#include <memory>

#include "../trading_ifc/instrument.h"
#include "../trading_ifc/account.h"
#include "../trading_ifc/orderbook.h"
#include "../trading_ifc/ticker.h"
#include "../trading_ifc/order.h"
#include "../trading_ifc/fill.h"
#include "../trading_ifc/function.h"
#include "basic_order.h"

namespace trading_api {


class IEventTarget;


using PEventTarget = std::shared_ptr<IEventTarget>;
using WPEventTarget = std::weak_ptr<IEventTarget>;

///Represents strategy (with context) from service provider side
/**
 * Each strategy is represented by this interface. When service provider detects
 * a market event, it calls a function on_event defined on this interface
 */
class IEventTarget {
public:

    virtual ~IEventTarget () {}
    ///called when update of an instrument is finished
    virtual void on_event(const Instrument &i) = 0;

    ///called when update on an account is finished
    virtual void on_event(const Account &a) = 0;

    ///called when ticker is updated
    virtual void on_event(const Instrument &i, const Ticker &tk) = 0;

    ///called when orderbook is updated
    virtual void on_event(const Instrument &i, const OrderBook &ord) = 0;

    ///called when order state changed
    virtual void on_event(const PBasicOrder &order, Order::State state) = 0;

    ///called when order state changed
    virtual void on_event(const PBasicOrder &order, Order::State state, Order::Reason reason, const std::string &message = {}) = 0;

    ///called when fill is detected
    virtual void on_event(const PBasicOrder &order, const Fill &fill) = 0;


};


}
