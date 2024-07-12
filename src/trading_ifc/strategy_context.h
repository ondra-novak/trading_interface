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
concept BinarySerializable = (
            (std::is_trivially_copy_constructible_v<T> && std::is_standard_layout_v<T>)
            || (std::is_constructible_v<T, std::string_view> && std::is_convertible_v<T, std::string_view>));


template<BinarySerializable T>
std::string_view serialize_binary(const T &data) {
    if constexpr(std::is_constructible_v<T, std::string_view> && std::is_convertible_v<T, std::string_view>) {
        return std::string_view(data);
    } else {
        return std::string_view(reinterpret_cast<const char *>(&data), sizeof(data));
    }
}

template<BinarySerializable T>
std::optional<T> deserialize_binary(std::string_view binary) {
    if constexpr(std::is_constructible_v<T, std::string_view> && std::is_convertible_v<T, std::string_view>) {
        return T(binary);
    } else {
        std::optional<T> ret;
        if (binary.size() == sizeof(T)) {
            ret.emplace(*reinterpret_cast<const T *>(binary.data()));
        }
        return ret;
    }
}

template<BinarySerializable T>
T deserialize_binary(std::string_view binary, const T &defval) {
    if constexpr(std::is_constructible_v<T, std::string_view> && std::is_convertible_v<T, std::string_view>) {
        return T(binary);
    } else if (sizeof(T) != binary.size()) {
        return defval;
    } else {
        return T(*reinterpret_cast<const T *>(binary.data()));
    }
}





class IContext {
public:

    virtual ~IContext() = default;

    ///request update account
    virtual void update_account(const Account &a, CompletionCB complete_ptr) = 0;

    ///request update instrument
    virtual void update_instrument(const Instrument &i, CompletionCB complete_ptr) = 0;

    virtual std::vector<Account> get_accounts() const = 0;

    virtual std::vector<Instrument> get_instruments() const = 0;

    virtual StrategyConfig get_config() const = 0;

    ///Retrieves current time
    virtual Timestamp get_event_time() const = 0;

    ///Sets time, which calls function wrapped into runnable object. It is still bound to strategy
    virtual void set_timer(Timestamp at, CompletionCB fnptr = {}, TimerID id = 0) = 0;

    ///Cancel timer
    virtual bool clear_timer(TimerID id) = 0;

    ///Place an order
    virtual Order place(const Instrument &instrument, const Account &account, const Order::Setup &setup) = 0;

    ///Creates and bind an order to an instrument
    /**
     * You can use replace() function to place the order with new setup. This
     * allows to track single order without need to know, whether order is actually
     * placed or not
     *
     * @param instrument associated instrument
     * @return dummy order (can be replaced)
     */
    virtual Order bind_order(const Instrument &instrument, const Account &account) = 0;

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

    virtual std::string get_var(std::string_view var_name) = 0;

    virtual void enum_vars(std::string_view prefix, const Function<void(std::string_view, std::string_view)> &fn) = 0;

    virtual void enum_vars(std::string_view start, std::string_view end,  const Function<void(std::string_view, std::string_view)> &fn) = 0;


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
    virtual std::vector<Account> get_accounts() const {throw_error();};
    virtual std::vector<Instrument> get_instruments() const {throw_error();};
    virtual StrategyConfig get_config() const {throw_error();};
    virtual void set_timer(Timestamp , CompletionCB, TimerID ) override {throw_error();}
    virtual void cancel(const Order &) override {throw_error();}
    virtual Order replace(const Order &, const Order::Setup &, bool) override {throw_error();}
    virtual void update_instrument(const Instrument &, CompletionCB) override {throw_error();}
    virtual Fills get_fills(const Instrument &, std::size_t ) const override{throw_error();}
    virtual Fills get_fills(const Instrument &, Timestamp )const  override{throw_error();}
    virtual Timestamp get_event_time() const override{throw_error();}
    virtual bool clear_timer(TimerID ) override{throw_error();}
    virtual void update_account(const Account &, CompletionCB) override{throw_error();}
    virtual Order bind_order(const Instrument &,  const Account &)override{throw_error();}
    virtual Order place(const Instrument &, const Account &, const Order::Setup &) override{throw_error();}
    virtual void allocate(const Account &, double ) override {throw_error();}
    virtual void set_var(std::string_view , std::string_view ) override{throw_error();};
    virtual void unset_var(std::string_view ) override{throw_error();};
    virtual void subscribe(SubscriptionType , const Instrument &) override {throw_error();}
    virtual void unsubscribe(SubscriptionType , const Instrument &) override {throw_error();}
    virtual std::string get_var(std::string_view ) override {throw_error();}
    virtual void enum_vars(std::string_view , const Function<void(std::string_view, std::string_view)> &)  override {throw_error();}
    virtual void enum_vars(std::string_view , std::string_view ,  const Function<void(std::string_view, std::string_view)> &)  override {throw_error();}
    constexpr virtual ~NullContext() {}

};


template<typename PtrType>
class ContextT {
public:

    static std::shared_ptr<IContext> null_context_ptr;

    ContextT(PtrType ptr):_ptr(std::move(ptr)) {}


    std::vector<Account> get_accounts() const {return _ptr->get_accounts();}

    std::vector<Instrument> get_instruments() const {return _ptr->get_instruments();}

    virtual StrategyConfig get_config() const {return _ptr->get_config();}



