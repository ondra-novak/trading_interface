#pragma once

#include "coro_support.h"
#include "fill.h"
#include "order.h"
#include "ticker.h"
#include "timer.h"
#include "config.h"
#include <span>

namespace trading_api {


using CompletionCB = Function<void()>;

template<typename T>
concept BinarySerializable = std::is_trivially_copy_constructible_v<T>;




enum class SubscriptionType {
    ticker,
    orderbook,
};

class IContext {
public:

    virtual ~IContext() = default;

    ///request update account
    virtual void update_account(const Account &a, CompletionCB complete_ptr) = 0;

    ///request update instrument
    virtual void update_instrument(const Instrument &i, CompletionCB complete_ptr) = 0;

    ///Retrieves current time
    virtual Timestamp now() const = 0;


    ///Sets time, which calls function wrapped into runnable object. It is still bound to strategy
    virtual TimerID set_timer(Timestamp at, CompletionCB fnptr = {}) = 0;

    ///Cancel timer
    virtual bool clear_timer(TimerID id) = 0;

    ///Place an order
    virtual Order place(const Instrument &instrument, const Order::Setup &setup) = 0;

    ///Creates and bind an order to an instrument
    /**
     * You can use replace() function to place the order with new setup. This
     * allows to track single order without need to know, whether order is actually
     * placed or not
     *
     * @param instrument associated instrument
     * @return dummy order (can be replaced)
     */
    virtual Order bind_order(const Instrument &instrument) = 0;

    ///Cancel given order
    virtual void cancel(const Order &order) = 0;

    ///Replace order
    /**
     * @param order order to replace
     * @param setup new setup of the order
     * @return new order
     *
     * @note if replace partially filled order, filled amount is perserved
     * @note replace can fail, if exchange cannot garanteed to replace order
     * without avoiding double execution. In this case, old order is canceled
     * new order is rejected
     */
    virtual Order replace(const Order &order, const Order::Setup &setup, bool amend) = 0;

    ///Retrieve recent fills
    virtual Fills get_fills(const Instrument &i, std::size_t limit) const = 0;

    ///Retrieve recent fills
    virtual Fills get_fills(const Instrument &i, Timestamp tp) const = 0;

    ///set persistent variable
    /**
     * It is expected, that key-value database is involved.
     * @param var_name variable name - any binary content is allowed
     * @param value value as string - any binary content is allowed
     */
    virtual void set_var(std::string_view var_name, std::string_view value) = 0;

    ///Deletes persistently stored variable
    /**
     * @param var_name name of variable.
     *
     * @note Deleted variable releases some space in database and it
     * also stops appearing in list of variables supplied during init()
     */
    virtual void unset_var(std::string_view var_name) = 0;

    ///allocate equity on given account
    virtual void allocate(const Account &a, double equity) = 0;

    ///subscribe market events
    virtual void subscribe(SubscriptionType type, const Instrument &i) = 0;

    ///unsubscribe instrument
    virtual void unsubscribe(SubscriptionType type, const Instrument &i) = 0;

};

class NullContext: public IContext {
public:
    [[noreturn]] void throw_error() const {throw std::runtime_error("Used uninitialized context");}
    virtual TimerID set_timer(Timestamp , CompletionCB ) override {throw_error();}
    virtual void cancel(const Order &) override {throw_error();}
    virtual Order replace(const Order &, const Order::Setup &, bool) override {throw_error();}
    virtual void update_instrument(const Instrument &, CompletionCB) override {throw_error();}
    virtual Fills get_fills(const Instrument &, std::size_t ) const override{throw_error();}
    virtual Fills get_fills(const Instrument &, Timestamp )const  override{throw_error();}
    virtual Timestamp now() const override{throw_error();}
    virtual bool clear_timer(TimerID ) override{throw_error();}
    virtual void update_account(const Account &, CompletionCB) override{throw_error();}
    virtual Order bind_order(const Instrument &)override{throw_error();}
    virtual Order place(const Instrument &,
            const Order::Setup &) override{throw_error();}
    virtual void allocate(const Account &, double ) override {throw_error();}
    virtual void set_var(std::string_view , std::string_view ) override{throw_error();};
    virtual void unset_var(std::string_view ) override{throw_error();};
    virtual void subscribe(SubscriptionType , const Instrument &) override {throw_error();}
    virtual void unsubscribe(SubscriptionType , const Instrument &) override {throw_error();}
    constexpr virtual ~NullContext() {}

};


template<typename PtrType>
class ContextT {
public:

    static std::shared_ptr<IContext> null_context_ptr;

    ContextT(PtrType ptr):_ptr(std::move(ptr)) {}

    ///store value under a variable name in a persistent storage
    /**
     * Key-Value database is involved.
     *
     * @param key a string key under which the value will be stored
     * @param value a string value to be stored
     *
     * @note There is no limitation for key and value. You can use binary
     * content for both key and value. There is also no limit of size. However
     * keep in mind, that to store longer keys and values can have impact on
     * performance.
     */
    void store(std::string_view key, std::string_view value) {
        _ptr->set_var(key, value);
    }

