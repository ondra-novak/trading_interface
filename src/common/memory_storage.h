#pragma once
#include "storage.h"

namespace trading_api {

class MemoryStorage: public IStorage {
public:
    virtual void rollback() override;
    virtual void begin_transaction() override;
    virtual void put_order(const Order &ord) override;
    virtual void erase_var(std::string_view name) override;
    virtual void put_fill(const Fill &fill) override;
    virtual void commit() override;
    virtual bool is_duplicate_fill(const Fill &fill) const
            override;
    virtual void put_var(std::string_view name, std::string_view value)
            override;
    virtual std::vector<SerializedOrder> load_open_orders() const override;
    virtual Fills load_fills(std::size_t limit, std::string_view filter) const override;
    virtual Fills load_fills(Timestamp limit, std::string_view filter) const override;
    virtual std::string get_var(std::string_view var_name) const override;
    virtual void enum_vars(std::string_view prefix,
                 Function<void(std::string_view,std::string_view)> &fn) const  override;
    virtual void enum_vars(std::string_view start, std::string_view end,
                 Function<void(std::string_view,std::string_view)> &fn) const  override;

protected:

    struct TrnEraseVar {
        std::string name;
        void operator()(MemoryStorage *);
    };

    struct TrnPutVar {
        std::string name;
        std::string value;
        void operator()(MemoryStorage *);
    };

    struct TrnPutOrder {
        Order order;
        void operator()(MemoryStorage *);
    };

    struct TrnPutFill {
        Fill fill;
        void operator()(MemoryStorage *);
    };

    using TrnItem = std::variant<TrnEraseVar, TrnPutVar, TrnPutOrder, TrnPutFill>;

    std::vector<TrnItem> _transaction = {};
    int _transaction_counter = 0;

    void store(TrnItem item);



    std::map<std::string, std::string, std::less<> > _vars = {};
    std::map<std::string, std::string> _orders = {};
    Fills _fills = {};

};


}

