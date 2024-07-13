#pragma once

#include "strategy.h"

#include <atomic>
#include <map>

namespace trading_api {

class IStrategy;
class IExchangeService;

class IModule {
public:


    template<typename T>
    class Factory {
    public:

        template<typename U>
        Factory(std::string_view name, std::in_place_type_t<U>)
            :create_fn(&uhlp<U>::create),name(name),next(first) {
            first = this;
        }

        std::string_view get_name() const {return name;}
        std::unique_ptr<T> create() const {return create_fn();}

        Factory(const Factory &) = delete;
        Factory &operator=(const Factory &) = delete;

        static Factory *first;

        const Factory *get_next() const {
            return next;
        }

    protected:
        std::unique_ptr<T> (*create_fn)() = nullptr;
        std::string_view name;
        Factory *next = nullptr;


        template<typename U>
        struct uhlp {
            static std::unique_ptr<T> create();
        };


    };

    template<typename T>
    class Iterator {
    public:
        Iterator(const Factory<T> *ptr):_ptr(ptr) {}
        Iterator() = default;
        Iterator(const Iterator& ) = default;
        Iterator &operator=(const Iterator& ) = default;

        using value_type = const Factory<T>;
        using reference = const Factory<T> &;
        using pointer = const Factory<T> *;
        using iterator_category = std::forward_iterator_tag;

        reference operator *() const {return *_ptr;}
        pointer operator->() const {return _ptr;}

        bool operator==(const Iterator &other) const = default;
        Iterator &operator++()  {_ptr = _ptr->get_next();return *this;}
        Iterator operator++(int)  {auto save = *this;_ptr = _ptr->next;return save;}


    protected:
        const Factory<T> *_ptr= nullptr;
    };


    template<typename T>
    class Inventory {
    public:
        Inventory (const Factory<T> *ptr):_ptr(ptr) {};
        Inventory (const Inventory &) = default;
        Inventory &operator=(const Inventory &) = default;

        Iterator<T> begin() const {return _ptr;}
        static constexpr Iterator<T> end() {return {};}
        Iterator<T> find(const std::string_view &name) const {
            auto iter = begin();
            while (iter != end()  && iter->get_name() != name) ++iter;
            return iter;
        }

    protected:
        const Factory<T> *_ptr= nullptr;
    };



    virtual Inventory<IStrategy> get_strategies() const = 0;
    virtual Inventory<IExchangeService> get_exchanges() const = 0;
    virtual bool can_unload() const = 0;
    virtual std::size_t get_active_objects() const = 0;
    virtual ~IModule()=default;

};

using EntryPointFn = const IModule *(*)();

}


