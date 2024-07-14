#pragma once

#include "../trading_ifc/strategy_context.h"
#include <unordered_set>
#include <list>

namespace trading_api {

class IStorage {
public:

    virtual ~IStorage() = default;
    ///begin transaction - all put or erase are now stored into single transaction
    virtual void begin_transaction() = 0;
    ///put strategy custom variable
    virtual void put_var(std::string_view name, std::string_view value) = 0;
    ///erase strategy custom variable
    virtual void erase_var(std::string_view name) = 0;
    ///put order (called on every order update)
    virtual void put_order(const Order &ord) = 0;
    ///put fill (called on fill)
    virtual void put_fill(const Fill &fill) = 0;
    ///commit all writes
    virtual void commit() = 0;
    ///discard writes
    virtual void rollback() = 0;
    ///determine, whether given fill is duplicate
    virtual bool is_duplicate_fill(const Fill &fill) const = 0;
    ///load recent fills
    /**
     * @param limit limit in count
     * @param flt_instrument filter for given instrument
     * @param flt_account filter for given account
     * @return fills
     */
    virtual Fills load_fills(std::size_t limit, std::string_view filter = {}) const = 0;
    ///load recent fills
    /**
     * @param limit limit as old timestamp. No older fills are returned
     * @param flt_instrument filter for given instrument
     * @param flt_account filter for given account
     * @return fills
     */
    virtual Fills load_fills(Timestamp limit, std::string_view filter = {}) const = 0;
    ///load all open orders (stored binary)
    virtual std::vector<SerializedOrder> load_open_orders() const = 0;

    virtual std::string get_var(std::string_view var_name) const = 0;

    virtual void enum_vars(std::string_view start, std::string_view end,
            Function<void(std::string_view,std::string_view)> &fn) const = 0;

    virtual void enum_vars(std::string_view prefix,
            Function<void(std::string_view,std::string_view)> &fn) const = 0;


    class Null;

};

class IStorage::Null: public IStorage {
public:
    virtual void rollback() override {}
    virtual void begin_transaction() override {}
    virtual void put_order(const Order &) override {}
    virtual void erase_var(std::string_view ) override {}
    virtual void put_fill(const Fill &) override {}
    virtual void commit() override {}
    virtual bool is_duplicate_fill(const Fill &) const override {return false;} ;
    virtual void put_var(std::string_view , std::string_view ) override {}
    virtual std::vector<SerializedOrder> load_open_orders() const override {return {};}
    virtual Fills load_fills(std::size_t, std::string_view) const override{return {};}
    virtual Fills load_fills(Timestamp ,std::string_view) const  override{return {};}
    virtual std::string get_var(std::string_view ) const override {return {};}
    virtual void enum_vars(std::string_view , std::string_view ,
             Function<void(std::string_view,std::string_view)> &) const override {}
    virtual void enum_vars(std::string_view ,
            Function<void(std::string_view,std::string_view)> &) const override {}
};


}
