#pragma once

#include <vector>
#include <memory>
#include <chrono>
#include <string_view>


namespace trading_ifc {


using PositionID = std::intptr_t;

class Instrument;

class IAccount {
public:
    struct Info {
        double equity = 0;
        double balance = 0;
        double blocked = 0;
        double leverage = 0;
    };

    enum class Side {
        buy = 1,
        sell = -1
    };

    struct Position {
        PositionID id = {};    //id of position
        Side side;
        double amount = 0;      //position size (negative is short)
        double open_price = 0;  //open price (if 0 then unknown)
    };

    struct HedgePosition {
        Position buy = {};
        Position sell = {};     //note always emits negative position
    };

    using PositionList = std::vector<Position>;

    constexpr virtual ~IAccount() = default;


    virtual Info get_info() const = 0;
    ///Retrieves position on given instrument
    /**
     * @param instrument instrument.
     * @return overall position. If exchange tracks each opened position, it
     * returns overall position where shorts are substracted from longs, etc
     *
     * @note short position has negative amount
     */
    virtual Position get_position(const Instrument &instrument) const = 0;

    ///Retrieves hedge position info
    /**
     * If exchange doesn't support hedge position, it returns overall position
     * depend on current side
     * @param instrument instrument
     * @return overall position for both sides
     */
    virtual HedgePosition get_hedge_position(const Instrument &instrument) const = 0;

    ///Retrieves all positions
    /**
     * Can return one position for standard futures, but multiple positions
     * on CFD
     *
     * @param instrument instrument
     *
     * @return list of positions
     */
    virtual PositionList get_all_positions(const Instrument &instrument) const = 0;
};


class NullAccount: public IAccount{
public:
    virtual Info get_info() const override {return {};}
    virtual Position get_position(const Instrument &)const  override {return {};}

    virtual HedgePosition get_hedge_position(const Instrument &) const override {return {};}
    virtual PositionList get_all_positions(const Instrument &) const override {return {};}
    constexpr virtual ~NullAccount() {}
};



///the account on which the particulal symbol is traded
/**
 * @note the account can be part of user account on the exchange. On
 * spot exchange, the account is mapped on symbol which represents
 * currency side. On futures account, this can be mapped to collateral
 * account type. One user account can have multiple such accounts.
 */
class Account {
public:

    static constexpr NullAccount null_account = {};
    static std::shared_ptr<const IAccount> null_account_ptr;

    using Info = IAccount::Info;
    using Position = IAccount::Position;
    using HedgePosition = IAccount::HedgePosition;
    using PositionList = IAccount::PositionList;

    Account():_ptr(null_account_ptr) {}
    Account(std::shared_ptr<const IAccount> x): _ptr(std::move(x)) {}

    bool operator==(const Account &other) const = default;
    std::strong_ordering operator<=>(const Account &other) const = default;


    struct Hasher {
        auto operator()(const Account &ord) const {
            std::hash<std::shared_ptr<const IAccount> > hasher;
            return hasher(ord._ptr);
        }
    };

protected:
    std::shared_ptr<const IAccount> _ptr;
};

inline std::shared_ptr<const IAccount> Account::null_account_ptr = {&Account::null_account, [](auto){}};


}
