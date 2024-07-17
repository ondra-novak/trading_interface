#pragma once
#include <mutex>
#include <memory>

namespace trading_api {

template<typename Key, typename Value, typename Cmp = std::less<Key>, typename Allocator = std::allocator<std::pair<Key,Value> > >
class WanderingTree {
public:
    struct Node;
    using PNode = std::shared_ptr<const Node>;
    using value_type = std::pair<Key, Value>;

    struct Node : value_type{
        PNode _left;
        PNode _right;
        mutable int _height = 1;

        template<std::convertible_to<Key> _Key, std::convertible_to<Value> _Value>
        Node(_Key &&key, _Value &&val, int height = 1, PNode left = {}, PNode right = {})
            :value_type(std::forward<_Key>(key),std::forward<_Value>(val))
            ,_left(std::move(left))
            ,_right(std::move(right))
            ,_height(height) {}
        const Key &key() const {return this->first;}
        const Value &value() const {return this->second;}

    };

    class Iterator {


        static constexpr int max_depth = 64;    //18446744073709551616 items
        PNode _root = {};
        const Node *_levels[max_depth] = {};
        unsigned int _level_count = 0;
    public:

        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = std::pair<Key, Value>;
        using reference = std::add_lvalue_reference_t<std::add_const_t<value_type> >;
        using pointer = std::add_pointer_t<std::add_const_t<value_type> >;

        Iterator() = default;
        Iterator(const PNode &root):_root(root) {
            auto nd = root.get();
            while (nd) {
                _levels[_level_count++] = nd;
                nd = nd->_left.get();
            }
        }
        Iterator(const PNode &root, const PNode &):_root(root) {}

        Iterator(const PNode &root, const Key &key, const Cmp &isless, bool upper_bound)
           :_root(root) {
            auto nd = root.get();
            while (nd) {
                _levels[_level_count++] = nd;
                if (isless(key, nd->key())) {
                    nd = nd->_left.get();
                } else if (isless(nd->key(), key)) {
                    nd = nd->_right.get();
                } else {
                    if (upper_bound) {
                        go_left();
                    }
                    return;
                }
            }
            if (_level_count) {
                bool cres = isless(_levels[_level_count-1]->key(), key);
                if (cres && !upper_bound) {
                    go_right();
                }
            }
        }

        void go_right() {
            if (_level_count) {
                auto nd = _levels[_level_count-1]->_right.get();
                if (nd) {
                    while (nd) {
                        _levels[_level_count++] = nd;
                        nd = nd->_left.get();
                    }
                    return;
                }
                nd = _levels[_level_count-1];
                --_level_count;
                while (_level_count) {
                    auto p = _levels[_level_count-1];
                    if (p->_left.get() == nd) {
                        return;
                    }
                    nd = p;
                    --_level_count;
                }
            }
        }

        void go_left() {
            if (_level_count) {
                auto nd = _levels[_level_count-1]->_left.get();
                if (nd) {
                    while (nd) {
                        _levels[_level_count++] = nd;
                        nd = nd->_right.get();
                    }
                    return;
                }
                nd = _levels[_level_count-1];
                --_level_count;
                while (_level_count) {
                    auto p = _levels[_level_count-1];
                    if (p->_right.get() == nd) {
                        return;
                    }
                    nd = p;
                    --_level_count;
                }
            } else {
                auto nd = _root.get();
                while (nd != nullptr) {
                    _levels[_level_count] = nd;
                    nd = nd->_right.get();
                    ++_level_count;
                }
            }


        }

        bool operator==(const Iterator &iter) const {
            if (iter._level_count != _level_count) return false;
            for (unsigned int i = 0; i < _level_count; ++i) {
                if (iter._levels[i] != _levels[i]) return false;
            }
            return true;
        }

        reference operator *() const {return *_levels[_level_count-1];}
        pointer operator ->() const {return _levels[_level_count-1];}
        Iterator &operator++() {
            go_right();
            return *this;
        }
        Iterator &operator--() {
            go_left();
            return *this;
        }
        Iterator operator++(int) {
            Iterator ret = *this;
            go_right();
            return ret;
        }
        Iterator operator--(int) {
            Iterator ret = *this;
            go_left();
            return ret;
        }

        bool is_end() const {return _level_count == 0;}
        void set_end() {_level_count = 0;}

    };

    WanderingTree() = default;


    template<std::convertible_to<Key> _Key, std::convertible_to<Value> _Value>
    bool insert(_Key &&key, _Value &&value) {
        const Key *finkey = &key;
        PNode ret = insert_internal<false>(_root, std::forward<_Key>(key), std::forward<_Value>(value), finkey);
        if (ret == _root) return false;
        _root = ret;
        return true;
    }

    template<std::convertible_to<Key> _Key, std::convertible_to<Value> _Value>
    bool replace(_Key &&key, _Value &&value) {
        const Key *finkey = &key;
        PNode ret = insert_internal<true>(_root, std::forward<_Key>(key), std::forward<_Value>(value), finkey);
        if (ret == _root) return false;
        _root = ret;
        return true;
    }

    bool erase(const Key &key) {
        PNode ret = delete_internal(_root, key);
        if (ret == _root) return false;
        _root = ret;
        return true;
    }

    Iterator find(const Key &key) const {
        Iterator iter(_root, key,_less,false);
        if (!iter.is_end() && _less(key, iter->first)) iter.set_end();
        return iter;
    }

    Iterator lower_bound(const Key &key) const {
        return {_root, key, _less,false};
    }
    Iterator upper_bound(const Key &key) const {
        return {_root, key, _less, true};
    }
    Iterator begin() const {
        return Iterator{_root};
    }
    Iterator end() const {
        return Iterator{_root, _root};
    }

