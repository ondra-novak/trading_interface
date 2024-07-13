#pragma once

#ifndef TRADING_API_MODULE_ENTRY_POINT
#define TRADING_API_MODULE_ENTRY_POINT



namespace trading_api {

template<typename T>
inline IModule::Factory<T> *IModule::Factory<T>::first = nullptr;


namespace {

inline std::atomic<std::size_t> object_counter;


template<typename T>
class ObjectCounter: public T {
public:
    ObjectCounter() {
        ++object_counter;
    }
    ~ObjectCounter() {
        --object_counter;
    }
};



class Module: public IModule {
public:
    virtual Inventory<IStrategy> get_strategies() const override {
        return Inventory<IStrategy>(IModule::Factory<IStrategy>::first);
    }
    virtual Inventory<IExchangeService> get_exchanges() const override {
        return Inventory<IExchangeService>(IModule::Factory<IExchangeService>::first);
    }
    virtual bool can_unload() const override {
        return object_counter.load() == 0;
    }
    virtual std::size_t get_active_objects() const override {
        return object_counter.load();
    }
};

}

template<typename T>
template<typename U>
std::unique_ptr<T> IModule::Factory<T>::uhlp<U>::create() {
    return std::unique_ptr<T>(new ObjectCounter<U>());
}





}



extern "C" {
const trading_api::IModule * __trading_api_module_entry_point() {


    static trading_api::Module module;
    return &module;
}
}


#endif
