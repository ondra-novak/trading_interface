#pragma once

#include <vector>
#include <memory>
#include <chrono>
#include <string_view>
#include "instrument.h"


namespace trading_api {



class Instrument;

class IAccount {
public:
    struct Info {
        double equity = 0;
        double balance = 0;
        double blocked = 0;
        double leverage = 0;
        std::string currency = {};      //currency symbol - depend on exchange
        double ratio = 0;               //ratio to main currency (if zero - unknown)
    };


    virtual ~IAccount() = default;


    virtual Info get_info() const = 0;
    virtual std::string get_label() const = 0;

    virtual Exchange get_exchange() const = 0;

    ///Retrieve internal instrument ID
    virtual std::string get_id() const = 0;


    class Null;
};


class IAccount::Null: public IAccount{
public:
    virtual Info get_info() const override {return {};}
    virtual std::string get_label() const override {return {};}
    virtual Exchange get_exchange() const override {return {};}
    virtual std::string get_id() const override {return {};}
};



///the account on which the particulal symbol is traded
/**
 * @note the account can be part of user account on the exchange. On
 * spot exchange, the account is mapped on symbol which represents
 * currency side. On futures account, this can be mapped to collateral
 * account type. One user account can have multiple such accounts.
 */
class Account: public Wrapper<IAccount> {
public:

    using Wrapper<IAccount>::Wrapper;

    using Info = IAccount::Info;


    ///Retrieve account's label
    /** Account's label can be defined in config */
    std::string get_label() const {return _ptr->get_label();}


    Info get_info() const {return _ptr->get_info();}

    ///Retrieve exchange instance, where this account is managed
    Exchange get_exchange() const {return _ptr->get_exchange();}

    std::string get_id() const {return _ptr->get_id();}


};



}