    bool empty() const {
        return _root == nullptr;
    }

protected:

    PNode _root = {};
    Allocator _alloc = {};
    Cmp _less = {};

    template<std::convertible_to<Key> _Key, std::convertible_to<Value> _Value>
    PNode create_node(_Key &&key, _Value &&val, int height = 1, PNode left = {}, PNode right = {}) {
        return std::allocate_shared<Node>(_alloc,
                std::forward<_Key>(key),
                std::forward<_Value>(val),
                height,
                std::move(left),
                std::move(right));
    }

    PNode duplicate_node(const PNode &src, const PNode &new_left, const PNode &new_right) {
        if (src->_left != new_left || src->_right != new_right) {
            return create_node(src->key(), src->value(), src->_height, new_left, new_right);
        } else {
            return src;
        }
    }

    PNode duplicate_node(const PNode &src, int new_height, const PNode &new_left, const PNode &new_right) {
        if (src->_left != new_left || src->_right != new_right || src->_height != new_height) {
            return create_node(src->key(), src->value(), new_height, new_left, new_right);
        } else {
            return src;
        }
    }

    static int height(PNode node) {
        return node ? node->_height : 0;
    }

    static int balanceFactor(PNode node) {
        return node ? height(node->_left) - height(node->_right) : 0;
    }

    static PNode updateHeight(PNode node) {
        if (node) {
            node->_height = 1 + std::max(height(node->_left), height(node->_right));
        }
        return node;
    }

    PNode rotateRight(PNode n) {
        auto nl = n->_left;
        auto nlr = nl->_right;

        auto new_y = duplicate_node(n, nlr, n->_right);
        auto new_x = duplicate_node(nl, nl->_left, new_y);

        updateHeight(new_y);
        updateHeight(new_x);

        return new_x;
    }

    PNode rotateLeft(PNode n) {
        auto nr = n->_right;
        auto nrl = nr->_left;

        auto new_x = duplicate_node(n, n->_left, nrl);
        auto new_y = duplicate_node(nr, new_x, nr->_right);

        updateHeight(new_x);
        updateHeight(new_y);

        return new_y;
    }

    template<bool replace, std::convertible_to<Key> _Key, std::convertible_to<Value> _Value>
    PNode insert_internal(PNode node, _Key &&key, _Value &&value, const Key *& finkey) {
        if (!node) {
            node = create_node(std::forward<_Key>(key),std::forward<_Value>(value));
            finkey = &node->key();
            return node;
        }

        if (_less(key, node->key())) {
            node = duplicate_node(node,
                    insert_internal<replace>(node->_left, std::forward<_Key>(key), std::forward<_Value>(value), finkey),
                    node->_right);
        } else if (_less(node->key(), key)) {
            node = duplicate_node(node,
                    node->_left,
                    insert_internal<replace>(node->_right, std::forward<_Key>(key), std::forward<_Value>(value), finkey));
        } else {
            if constexpr(replace) {
                node = create_node(std::forward<_Key>(key), std::forward<_Value>(value), node->_height, node->_left, node->_right);
            }
            finkey = &node->key();
            return node; // Duplicate keys are not allowed
        }

        updateHeight(node);

        int balance = balanceFactor(node);

        // Left Left Case
        if (balance > 1 && _less(*finkey ,node->_left->key())) {
            return rotateRight(node);
        }

        // Right Right Case
        if (balance < -1 && _less(node->_right->key(), *finkey)) {
            return rotateLeft(node);
        }

        // Left Right Case
        if (balance > 1 && _less(node->_left->key(), *finkey)) {
            node = duplicate_node(node, rotateLeft(node->_left), node->_right);
            return rotateRight(node);
        }

        // Right Left Case
        if (balance < -1 && _less(*finkey,node->_right->key())) {
            node = duplicate_node(node, node->_left, rotateRight(node->_right));
            return rotateLeft(node);
        }

        return node;
    }


    static PNode minValueNode(PNode node) {
        auto current = node;
        while (current->_left) {
            current = current->_left;
        }
        return current;
    }


    PNode delete_internal(PNode root, const Key &key) {
        if (!root) return root;
        if (_less(key,  root->key())) {
            root = duplicate_node(root, delete_internal(root->_left, key), root->_right);
        } else if (_less(root->key(),key)) {
            root = duplicate_node(root, root->_left, delete_internal(root->_right, key));
        } else {
            if (!root->_left || !root->_right) {
                auto temp = root->_left ? root->_left : root->_right;
                if (!temp) {
                    temp = root;
                    root = nullptr;
                } else {
                    root = temp;
                }
            } else {
                auto temp = minValueNode(root->_right);
                root = duplicate_node(temp, root->_height, root->_left, delete_internal(root->_right, temp->key()));
            }
        }

        if (!root) {
            return root;
        }

        updateHeight(root);

        int balance = balanceFactor(root);

        // Left Left Case
        if (balance > 1 && balanceFactor(root->_left) >= 0) {
            return rotateRight(root);
        }

        // Left Right Case
        if (balance > 1 && balanceFactor(root->_left) < 0) {
            root = duplicate_node(root, rotateLeft(root->_left), root->_right);
            return rotateRight(root);
        }

        // Right Right Case
        if (balance < -1 && balanceFactor(root->_right) <= 0) {
            return rotateLeft(root);
        }

        // Right Left Case
        if (balance < -1 && balanceFactor(root->_right) > 0) {
            root = duplicate_node(root, root->_left, rotateRight(root->_right));
            return rotateLeft(root);
        }

        return root;
    }



};

}
