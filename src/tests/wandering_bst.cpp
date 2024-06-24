#include "../trading_ifc/wandering_bst.h"
#include "check.h"


struct Key {
    int k;
};

struct LessKey {
    bool operator()(const Key &a, const Key &b) const {
        return a.k < b.k;
    }
};

int main() {


    trading_api::WanderingTree<Key, int, LessKey> tree;
    for (int i = 0; i < 50; ++i) {
        tree.insert(Key{i},i);
    }
    int v = 0;
    for (const auto &kv: tree) {
        CHECK_EQUAL(kv.first.k, v);
        CHECK_EQUAL(kv.second, v);
        ++v;
    }

    for (const auto &kv: tree) {
        tree.replace(kv.first, kv.second+10);
    }

    v = 0;
    for (const auto &kv: tree) {
        CHECK_EQUAL(kv.first.k, v);
        CHECK_EQUAL(kv.second, v+10);
        ++v;
    }

    for (const auto &kv: tree) {
        if (kv.first.k & 1) tree.erase(kv.first);
    }

    v = 0;
    for (const auto &kv: tree) {
        CHECK_EQUAL(kv.first.k, v);
        v+=2;
    }


}