    template<BinarySerializable T>
    void store(std::string_view key, const T &value) {
        std::string_view binstr(reinterpret_cast<const char *>(&value), sizeof(T));
        _ptr->set_var(key, binstr);
    }

    ///request update account
    /**
     * @param a account to update
     * @param fn function called when update is complete.
     *
     * @note In most cases, you don't need to update account when fill.
     */
    template<std::invocable<> Fn>
    void update_account(const Account &a, Fn &&fn) {
        _ptr->update_account(a, CompletionCB(std::forward<Fn>(fn)));
    }

    ///request update account - use coroutines
    /**
     * @param a account to update
     * @return awaiter object, which must be co_awaited to complete operation.
     *
     * @exception canceled when operation is canceled because the strategy is
     * shut down
     */
    completion_awaiter update_account(const Account &a) {
        return [&](auto fn){
            _ptr->update_account(a, std::move(fn));
        };
    }

    ///request update instrument
    /**
     * @param i instrument to update. The function refresh instrument features
     *  from the exchange
     * @param fn function which is called when update is complete
     *
     * @note you don't need to update instrument often.
     */
    template<std::invocable<> Fn>
    void update_instrument(const Instrument &i, Fn &&fn) {
        _ptr->update_instrument(i, CompletionCB(std::forward<Fn>(fn)));
    }

    ///request update instrument - use coroutine
    /**
     * @param i instrument to update. The function refresh instrument features
     *  from the exchange
     * @return awaiter object, which must be co_awaited to complete operation.
     *
     * @exception canceled when operation is canceled because the strategy is
     * shut down
     *
     * @note you don't need to update instrument often.
     */
    completion_awaiter update_instrument(const Instrument &i) {
        return [&](auto fn){
            _ptr->update_instrument(i, std::move(fn));
        };
    }
    ///Retrieves current time
    Timestamp now() const {return _ptr->now();}
    ///Sets timer, which triggers event, on strategy interface
    TimerID set_timer(Timestamp at) {return _ptr->set_timer(at);}

    template<typename A, typename B>
    TimerID set_timer(std::chrono::duration<A, B> dur) {
        return _ptr->set_timer(now() + dur);
    }

    template<std::invocable<> Fn>
    TimerID set_timer(Timestamp at, Fn &&fn) {
        return _ptr->set_timer(at, make_runnable(std::forward<Fn>(fn)));
    }

    template<typename A, typename B, std::invocable<> Fn>
    TimerID set_timer(std::chrono::duration<A, B> dur, Fn &&fn) {
        return _ptr->set_timer(now() + dur, make_runnable(std::forward<Fn>(fn)));
    }

    ///sleep until timestamp reached - coroutines
    /**
     * You need to co_await result to sleep
     * @param at timestamp to sleep
     * @return awaiter to be co_await
     * @exception canceled strategy is about shut down
     * @note you cannot cancel this operation (no clear_timer)
     */
    completion_awaiter sleep_until(Timestamp &at) {
        return [&](auto fn) {
            _ptr->set_timer(at, std::move(fn));
        };
    }
    ///sleep for given duration - coroutines
    /**
     * You need to co_await result to sleep
     * @param dur duration
     * @return awaiter to be co_await
     * @exception canceled strategy is about shut down
     * @note you cannot cancel this operation (no clear_timer)
     */
    template<typename A, typename B>
    completion_awaiter sleep_for(std::chrono::duration<A, B> dur) {
        return [&](auto fn) {
            _ptr->set_timer(now()+dur, std::move(fn));
        };
    }

    ///Cancel timer
    bool clear_timer(TimerID id) {return _ptr->clear_timer(id);}
    ///Place an order
    Order place(const Instrument &instrument, const Order::Setup &setup) {return _ptr->place(instrument, setup);}
    ///Creates an order, which is asociated with an instrument, but it is not placed
    /**
     * You can use replace() function to place the order with new setup. This
     * allows to track single order without need to know, whether order is actually
     * placed or not
     *
     * @param instrument associated instrument
     * @return dummy order (can be replaced)
     */
    Order bind_order(const Instrument &instrument) {return _ptr->bind_order(instrument);}
    ///Cancel given order
    void cancel(const Order &order) {_ptr->cancel(order);}

