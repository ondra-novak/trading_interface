#pragma once
#include <memory>
#include <string_view>
#include <shared_mutex>

namespace trading_api {


union UnorderedMapStringKey {
    enum class Type : char{
        view,
        allocated,
        embedded,
    };

    struct View {
        Type type =  Type::allocated;
        const char *ptr = nullptr;
        std::size_t size = 0;
    };

    struct Embedded {
        Type type = Type::embedded;
        unsigned char len = {};
        char text[sizeof(View)-2] = {};
    };

    struct ViewTag {};

    static constexpr ViewTag make_view_flag = {};

    View _view;
    Embedded _embedded;

    UnorderedMapStringKey():_view{Type::embedded,0,{}} {}
    explicit UnorderedMapStringKey(const std::string_view &txt) {
        if (txt.size() <= sizeof(_embedded.text)) {
            std::construct_at(&_embedded);
            _embedded.len = static_cast<unsigned char>(txt.size());
            std::copy(txt.begin(), txt.end(), std::begin(_embedded.text));
        } else {
            std::construct_at(&_view);
            char *p = new char[txt.size()];
            _view.ptr = p;
            _view.size = txt.size();
            std::copy(txt.begin(), txt.end(), p);
        }
    }
    UnorderedMapStringKey(const std::string_view &txt, ViewTag)
        :_view{Type::view, txt.data(), txt.size()} {}

    ~UnorderedMapStringKey() {
        if (_view.type == Type::allocated) delete [] (_view.ptr);
    }

    static UnorderedMapStringKey view(const std::string_view &str) {
        return UnorderedMapStringKey(str, make_view_flag);
    }

    std::string_view get() const {
        if (_view.type == Type::embedded) {
            return std::string_view(_embedded.text, _embedded.len);
        } else {
            return std::string_view(_view.ptr, _view.size);
        }
    }

    UnorderedMapStringKey(const UnorderedMapStringKey &other)
        :UnorderedMapStringKey(other.get()) {}

    UnorderedMapStringKey(UnorderedMapStringKey &&other) {
        if (other._view.type == Type::embedded) {
            _embedded = other._embedded;
            other._embedded.len = 0;
        } else {
            _view = other._view;
            if (_view.type == Type::allocated) {
                other._view.type = Type::view;
                other._view.size = 0;
                other._view.ptr = 0;
            }
        }
    }


    UnorderedMapStringKey &operator=(const UnorderedMapStringKey &other) {
        std::destroy_at(this);
        std::construct_at(this, other);
        return *this;
    }
    UnorderedMapStringKey &operator=(UnorderedMapStringKey &&other) {
        std::destroy_at(this);
        std::construct_at(this, std::move(other));
        return *this;
    }

    friend auto operator<=>(const UnorderedMapStringKey &a, const UnorderedMapStringKey &b) {
        return a.get().compare(b.get());
    }
    friend auto operator<=>(const UnorderedMapStringKey &a, const std::string_view &b)  {
        return a.get().compare(b);
    }
    friend auto operator<=>(const std::string_view &a, const UnorderedMapStringKey &b) {
        return a.compare(b.get());
    }
    friend auto operator==(const UnorderedMapStringKey &a, const UnorderedMapStringKey &b)  {
        return a.get().compare(b.get()) == 0;
    }
    friend auto operator==(const UnorderedMapStringKey &a, const std::string_view &b) {
        return a.get().compare(b) == 0;
    }
    friend auto operator==(const std::string_view &a, const UnorderedMapStringKey &b) {
        return a.compare(b.get()) == 0;
    }

    struct Hasher {
        auto operator()(const UnorderedMapStringKey &a) const {
            std::hash<std::string_view> hasher;
            return hasher(a.get());
        }
    };

};


template<typename T>
class WeakObjectMap {
public:

    std::shared_ptr<T> find(const std::string_view &id) const;
    void insert(const std::string_view &id, const std::shared_ptr<T> &item);
    void erase(const std::string_view &id);
    void gc();

    template<typename X, typename ... Args>
    std::shared_ptr<T> create_if_not_exists(const std::string_view &id, Args && ... args);


protected:

    using Map = std::unordered_map<UnorderedMapStringKey, std::weak_ptr<T>, UnorderedMapStringKey::Hasher>;

    Map _map;


};

template<typename T>
inline std::shared_ptr<T> WeakObjectMap<T>::find(const std::string_view &id) const {
    auto iter = _map.find(UnorderedMapStringKey::view(id));
    if (iter == _map.end()) return {};
    return iter->second.lock();
}


template<typename T>
inline void WeakObjectMap<T>::insert(const std::string_view &id, const std::shared_ptr<T> &item) {
    auto iter = _map.find(UnorderedMapStringKey::view(id));
    if (iter != _map.end()) {
        iter->second = item;
    } else {
        _map.insert(std::pair(UnorderedMapStringKey(id), item));
    }
}

template<typename T>
inline void WeakObjectMap<T>::erase(const std::string_view &id) {
    _map.erase(UnorderedMapStringKey::view(id));
}

template<typename T>
template<typename X, typename ... Args>
std::shared_ptr<T> WeakObjectMap<T>::create_if_not_exists(const std::string_view &id, Args && ... args) {
    auto iter = _map.find(UnorderedMapStringKey::view(id));
    if (iter == _map.end()) {
        std::shared_ptr<T> c = std::make_shared<X>(std::forward<Args>(args)...);
        _map.insert(std::pair(UnorderedMapStringKey(id), c));
        return c;
    } else {
        std::shared_ptr<T> c = iter->second.lock();
        if (c == nullptr) {
            c = std::make_shared<X>(std::forward<Args>(args)...);
            iter->second = c;
        }
        return c;
    }
}


template<typename T>
inline void WeakObjectMap<T>::gc() {
    auto iter = _map.begin();
    while (iter != _map.end()) {
        if (iter->second.expired()) iter = _map.erase(iter);
        else ++iter;
    }
}

template<typename T>
class WeakObjectMapWithLock: public WeakObjectMap<T> {
public:
    using WeakObjectMap<T>::WeakObjectMap;
    using Super = WeakObjectMap<T>;

    std::shared_ptr<T> find(const std::string_view &id) const {
        std::shared_lock _(_mx);
        return Super::find(id);
    }
    void insert(const std::string_view &id, const std::shared_ptr<T> &item) {
        std::lock_guard _(_mx);
        return Super::insert(id, item);
    }
    void erase(const std::string_view &id) {
        std::lock_guard _(_mx);
        Super::erase(id);
    }
    template<typename X, typename ... Args>
    std::shared_ptr<T> create_if_not_exists(const std::string_view &id, Args && ... args) {
        std::lock_guard _(_mx);
        return Super::template create_if_not_exists<X>(id, std::forward<Args>(args)...);
    }
    void gc() {
        std::lock_guard _(_mx);
        Super::gc();
    }
protected:
    mutable std::shared_mutex _mx;
};

}
