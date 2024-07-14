#pragma once
#include <utility>
#include <vector>


namespace trading_api {

template<typename T, typename Cmp = std::less<T>, typename Alloc = std::allocator<T> >
class PriorityQueue: private std::vector<T> {
public:

    using Super = std::vector<T, Alloc>;


    PriorityQueue() = default;
    PriorityQueue(Cmp cmp):_cmp(std::move(cmp)) {}
    template<std::input_iterator Iter>
    PriorityQueue(Iter begin, Iter end) {
        while (begin != end) {
            push(*begin);
            ++begin;
        }
    }

    void push(const T &arg) {
        emplace(arg);
    }

    void push(T &&arg) {
        emplace(std::move(arg));
    }

    template<typename ... Args>
    void emplace(Args && ... args) {
        static_assert(std::is_constructible_v<T, Args...>);
        auto index= Super::size();
        Super::emplace_back(std::forward<Args>(args)...);
        heapify_up(index);
    }

    void pop() {
        if (Super::empty()) return;
        if (Super::size() > 1) {
            Super::front() = std::move(Super::back());
            Super::pop_back();
            heapify_down(0);
        } else {
            Super::pop_back();
        }
    }

    void priority_altered(typename Super::const_iterator it, bool cmp_result) {
        std::size_t index = std::distance(Super::cbegin(), it);
        if (cmp_result) heapify_down(index);
        else heapify_up(index);
    }

    void replace(typename Super::const_iterator it, const T &val) {
        bool isless= _cmp(val, *it);
        std::size_t index = std::distance(Super::cbegin(), it);
        Super::at(index) = val;
        priority_altered(it, isless);
    }

    void replace(typename Super::const_iterator it, T &&val) {
        bool isless= _cmp(val, *it);
        std::size_t index = std::distance(Super::cbegin(), it);
        Super::at(index) = std::move(val);
        priority_altered(it, isless);
    }

    void erase(typename Super::const_iterator it) {
        std::size_t index = std::distance(Super::cbegin(), it);
        if (index == 0) {
            pop();
        } else if (index == Super::size()-1) {
            Super::pop_back();
        } else {
            std::swap(Super::operator[](index), Super::back());
            Super::pop_back();
            heapify_down(std::distance(Super::cbegin(), it));
        }
    }

    using Super::begin;
    using Super::end;
    using Super::cbegin;
    using Super::cend;
    using Super::rbegin;
    using Super::rend;
    using Super::crbegin;
    using Super::crend;
    using Super::front;
    using Super::back;
    using Super::empty;
    using Super::size;
    using Super::data;

    typename Super::const_iterator to_iterator(const T &item) const {
        auto ret = Super::cbegin();
        std::advance(ret, &item - Super::data());
        return ret;
    }
    typename Super::iterator to_iterator(const T &item) {
        auto ret = Super::begin();
        std::advance(ret, &item - Super::data());
        return ret;
    }

protected:
    Cmp _cmp;


    void heapify_down(std::size_t index) {
        std::size_t n = Super::size();

        while (true) {
            auto left = 2 * index + 1;
            auto right = 2 * index + 2;
            auto largest = index;

            if (left < n && _cmp(Super::operator[](largest), Super::operator[](left))) {
                largest = left;
            }

            if (right < n && _cmp(Super::operator[](largest), Super::operator[](right))) {
                largest = right;
            }

            if (largest != index) {
                std::swap(Super::operator[](index),Super::operator[](largest));
                index = largest;
            } else {
                break;
            }
        }
    }

    void heapify_up(std::size_t index) {

        while (index > 0) {
            auto parent = (index - 1) / 2;

            if (_cmp(Super::operator[](parent) , Super::operator[](index))) {
                std::swap(Super::operator[](index), Super::operator[](parent));
                index = parent;
            } else {
                break;
            }
        }
    }


};




}
