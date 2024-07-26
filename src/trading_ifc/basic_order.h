#pragma once

#include "instrument.h"
#include "account.h"
#include "order.h"

namespace trading_api {

///For exchanges - associated order (no
class AssociatedOrder: public IOrder::Null {
public:

    AssociatedOrder(Instrument instrument, Account account)
        :_instrument(std::move(instrument)),_account(std::move(account)) {}

    virtual Instrument get_instrument() const override {return _instrument;}
    virtual Account get_account() const override {return _account;}
    virtual State get_state() const override {return State::associated;}
    virtual SerializedOrder to_binary() const override {return {};}
    virtual Origin get_origin() const override {return Origin::strategy;}

protected:
    Instrument _instrument;
    Account _account;
};

class ErrorOrder: public AssociatedOrder {
public:
    ErrorOrder(Instrument instrument, Account account, Reason r, std::string message)
        :AssociatedOrder(std::move(instrument),std::move(account))
        ,_r(r),_message(std::move(message)) {}

    virtual State get_state() const override {return State::discarded;}
    virtual Reason get_reason() const override {return _r;}
    virtual std::string_view get_message() const override {return _message;}
    virtual Origin get_origin() const override {return Origin::strategy;}
protected:
    Reason _r;
    std::string _message;
};

///Create error order (create_order cannot throw exception)
Order order_error(Instrument instrument, Account account, Order::Reason r, std::string msg) {
    return Order(std::make_shared<ErrorOrder>(
            std::move(instrument),
            std::move(account),
            r,std::move(msg)));
}

class BasicOrder: public IOrder {
public:

    struct Status {
        std::string id = {};
        double filled = 0;
        double last_price = 0;
        Order::Report last_report = {State::sent, Reason::no_reason, {}};

        void add_fill(double price, double amount);
        void update_report(Order::Report report);
    };


    BasicOrder(Instrument instrument, Account account, Setup setup, Origin origin)
        :_instrument(std::move(instrument))
        ,_account(std::move(account))
        ,_setup(std::move(setup))
        ,_origin(std::move(origin)) {}
    BasicOrder(Order replaced, Setup setup, bool amend, Origin origin)
        :_instrument(replaced.get_instrument())
        ,_account(replaced.get_account())
        ,_setup(std::move(setup))
        ,_origin(std::move(origin))
        ,_replaced(replaced.get_handle())
        ,_amend(amend) {}
    virtual State get_state() const override {
        return _status.last_report.new_state;
    }
    virtual double get_last_price() const override {
        return _status.last_price;
    }
    virtual std::string_view get_message() const override {
        return _status.last_report.message;
    }
    virtual double get_filled() const override {
        return _status.filled;
    }
    virtual const Setup &get_setup() const override {
        return _setup;
    }
    virtual Reason get_reason() const override {
        return _status.last_report.reason;
    }
    virtual Instrument get_instrument() const override {
        return _instrument;
    }
    virtual SerializedOrder to_binary() const override {return {_status.id,{} };}
    virtual Origin get_origin() const override  {return _origin;}
    virtual Account get_account() const override {return _account;}
    virtual std::string get_id() const override {return _status.id;}

    Status &get_status() const {
        return _status;
    }

    Order get_replaced_order() const {
        auto lk = _replaced.lock();
        if (lk) return Order(lk);
        else return Order();
    }

    static const BasicOrder &from_order(const Order &ord) {
        return dynamic_cast<const BasicOrder &>(*ord.get_handle());
    }

protected:

    Instrument _instrument;
    Account _account;
    Setup _setup;
    Origin _origin;
    std::weak_ptr<const IOrder> _replaced;
    bool _amend = false;

    mutable Status _status;


};



}