    ///store value under a variable name in a persistent storage
    /**
     * Key-Value database is expected.
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

    ///store value under a variable name in a persistent storage
    /**
     * Key-Value database is expected.
     *
     * @param key a string key under which the value will be stored
     * @param value any binary serializable value, which includes all basic types, enums, and classes
     * with trivial copy constructors (must not contain pointer)
     *
     */
    template<BinarySerializable T>
    void store(std::string_view key, const T &value) {
        _ptr->set_var(key,serialize_binary(value));
    }

    std::string retrieve(std::string_view varname) {
        return _ptr->retrieve(varname);
    }
    template<BinarySerializable T>
    T retrieve(std::string_view key, const T &default_value) {
        return deserialize_binary(_ptr->retrieve(key), default_value);
    }

    template<std::invocable<std::string_view, std::string_view> Fn>
    void retrieve(std::string_view prefix, Fn &&fn) {
        return _ptr->enum_vars(prefix, std::forward<Fn>(fn));
    }

    template<BinarySerializable T, std::invocable<std::string_view, T> Fn>
    void retrieve(std::string_view prefix, Fn &&fn) {
        return _ptr->enum_vars(prefix, [&](std::string_view a, std::string_view b){
            auto opt = deserialize_binary<T>(b);
            if (opt.has_value()) fn(a,*opt);
        });
    }
    template<std::invocable<std::string_view, std::string_view> Fn>
    void retrieve(std::string_view from, std::string_view to, Fn &&fn) {
        return _ptr->enum_vars(from, to, std::forward<Fn>(fn));
    }

    template<BinarySerializable T, std::invocable<std::string_view, T> Fn>
    void retrieve(std::string_view from, std::string_view to, Fn &&fn) {
        return _ptr->enum_vars(from, to, [&](std::string_view a, std::string_view b){
            auto opt = deserialize_binary<T>(b);
            if (opt.has_value()) fn(a,*opt);
        });
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
    ///Retrieves time of current event
    /**
     * @return time of current event. This contains snapshot of real time clock,
     * when current event was started. It returns same value for current
     * event regardless on how long the event is executed.
     *
     * During backtest, this value doesn't contain real time value, but
     * backtest's time value. When strategy needs source of the time for calculation
     * it always should use event time.
     *
     * @note if coroutine awaits for asynchronous operation, calling
     * of this function after co_await can return different time then
     * value returned before co_await.
     */
    Timestamp get_event_time() const {return _ptr->get_event_time();}
    ///Sets timer, which triggers event, on strategy interface
    /**
     * @param at time point when timer is triggered
     * @param id optional identifier. It can be 0, but you will unable to detect, which timer triggered.
     * The ID also allows you to cancel timer. ID don't need to be unique
     *
     */
    void set_timer(Timestamp at, TimerID id = 0) {return _ptr->set_timer(at, {}, id);}

    ///Sets timer, which triggers event, on strategy interface
    /**
     * @param dur duration
     * @param id optional identifier. It can be 0, but you will unable to detect, which timer triggered.
     * The ID also allows you to cancel timer. ID don't need to be unique
     */
    template<typename A, typename B>
    void set_timer(std::chrono::duration<A, B> dur, TimerID id = 0) {
        return _ptr->set_timer(get_event_time() + dur, {}, id);
    }

    ///Schedule a function call to given time
    /**
     * @param at time point
     * @param fn function
     * @param id identifier
     *
     * @note this timer doesn't trigger on_timer
     */
    template<std::invocable<> Fn>
    void set_timer(Timestamp at, Fn &&fn, TimerID id = 0) {
        return _ptr->set_timer(at, make_runnable(std::forward<Fn>(fn)), id);
    }

    ///Schedule a function call to given time
    /**
     * @param dur duration
     * @param fn function
     * @param id identifier
     *
     * @note this timer doesn't trigger on_timer
     */
    template<typename A, typename B, std::invocable<> Fn>
    void set_timer(std::chrono::duration<A, B> dur, Fn &&fn, TimerID id = 0) {
        return _ptr->set_timer(get_event_time() + dur, make_runnable(std::forward<Fn>(fn)), id);
    }

    ///sleep until timestamp reached - coroutines
    /**
     * You need to co_await result to sleep
     * @param at time point
     * @param id optional id
     */
    completion_awaiter sleep_until(Timestamp &at, TimerID id = 0) {
        return [&](auto fn) {
            _ptr->set_timer(at, std::move(fn), id);
        };
    }
    ///sleep until timestamp reached - coroutines
    /**
     * You need to co_await result to sleep
     * @param dur duration
     * @param id optional id
     */
    template<typename A, typename B>
    completion_awaiter sleep_for(std::chrono::duration<A, B> dur, TimerID id = 0) {
        return [&](auto fn) {
            _ptr->set_timer(get_event_time()+dur, std::move(fn), id);
        };
    }

    ///Cancel timer
    bool clear_timer(TimerID id) {return _ptr->clear_timer(id);}
    ///Place an order
    Order place(const Instrument &instrument, const Account &account, const Order::Setup &setup) {
        return _ptr->place(instrument, account, setup);
    }
    ///Creates an order, which is asociated with an instrument, but it is not placed
    /**
     * You can use replace() function to place the order with new setup. This
     * allows to track single order without need to know, whether order is actually
     * placed or not
     *
     * @param instrument associated instrument
     * @return dummy order (can be replaced)
     */
    Order bind_order(const Instrument &instrument, const Account &account) {return _ptr->bind_order(instrument, account);}
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

}

