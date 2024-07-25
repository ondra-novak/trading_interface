#pragma once

#include "common.h"
#include "acb.h"
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
        double leverage = 0;            //min common leverage, if zero - unknown
        std::string currency = {};      //currency symbol - depend on exchange
        double ratio = 0;               //ratio to main currency (if zero - unknown)
    };


    struct Position {
        std::string id;
        Side side;
        double open_price;
        double amount;
        double leverage;                //position leverage if zero - unknown
    };

    struct AggregatedPosition {
        ///final side of the aggregated position
        Side side = Side::undefined;
        ///open price of this position
        double open_price = 0;
        ///amount of position
        double amount = 0;
        ///locked pnl - nonzero if hedge positions
        double locked_pnl = 0;
    };

    class Positions : public std::vector<Position>{
    public:
        using std::vector<Position>::vector;

        ///aggregates all positions into one
        template<Side skip = Side::undefined>
        AggregatedPosition aggregated()const {
            AggregatedPosition out;
            ACB acb;
            for (const Position &pos: *this) {
                if (pos.side == skip) continue;
                acb = acb(pos.open_price, pos.amount * static_cast<double>(pos.side));
            }
            double a= acb.getPos();
            out.side = a<0?Side::sell:a>0?Side::buy:Side::undefined;
            out.amount = a * static_cast<double>(out.side);
            out.locked_pnl = acb.getRPnL();
            out.open_price = acb.getOpen();
            return out;
        }
        ///aggregate all buy positions (in hedge mode)
        AggregatedPosition aggregated_buy() const {return aggregated<Side::sell>();}
        ///aggregate all sell positions (in buy mode)
        AggregatedPosition aggregated_sell() const {return aggregated<Side::buy>();}
    };

    virtual ~IAccount() = default;


    virtual Info get_info() const = 0;
    virtual std::string get_label() const = 0;

    virtual Exchange get_exchange() const = 0;

    ///Retrieve internal instrument ID
    virtual std::string get_id() const = 0;

    virtual Positions get_positions(const Instrument &i) const = 0;


    class Null;
};


class IAccount::Null: public IAccount{
public:
    virtual Info get_info() const override {return {};}
    virtual std::string get_label() const override {return {};}
    virtual Exchange get_exchange() const override {return {};}
    virtual std::string get_id() const override {return {};}
    virtual Positions get_positions(const Instrument &) const override {return {};}
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
    using Position = IAccount::Position;
    using Positions = IAccount::Positions;
    using AggregatedPosition = IAccount::AggregatedPosition;


    ///Retrieve account's label
    /** Account's label can be defined in config */
    std::string get_label() const {return _ptr->get_label();}


    Info get_info() const {return _ptr->get_info();}

    ///Retrieve exchange instance, where this account is managed
    Exchange get_exchange() const {return _ptr->get_exchange();}

    std::string get_id() const {return _ptr->get_id();}

    Positions get_positions(const Instrument &i) const {return _ptr->get_positions(i);}

};



}
