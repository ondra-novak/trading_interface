#pragma once

#include "config.h"
#include "order.h"
#include "fill.h"

#include "log.h"
namespace trading_api {

class IEvantTarget;

///Counterpart object to IExchangeService - to communicate from exchange to core
class IExchangeContext {
public:


    virtual ~IExchangeContext() = default;

    ///call this function when ticker for given instrument arrived
    /**
     * @param i instrument
     * @param t ticker
     */
    virtual void income_data(const Instrument &i, const Ticker &t) = 0;
    ///call this function when orderbook for given instrument arrived
    /**
     * @param i instrument
     * @param o orderbook
     */
    virtual void income_data(const Instrument &i, const OrderBook &o) = 0;
    ///call this function when account is updated
    virtual void object_updated(const Account &i) = 0;
    ///call this function when instrument is updated
    virtual void object_updated(const Instrument &i) = 0;
    ///call this function when order's state changed
    /**
     * As the orders are const, you cannot change state of the order directly. The
     * state is changed during processing the event because orders are in possesion
     * of the strategy
     *
     * @param order order instance
     * @param state new state
     * @param reason optional reason (for the state)
     * @param message optional string message if error
     */
    virtual void order_state_changed(const Order &order, const Order::Report &report) = 0;
    ///call this function for every fill on the order
    /**
     * @param order order instance
     * @param fill fill information
     */
    virtual void order_fill(const Order &order, const Fill &fill) = 0;
    ///call this function for every restored order from set passed to restore_orders()
    /**
     * @param context pointer which has been passed to restore_orders.
     * @param order restored order instance
     */
    virtual void order_restore(void *context, const Order &order) = 0;


    ///Convert this object to exchange
    virtual Exchange get_exchange() const = 0;

    virtual Log get_log() const = 0;

    class Null;

};

class IExchangeContext::Null : public IExchangeContext{
public:
    [[noreturn]] void throw_error() const {throw std::runtime_error("Used uninitialized context");}
    virtual void order_state_changed(const Order &, const Order::Report & ) override {throw_error();}
    virtual void order_restore(void *, const Order &) override{throw_error();}
    virtual void order_fill(const Order &, const Fill &) override{throw_error();}
    virtual void income_data(const Instrument &, const OrderBook &) override{throw_error();}
    virtual void object_updated(const Account &) override{throw_error();}
    virtual void object_updated(const Instrument &) override{throw_error();}
    virtual void income_data(const Instrument &, const Ticker &) override{throw_error();}
    virtual Log get_log() const override {throw_error();}
    virtual Exchange get_exchange() const override {throw_error();}
};

class ExchangeContext {
public:

    static constexpr IExchangeContext::Null null_context = {};

    ExchangeContext():_ptr(const_cast<IExchangeContext::Null *>(&null_context)) {}
    ExchangeContext(IExchangeContext *ctx):_ptr(ctx) {}

    bool defined() const {return _ptr != &null_context;}
    explicit operator bool() const {return defined();}
    bool operator!() const {return !defined();}

    ///call this function when ticker for given instrument arrived
    /**
     * @param i instrument
     * @param t ticker
     */
    void income_data(const Instrument &i, const Ticker &t) {
        _ptr->income_data(i, t);
    }
    ///call this function when orderbook for given instrument arrived
    /**
     * @param i instrument
     * @param o orderbook
     */
    void income_data(const Instrument &i, const OrderBook &o) {
        _ptr->income_data(i, o);
    }
    ///call this function when account is updated
    void object_updated(const Account &a) {
        _ptr->object_updated(a);
    }
    ///call this function when instrument is updated
    void object_updated(const Instrument &i) {
        _ptr->object_updated(i);
    }
    ///call this function when order's state changed
    /**
     * As the orders are const, you cannot change state of the order directly. The
     * state is changed during processing the event because orders are in possesion
     * of the strategy
     *
     * @param order order instance
     * @param report report
     *
     * As result of this function, the strategy context will eventually call
     * order_apply_report (asynchronously in different thread)
     */
    void order_state_changed(const Order &order, const Order::Report &report) {
        _ptr->order_state_changed(order, report);
    }
    ///call this function for every fill on the order
    /**
     * @param order order instance
     * @param fill fill information
     *
     * As result of this function, the strategy context will eventually call
     * order_apply_fill(asynchronously in different thread)
     *
     */
    void order_fill(const Order &order, const Fill &fill) {
        return _ptr->order_fill(order, fill);
    }
    ///call this function for every restored order from set passed to restore_orders()
    /**
     * @param context pointer which has been passed to restore_orders.
     * @param order restored order instance
     */
    void order_restore(void *context, const Order &order) {
        return _ptr->order_restore(context, order);
    }

    ///convert to exchange
    Exchange get_exchange() const {
        return _ptr->get_exchange();
    }

    Log get_log() const {
        return _ptr->get_log();
    }


protected:
    IExchangeContext *_ptr;
};


}


