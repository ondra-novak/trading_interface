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
    virtual Fills get_fills(std::size_t limit) = 0;

    ///Retrieve recent fills
    virtual Fills get_fills(Timestamp tp) = 0;

    ///set persistent variable
    virtual void set_var(int idx, const Value &val) = 0;

    ///get persistent variable
    virtual Value get_var(int idx) const = 0;

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
    virtual Fills get_fills(std::size_t ) override{throw_error();}
    virtual Fills get_fills(Timestamp ) override{throw_error();}
    virtual Timestamp now() const override{throw_error();}
    virtual bool clear_timer(TimerID ) override{throw_error();}
    virtual void update_account(const Account &, CompletionCB) override{throw_error();}
    virtual Order bind_order(const Instrument &)override{throw_error();}
    virtual Order place(const Instrument &,
            const Order::Setup &) override{throw_error();}
    virtual void allocate(const Account &, double ) override {throw_error();}
    virtual Value get_var(int ) const override  {throw_error();}
    virtual void set_var(int  , const Value &) override {throw_error();}
    virtual void subscribe(SubscriptionType , const Instrument &) override {throw_error();}
    virtual void unsubscribe(SubscriptionType , const Instrument &) override {throw_error();}
    constexpr virtual ~NullContext() {}

};


template<typename PtrType>
class ContextT {
public:

    static std::shared_ptr<IContext> null_context_ptr;

    ContextT(PtrType ptr):_ptr(std::move(ptr)) {}

    template<typename Ctx>
    class Variable {
    public:

        template<std::convertible_to<Value> T>
        requires (!std::is_const_v<Ctx>)
        const T &operator=(const T &x) {
            ctx->set_var(var, Value(x));
        }

        template<std::same_as<std::nullptr_t> T>
        requires (!std::is_const_v<Ctx>)
        T operator=(T) {
            ctx->set_var(var, Value());
        }


        template<typename T>
        operator T() const {
            return std::visit([](const auto &x)->T{
                if constexpr (std::is_constructible_v<T, decltype(x)>) {
                    return T(x);
                } else {
                    return T();
                }
            }, ctx->get_var(var));
        }
        template<typename T>
        T operator()(const T &def_val) const {
            return std::visit([&](const auto &x)->T{
                if constexpr (std::is_constructible_v<T, decltype(x)>) {
                    return T(x);
                } else {
                    return def_val;
                }
            }, ctx->get_var(var));
        }

        ///retrieve array value (with default value which is empty array)
        template<typename T>
        T operator()() const {
            return operator()(std::span<Value>());
        }


    protected:
        Variable (Ctx *ctx, int var)
            :ctx(ctx),var(var) {}
        Ctx *ctx;
        int var;
        friend class Context;
    };

    ///access to permanent storage
    template<typename T>
    requires std::same_as<std::underlying_type_t<T>, int>
    auto operator[](T name) {
        return Variable<IContext>(_ptr.get(), static_cast<int>(name));
    }
    ///access to permanent storage
    template<typename T>
    requires std::same_as<std::underlying_type_t<T>, int>
    auto operator[](T name) const {
        return Variable<const IContext>(_ptr.get(), static_cast<int>(name));
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
    Fills get_fills(std::size_t limit) {
        return _ptr->get_fills(limit);
    }

    Fills get_fills(Timestamp tp) {
        return _ptr->get_fills(tp);
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

