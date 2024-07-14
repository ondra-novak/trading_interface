#pragma once


#include "atomic_future.h"
#include "../trading_ifc/order.h"
#include "../trading_ifc/instrument.h"

namespace trading_api {

class AssociatedOrder: public IOrder::Null {
public:

    AssociatedOrder(Instrument instrument, Account account);

    virtual Instrument get_instrument() const override;
    virtual Account get_account() const override;
    virtual State get_state() const override;
    virtual SerializedOrder to_binary() const override {return {};}
    virtual Origin get_origin() const override {return Origin::strategy;}

protected:
    Instrument _instrument;
    Account _account;
};

class ErrorOrder: public AssociatedOrder {
public:
    ErrorOrder(Instrument instrument, Account account, Reason r, std::string message);

    virtual State get_state() const override;
    virtual Reason get_reason() const override;
    virtual std::string_view get_message() const override;
    virtual Origin get_origin() const override {return Origin::strategy;}
protected:
    Reason _r;
    std::string _message;
};

class BasicOrder: public IOrder {
public:

    struct Status {
        double filled = 0;
        double last_price = 0;
        Order::Report last_report = {State::sent, Reason::no_reason, {}};

        void add_fill(double price, double amount);
        void update_report(Order::Report report);
    };


    BasicOrder(Instrument instrument, Account account, Setup setup, Origin origin);
    BasicOrder(Order replaced, Setup setup, bool amend, Origin origin);
    virtual State get_state() const override;
    virtual double get_last_price() const override;
    virtual std::string_view get_message() const override;
    virtual double get_filled() const override;
    virtual const Setup &get_setup() const override;
    virtual Reason get_reason() const override;
    virtual Instrument get_instrument() const override;
    virtual SerializedOrder to_binary() const override {return {};}
    virtual Origin get_origin() const override ;
    virtual Account get_account() const override;
    virtual std::string get_id() const override;

    Status &get_status() const {
        return _status;
    }

    Order get_replaced_order() const;



protected:


    Setup _setup;
    Instrument _instrument;
    Account _account;
    Origin _origin;
    std::weak_ptr<const IOrder> _replaced;
    bool _amend = false;

    mutable Status _status;


};



}
