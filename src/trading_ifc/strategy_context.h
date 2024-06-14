#pragma once

#include "abstract_order.h"
#include "order_params.h"
#include "fill.h"
#include "ticker.h"

namespace trading_ifc {


class Instrument;
class Account;
class Order;
struct OrderState;

using Timestamp = std::chrono::system_clock::time_point;
using TimerID = unsigned int;

using Fills = std::vector<Fill>;


class IContext {
public:

    virtual ~IContext() = default;

    ///request update account
    virtual void update_account(const Account &a) = 0;

    ///request update instrument
    virtual void update_instrument(const Instrument &i) = 0;

    ///Retrieves current time
    virtual Timestamp get_current_time() const = 0;

    ///Sets timer, which triggers event, on strategy interface
    virtual TimerID set_timer(Timestamp at) = 0;

    ///Cancel timer
    virtual bool clear_timer(TimerID id) = 0;

    ///Place an order
    virtual Order place(const Instrument &instrument, const OrderParams &params) = 0;

    ///Creates an order, which is asociated with an instrument, but it is not placed
    /**
     * You can use replace() function to place the order with new params. This
     * allows to track single order without need to know, whether order is actually
     * placed or not
     *
     * @param instrument associated instrument
     * @return dummy order (can be replaced)
     */
    virtual Order create(const Instrument &instrument) = 0;

    ///Cancel given order
    virtual void cancel(const Order &order) = 0;

    ///Replace order
    /**
     * @param order order to replace
     * @param params new params of the order
     * @return new order
     *
     * @note if replace partially filled order, filled amount is perserved
     * @note replace can fail, if exchange cannot garanteed to replace order
     * without avoiding double execution. In this case, old order is canceled
     * new order is rejected
     */
    virtual Order replace(const Order &order, const OrderParams &params) = 0;

    ///Retrieve last fills
    virtual Fills get_fills(std::size_t limit) = 0;

    ///set persistent variable
    virtual void set_var(std::string_view name, double val) = 0;

    ///set persistent variable
    virtual void set_var(std::string_view name, int val) = 0;

    ///set persistent variable
    virtual void set_var(std::string_view name, long val) = 0;

    ///get persistent variable
    virtual double get_var(std::string_view name, double def_val) = 0;

    ///get persistent variable
    virtual int get_var(std::string_view name, int def_val) = 0;

    ///get persistent variable
    virtual long get_var(std::string_view name, long def_val) = 0;

    ///reset variable
    virtual void reset_var(std::string_view name) = 0;

};

class NullContext: public IContext {
public:
    [[noreturn]] void throw_error() const {throw std::runtime_error("Used uninitialized context");}
    virtual TimerID set_timer(Timestamp) override {throw_error();}
    virtual void cancel(const Order &) override {throw_error();};
    virtual Order replace(const Order &, const OrderParams &) override {throw_error();}
    virtual void update_instrument(const Instrument &) override {throw_error();}
    virtual int get_var(std::string_view , int ) override{throw_error();}
    virtual long int get_var(std::string_view , long int ) override{throw_error();}
    virtual Fills get_fills(std::size_t ) override{throw_error();}
    virtual void set_var(std::string_view , int ) override{throw_error();}
    virtual Timestamp get_current_time() const override{throw_error();}
    virtual bool clear_timer(TimerID ) override{throw_error();}
    virtual double get_var(std::string_view , double ) override{throw_error();}
    virtual void set_var(std::string_view , long int ) override{throw_error();}
    virtual void update_account(const Account &) override{throw_error();}
    virtual void set_var(std::string_view , double ) override{throw_error();}
    virtual Order create(const Instrument &)override{throw_error();}
    virtual void reset_var(std::string_view ) override{throw_error();}
    virtual Order place(const Instrument &,
            const OrderParams &) override{throw_error();}
    constexpr virtual ~NullContext() {}
};


class Context {
public:

    static constexpr NullContext null_context = {};
    static std::shared_ptr<IContext> null_context_ptr;

    Context():_ptr(null_context_ptr) {}
    Context(std::shared_ptr<IContext> ptr):_ptr(std::move(ptr)) {}


    ///request update account
    void update_account(const Account &a) {_ptr->update_account(a);}
    ///request update instrument
    void update_instrument(const Instrument &i) {_ptr->update_instrument(i);}
    ///Retrieves current time
    Timestamp get_current_time() const {return _ptr->get_current_time();}
    ///Sets timer, which triggers event, on strategy interface
    TimerID set_timer(Timestamp at) {return _ptr->set_timer(at);}
    ///Cancel timer
    bool clear_timer(TimerID id) {return _ptr->clear_timer(id);}
    ///Place an order
    Order place(const Instrument &instrument, const OrderParams &params) {return _ptr->place(instrument, params);}

    ///Cancel given order
    void cancel(const Order &order) {return _ptr->cancel(order);}

    ///Replace order
    /**
     * @param order order to replace.
     * @param params new params of the order
     * @return new order
     *
     * @note if replace partially filled order, filled amount is perserved
     * @note replace can fail, if exchange cannot garanteed to replace order
     * without avoiding double execution. In this case, old order is canceled
     * new order is rejected
     */
    Order replace(const Order &order, const OrderParams &params) {
        return _ptr->replace(order, params);
    }

    ///Retrieve last fills
    Fills get_fills(std::size_t limit) {
        return _ptr->get_fills(limit);
    }

    ///set persistent variable
    void set_var(std::string_view name, double val) {
        _ptr->set_var(name, val);
    }

    ///set persistent variable
    void set_var(std::string_view name, int val) {
        _ptr->set_var(name, val);
    }

    ///set persistent variable
    void set_var(std::string_view name, long val) {
        _ptr->set_var(name, val);

    }

    ///get persistent variable
    double get_var(std::string_view name, double def_val) {
        return _ptr->get_var(name, def_val);
    }

    ///get persistent variable
    int get_var(std::string_view name, int def_val) {
        return _ptr->get_var(name, def_val);
    }

    ///get persistent variable
    long get_var(std::string_view name, long def_val) {
        return _ptr->get_var(name, def_val);
    }

    ///reset variable
    void reset_var(std::string_view name) {
        _ptr->reset_var(name);
    }

protected:
    std::shared_ptr<IContext> _ptr;
};

inline std::shared_ptr<IContext> Context::null_context_ptr = {const_cast<NullContext *>(&Context::null_context), [](auto){}};

}