    ///Replace order
    /**
     * @param order order to replace.
     * @param setup new setup of the order
     * @param amend try to amend current order - the exchange just modifies amount,
     * limit or stop price if order is pending (waiting for trigger). So you can
     * amend only order with same side, instrument, etc. If amend is not possible,
     * because above rules, new order is discarded and original order is untouched.
     * If exchange doesn't support amend, the service provider can simulate this
     * feature. In all cases, filled amount is transfered to the new order.
     *
     * @return new order
     *
     * @note if replace partially filled order, filled amount is perserved
     * @note replace can fail, if exchange cannot garanteed to replace order
     * without avoiding double execution. In this case, old order is canceled
     * new order is rejected
     *
     * @note if order is pending, you probably can replace with order of same
     * side and behavior. If order is done, it can be replaced with any
     * order (it only associates with order's original instrument)
     */
    Order replace(const Order &order, const Order::Setup &setup, bool amend) {
        return _ptr->replace(order, setup, amend);
    }

    ///Retrieve last fills
    Fills get_fills(const Instrument &i, std::size_t limit) {
        return _ptr->get_fills(i, limit);
    }

    Fills get_fills(const Instrument &i, Timestamp tp) {
        return _ptr->get_fills(i, tp);
    }

    ///allocate equity for current strategy
    /**
     * If equity is shared between multiple strategies,
     * it helps to visualise, how much equity is unallocated or overallocated
     * @param a account
     * @param equity amount equity;
     *
     * @note not persistent.
     *
     */
    void allocate(const Account &a, double equity) {
         _ptr->allocate(a, equity);
    }

    ///Subscribe market data
    /**
     * @param type type of subscription (ticker, orderbook)
     * @param i instrument instance
     *
     * Events are generated as they are received from market stream (as fastest as possible)
     *
     */
    void subscribe(SubscriptionType type, const Instrument &i) {
        _ptr->subscribe(type, i, std::chrono::seconds(0));
    }

    ///Subscribe market data
    /**
     * @param type type of subscription (ticker, orderbook)
     * @param i instrument instance
     * @param interval interval of generating market events. When this value is specified
     * market events are generated in this specified interval regardless on, whether
     * there were change in the event. Some exchanges cannot sent market events
     * as a stream, so each event is actually full query on market state polled in
     * specified interval. The service provider can adjust this interval to lowest
     * allowed value (for example, exchanges where price is polled, you probably don't
     * get interval under 1 minute)
     *
     * @note if called multiple times on single instrument, it only adjusts the
     * interval.
     */
    template<typename A, typename B>
    void subscribe(SubscriptionType type, const Instrument &i, std::chrono::duration<A,B> interval) {
        _ptr->subscribe(type, i, std::chrono::duration_cast<TimeSpan>(interval));
    }

    ///Unsubscribe market data
    /**
     * @param type type of subscription
     * @param i instrument
     *
     * @note if there is no subscription, it does nothing
     *
     * @note your strategy doesn't need to call unsubscribe in the destructor.
     *
     */
    void unsubscribe(SubscriptionType type, const Instrument &i) {
        _ptr->unsubscribe(type, i);
    }


protected:
    PtrType _ptr;
};

class Context : public ContextT<IContext *> {
public:

    static constexpr NullContext null_context = {};

    using ContextT<IContext *>::ContextT;
    Context():ContextT<IContext *>(const_cast<NullContext *>(&null_context)) {}

};

class Variables {
public:

    struct Value {
        const std::optional<std::string_view> v = {};
        operator std::string_view() const {
            return v.has_value()?*v:std::string_view();}

        template<BinarySerializable T>
        operator T() const {
            return v.has_value() && sizeof(T) == v->size()?
                    *reinterpret_cast<const T *>(v->data()):T();
        }

        std::string_view operator()(std::string_view defval) const {
            return v.has_value()?*v:defval;
        }
        template<BinarySerializable T>
        std::string_view operator()(const T &defval) const {
            return v.has_value() && sizeof(T) == v->size()?
                    *reinterpret_cast<const T *>(v->data()):defval;
        }

        bool operator !() const {
            return !v.has_value();
        }

        Value() = default;
        Value(const std::string &v):v(v) {}
        Value(const std::string_view &v):v(v) {}
    };

    Value operator[](std::string_view key) const {
        auto iter = _variables.find(key);
        if (iter == _variables.end()) return {};
        else return Value(iter->second);
    }

    auto begin() const {
        return _variables.begin();
    }
    auto end() const {
        return _variables.end();
    }

    auto prefix(std::string_view pfx) {
        if (pfx.empty()) return std::pair(begin(), end());
        auto low = _variables.lower_bound(pfx);
        std::string pfx_end = end_of_range(pfx);
        auto high = pfx.empty()?end():_variables.upper_bound(pfx_end);
        return std::pair(low,high);
    }

    const std::map<std::string, std::string, std::less<> > _variables;
protected:
    std::string end_of_range(std::string_view pfx) {
        std::string out;
        while (!pfx.empty()) {
            unsigned char c = pfx.back();
            constexpr unsigned char last_code = 0xFF;
            pfx = pfx.substr(0, pfx.length()-1);
            if (c != last_code) {
                ++c;
                out = pfx;
                out.push_back(c);
                break;
            }
        }
        return out;
    }
};


}

