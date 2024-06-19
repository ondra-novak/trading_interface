#include <utility>
#include <vector>


namespace trading_api {

template<typename T, typename Cmp = std::less<T> >
class heap: public std::vector<T> {
public:

    using Super = std::vector<T>;
    using std::vector<T>::vector;

    void set_compare(Cmp cmp) {_cmp = std::move(cmp);}


    void push(const T &arg) {
        emplace(arg);
    }

    void push(T &&arg) {
        emplace(std::move(arg));
    }

    template<typename ... Args>
    void emplace(Args && ... args) {
        static_assert(std::is_constructible_v<T, Args...>);
        Super::emplace_back(std::forward<Args>(args)...);
        std::push_heap(Super::begin(), Super::end(), _cmp);
    }

    void pop() {
        std::pop_heap(Super::begin(), Super::end(), _cmp);
        Super::pop_back();
    }


    void replace(typename Super::const_iterator it, const T &val) {
        bool isless= _cmp(val, *it);
        *it = val;
        if (isless) {
            heapify_down(it);
        } else {
            heapify_up(it);
        }
    }

    void replace(typename Super::const_iterator it, T &&val) {
        bool isless= _cmp(val, *it);
        *it = val;
        if (isless) {
            heapify_down(it);
        } else {
            heapify_up(it);
        }
    }

    void erase(typename Super::const_iterator it) {
        auto index = std::distance(Super::cbegin(), it);
        if (index == 0) {
            pop();
        } else if (index == Super::size()-1) {
            Super::pop_back();
        } else {
            std::swap(Super::operator[](index), Super::back());
            Super::pop_back();
            heapify_down(it);
        }
    }


protected:
    Cmp _cmp;


    void heapify_down(typename Super::const_iterator it) {
        auto n = Super::size();
        auto index = std::distance(Super::cbegin(), it);

        while (true) {
            auto left = 2 * index + 1;
            auto right = 2 * index + 2;
            auto largest = index;

            if (left < n && _cmp(Super::operator[](largest)), Super::operator[](left)) {
                largest = left;
            }

            if (right < n && _cmp(Super::operator[](largest)), Super::operator[](right)) {
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

    void heapify_up(typename Super::const_iterator it) {
        auto index = std::distance(Super::cbegin(), it);

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
