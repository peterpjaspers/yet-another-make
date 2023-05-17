#ifndef BTREE_PAGE_FUNCTIONS_IMP_H
#define BTREE_PAGE_FUNCTIONS_IMP_H

#include "PageFunctions.h"

namespace BTree {

    template <class T>
    inline void copy(T *dst, const T *src, const PageSize n) {
        if ((0 < n) && (dst != src)) {
            if ((dst < src) || ((src + n) <= dst)) {
                for (int i = 0; i < n; i++) { T t( src[i] ); dst[i] = t; }
            } else {
                for (int i = (n - 1); 0 <= i; i--) { T t( src[i] ); dst[i] = t; };
            }
        }
    };

    // Scalar to scalar pages have the layout:
    //   header       : page type information
    //   split value  : split value (if any)
    //   keys         : Scalar key array
    //   values       : Scalar value array
    // Array to scalar pages have the layout:
    //   header       : page type information
    //   split value  : split value (if any)
    //   values       : Scalar value array
    //   key sizes    : Cumulative size of variable size array keys
    //   key values   : Variable size array key values, packed
    // Scalar to array pages have the layout:
    //   header       : page type information
    //   split value  : split value (if any)
    //   keys         : Scalar key array
    //   values sizes : Cumulative size of variable size array of values
    //   value data   : Variable size array value data, packed
    // Array to array pages have the layout:
    //   header       : page type information
    //   split value  : split value (if any)
    //   key sizes    : Cumulative size of variable size array of keys
    //   value sizes  : Cumulative size of variable size array of values
    //   key data     : Variable size array key data, packed
    //   value data   : Variable size array value data, packed
    //
    // All PageFunctions member functions are templates on key class (K) and value class (V).
    // Both K and V must be of fixed size; i.e., may be passed as operand to 'sizeof'.
    // This ensures that B-Tree code can allocate a fixed amount of memory for each key or value.
    //
    // When rearranging page content, care must be taken to copy content in correct order.
    // When removing (decreasing content size), shift successive content from low to high memory addresses.
    // When adding (increasing content size), shift successive content from high to low memory addresses.
    
    template <class K, class V>
    void PageFunctions::pageSplit(
        const Page<K, V, false, false>& page,
        Page<K, V, false, false>& dst,
        const V& value
    ) {
        if ((page.header.split == 0) || (&page != &dst)) {
            K* k = page.keys();
            V* v = page.values();
            dst.header.count = page.header.count;
            dst.header.split = 1;
            copy<V>(dst.values(), v, dst.header.count);
            copy<K>(dst.keys(), k, dst.header.count);
        }
        dst.splitValue() = value;
    }
    template <class K, class V>
    void PageFunctions::pageSplit(
        const Page<K, V, true, false>& page,
        Page<K, V, true, false>& dst,
        const V& value
    ) {
        if ((page.header.split == 0) || (&page != &dst)) {
            K* k = page.keys();
            PageSize *ks = page.keyIndeces();
            PageIndex kn = page.keyIndex(page.header.count);
            V* v = page.values();
            dst.header.count = page.header.count;
            dst.header.split = 1;
            copy<K>(dst.keys(), k, kn);
            copy<PageSize>(dst.keyIndeces(), ks, dst.header.count);
            copy<V>(dst.values(), v, dst.header.count);
        }
        dst.splitValue() = value;
    }
    template <class K, class V>
    void PageFunctions::pageSplit(
        const Page<K, V, false, true>& page,
        Page<K, V, false, true>& dst,
        const V* value, PageSize valueSize
    ) {
        if ((page.header.split != valueSize) || (&page != &dst)) {
            K* k = page.keys();
            V* v = page.values();
            PageSize *vs = page.valueIndeces();
            PageIndex vn = page.valueIndex(page.header.count);
            PageSize size = page.header.split;
            dst.header.count = page.header.count;
            dst.header.split = valueSize;
            if (valueSize < size) {
                copy<K>(dst.keys(), k, dst.header.count);
                copy<PageSize>(dst.valueIndeces(), vs, dst.header.count);
                copy<V>(dst.values(), v, vn);
            } else {
                copy<V>(dst.values(), v, vn);
                copy<PageSize>(dst.valueIndeces(), vs, dst.header.count);
                copy<K>(dst.keys(), k, dst.header.count);
            }
        }
        copy<V>(dst.splitValue(), value, valueSize);            
    }
    template <class K, class V>
    void PageFunctions::pageSplit(
        const Page<K, V, true, true>& page,
        Page<K, V, true, true>& dst,
        const V* value, PageSize valueSize
    ) {
        if ((page.header.split != valueSize) || (&page != &dst)) {
            K* k = page.keys();
            PageSize *ks = page.keyIndeces();
            PageIndex kn = page.keyIndex(page.header.count);
            V* v = page.values();
            PageSize *vs = page.valueIndeces();
            PageIndex vn = page.valueIndex(page.header.count);
            PageSize size = page.header.split;
            dst.header.count = page.header.count;
            dst.header.split = valueSize;
            if (valueSize < size) {
                copy<V>(dst.splitValue(), value, valueSize);
                copy<PageSize>(dst.keyIndeces(), ks, dst.header.count);
                copy<PageSize>(dst.valueIndeces(), vs, dst.header.count);
                copy<K>(dst.keys(), k, kn);
                copy<V>(dst.values(), v, vn);
            } else {
                // New data array address value only valid after keyIndeces update
                uint8_t* adrv = (reinterpret_cast<uint8_t *>(v) + (valueSize * sizeof(V)) - (size * sizeof(V)));
                V* nv = reinterpret_cast<V* >(reinterpret_cast<uint8_t *>(&dst) + (adrv - reinterpret_cast<uint8_t *>(const_cast<Page<K, V, true, true>*>(&page))));
                copy<V>(nv, v, vn);
                copy<K>(dst.keys(), k, kn);
                copy<PageSize>(dst.valueIndeces(), vs, dst.header.count);
                copy<PageSize>(dst.keyIndeces(), ks, dst.header.count);
                copy<V>(dst.splitValue(), value, valueSize);
            }
        }
        copy<V>(dst.splitValue(), value, valueSize);
    }

    template <class K, class V>
    void PageFunctions::pageRemoveSplit(
        const Page<K, V, false, false>& page,
        Page<K, V, false, false>& dst
    ) {
        if ((page.header.split != 0) || (&page != &dst)) {
            K* k = page.keys();
            V* v = page.values();
            dst.header.count = page.header.count;
            dst.header.split = 0;
            copy<K>(dst.keys(), k, dst.header.count);
            copy<V>(dst.values(), v, dst.header.count);
        }
    }
    template <class K, class V>
    void PageFunctions::pageRemoveSplit(
        const Page<K, V, true, false>& page,
        Page<K, V, true, false>& dst
    ) {
        if ((page.header.split != 0) || (&page != &dst)) {
            K* k = page.keys();
            PageSize *ks = page.keyIndeces();
            PageIndex kn = page.keyIndex(page.header.count);
            V* v = page.values();
            dst.header.count = page.header.count;
            dst.header.split = 0;
            copy<V>(dst.values(), v, dst.header.count);
            copy<PageSize>(dst.keyIndeces(), ks, dst.header.count);
            copy<K>(dst.keys(), k, kn);
        }
    }
    template <class K, class V>
    void PageFunctions::pageRemoveSplit(
        const Page<K, V, false, true>& page,
        Page<K, V, false, true>& dst
    ) {
        if ((page.header.split != 0) || (&page != &dst)) {
            K* k = page.keys();
            V* v = page.values();
            PageSize *vs = page.valueIndeces();
            PageIndex vn = page.valueIndex(page.header.count);
            dst.header.count = page.header.count;
            dst.header.split = 0;
            copy<K>(dst.keys(), k, dst.header.count);
            copy<PageSize>(dst.valueIndeces(), vs, dst.header.count);
            copy<V>(dst.values(), v, vn);
        }
    }
    template <class K, class V>
    void PageFunctions::pageRemoveSplit(
        const Page<K, V, true, true>& page,
        Page<K, V, true, true>& dst
    ) {
        if ((page.header.split != 0) || (&page != &dst)) {
            K* k = page.keys();
            PageSize *ks = page.keyIndeces();
            PageIndex kn = page.keyIndex(page.header.count);
            V* v = page.values();
            PageSize *vs = page.valueIndeces();
            PageIndex vn = page.valueIndex(page.header.count);
            dst.header.count = page.header.count;
            dst.header.split = 0;
            copy<PageSize>(dst.keyIndeces(), ks, dst.header.count);
            copy<PageSize>(dst.valueIndeces(), vs, dst.header.count);
            copy<K>(dst.keys(), k, kn);
            copy<V>(dst.values(), v, vn);
        }
    }
    // ToDo: Optimize page insertion; i.e., only copy when &page != &dst
    template <class K, class V>
    void PageFunctions::pageInsert(
        const Page<K, V, false, false>& page,
        Page<K, V, false, false>& dst,
        PageIndex index,
        const K& key,
        const V& value
    ) {
        K* k = page.keys();
        V* v = page.values();
        V& sv = page.splitValue();
        PageIndex n = (page.header.count - index);
        dst.header.count = (page.header.count + 1);
        dst.header.split = page.header.split;
        copy<V>(&dst.values()[index + 1], &v[index], n);
        copy<V>(dst.values(), v, index);
        copy<K>(&dst.keys()[index + 1], &k[index], n);
        copy<K>(dst.keys(), k, index);
        dst.keys()[index] = key;
        dst.values()[index] = value;
        copy<V>(&dst.splitValue(), &sv, dst.header.split);
    }
    template <class K, class V>
    void PageFunctions::pageInsert(
        const Page<K, V, true, false>& page,
        Page<K, V, true, false>& dst,
        PageIndex index,
        const K* key, const PageSize keySize,
        const V& value
    ) {
        PageSize n = (page.header.count - index);
        K* k = page.keys();
        PageSize *ks = page.keyIndeces();
        PageIndex kn1 = page.keyIndex(index);
        PageIndex kn2 = page.keyIndex(page.header.count);
        V* v = page.values();
        V& sv = page.splitValue();
        dst.header.count = (page.header.count + 1);
        dst.header.split = page.header.split;
        copy<K>(&dst.keys()[kn1 + keySize], &k[kn1], (kn2 - kn1));
        copy<K>(&dst.keys()[kn1], key, keySize);
        copy<K>(dst.keys(), k, kn1);
        copy<PageSize>(&dst.keyIndeces()[index + 1], &ks[index], n);
        for (int o = (index + 1); o < dst.header.count; o++) dst.keyIndeces()[o] += keySize;
        copy<PageSize>(dst.keyIndeces(), ks, index);
        dst.keyIndeces()[index] = (((index == 0) ? 0 : dst.keyIndeces()[index - 1]) + keySize);
        copy<V>(&dst.values()[index + 1], &v[index], n);
        dst.values()[index] = value;
        copy<V>(dst.values(), v, index);
        copy<V>(&dst.splitValue(), &sv, dst.header.split);

    }
    template <class K, class V>
    void PageFunctions::pageInsert(
        const Page<K, V, false, true>& page,
        Page<K, V, false, true>& dst,
        PageIndex index,
        const K& key,
        const V* value, const PageSize valueSize
    ) {
        PageSize n = (page.header.count - index);
        K* k = page.keys();
        V* v = page.values();
        PageSize *vs = page.valueIndeces();
        PageIndex vn1 = page.valueIndex(index);
        PageIndex vn2 = page.valueIndex(page.header.count);
        V* sv = page.splitValue();
        dst.header.count = (page.header.count + 1);
        dst.header.split = page.header.split;
        copy<V>(&dst.values()[vn1 + valueSize], &v[vn1], (vn2 - vn1));
        copy<V>(&dst.values()[vn1], value, valueSize);
        copy<V>(dst.values(), v, vn1);
        copy<PageSize>(&dst.valueIndeces()[index + 1], &vs[index], n);
        for (int o = (index + 1); o < dst.header.count; o++) dst.valueIndeces()[o] += valueSize;
        copy<PageSize>(dst.valueIndeces(), vs, index);
        dst.valueIndeces()[index] = (((index == 0) ? 0 : dst.valueIndeces()[index - 1]) + valueSize);
        copy<K>(&dst.keys()[index + 1], &k[index], n);
        dst.keys()[index] = key;
        copy<K>(dst.keys(), k, index);
        copy<V>(dst.splitValue(), sv, dst.header.split);
    }
    template <class K, class V>
    void PageFunctions::pageInsert(
        const Page<K, V, true, true>& page,
        Page<K, V, true, true>& dst,
        PageIndex index,
        const K* key, const PageSize keySize,
        const V* value, const PageSize valueSize
    ) {
        PageSize n = (page.header.count - index);
        K* k = page.keys();
        PageSize *ks = page.keyIndeces();
        PageIndex kn1 = page.keyIndex(index);
        PageIndex kn2 = page.keyIndex(page.header.count);
        V* v = page.values();
        PageSize *vs = page.valueIndeces();
        PageIndex vn1 = page.valueIndex(index);
        PageIndex vn2 = page.valueIndex(page.header.count);
        V* sv = page.splitValue();
        dst.header.count = (page.header.count + 1);
        dst.header.split = page.header.split;
        // New value data array address only valid after cumulative size update (alignement?)
        uint8_t* adrv = (reinterpret_cast<uint8_t *>(v) + (keySize * sizeof(K)) + (2 * sizeof(PageSize)));
        V* nv = reinterpret_cast<V* >(reinterpret_cast<uint8_t *>(&dst) + (adrv - reinterpret_cast<uint8_t *>(const_cast<Page<K, V, true, true>*>(&page))));
        copy<V>(&nv[vn1 + valueSize], &v[vn1], (vn2 - vn1));
        copy<V>(&nv[vn1], value, valueSize);
        copy<V>(nv, v, vn1);
        copy<K>(&dst.keys()[kn1 + keySize], &k[kn1], (kn2 - kn1));
        copy<K>(&dst.keys()[kn1], key, keySize);
        copy<K>(dst.keys(), k, kn1);
        copy<PageSize>(&dst.valueIndeces()[index + 1], &vs[index], n);
        for (int o = (index + 1); o < dst.header.count; o++) dst.valueIndeces()[o] += valueSize;
        copy<PageSize>(dst.valueIndeces(), vs, index);
        dst.valueIndeces()[index] = (((index == 0) ? 0 : dst.valueIndeces()[index - 1]) + valueSize);
        copy<PageSize>(&dst.keyIndeces()[index + 1], &ks[index], n);
        for (int o = (index + 1); o < dst.header.count; o++) dst.keyIndeces()[o] += keySize;
        copy<PageSize>(dst.keyIndeces(), ks, index);
        dst.keyIndeces()[index] = (((index == 0) ? 0 : dst.keyIndeces()[index - 1]) + keySize);
        copy<V>(dst.splitValue(), sv, dst.header.split);
    }
    template <class K, class V>
    void PageFunctions::pageReplace(
        const Page<K, V, false, false>& page,
        Page<K, V, false, false>& dst,
        PageIndex index,
        const V& value
    ) {
        if (&page != &dst) {
            V& sv = page.splitValue();
            K* k = page.keys();
            V* v = page.values();
            dst.header.count = page.header.count;
            dst.header.split = page.header.split;
            copy<V>(&dst.splitValue(), &sv, dst.header.split);
            copy<K>(dst.keys(), k, dst.header.count);
            copy<V>(dst.values(), v, index);
            copy<V>(&dst.values()[index + 1], &v[index + 1], (dst.header.count - index - 1));
        }
        dst.values()[index] = value;
    }
    template <class K, class V>
    void PageFunctions::pageReplace(
        const Page<K, V, true, false>& page,
        Page<K, V, true, false>& dst,
        PageIndex index,
        const V& value
    ) {
        if (&page != &dst) {
            V& sv = page.splitValue();
            K* k = page.keys();
            PageSize* ks = page.keyIndeces();
            PageSize kn = page.keyIndex(page.header.count);
            V* v = page.values();
            dst.header.count = page.header.count;
            dst.header.split = page.header.split;
            copy<V>(&dst.splitValue(), &sv, dst.header.split);
            copy<V>(dst.values(), v, index);
            copy<V>(&dst.values()[index + 1], &v[index + 1], (dst.header.count - index - 1));
            copy<PageSize>(dst.keyIndeces(), ks, dst.header.count);
            copy<K>(dst.keys(), k, kn);
        }
        dst.values()[index] = value;
    }
    template <class K, class V>
    void PageFunctions::pageReplace(
        const Page<K, V, false, true>& page,
        Page<K, V, false, true>& dst,
        PageIndex index,
        const V* value,
        const PageSize valueSize
    ) {
        V* sv = page.splitValue();
        K* k = page.keys();
        V* v = page.values();
        PageIndex vn1 = page.valueIndex(index);
        PageIndex vn2 = page.valueIndex(page.header.count);
        PageSize size = page.valueSize(index);
        if (&page != &dst) {
            PageSize* vs = page.valueIndeces();
            dst.header.count = page.header.count;
            dst.header.split = page.header.split;
            copy<V>(dst.splitValue(), sv, dst.header.split);
            copy<K>(dst.keys(), k, dst.header.count);
            copy<PageSize>(dst.valueIndeces(), vs, dst.header.count);
            copy<V>(dst.values(), v, vn1);
        }
        copy<V>(&dst.values()[vn1 + valueSize], &v[vn1 + size], (vn2 - (vn1 + size)));
        if (size != valueSize) {
            for (int o = index; o < dst.header.count; o++) {
                dst.valueIndeces()[o] = ((dst.valueIndeces()[o] + valueSize) - size);
            }
        }
        copy<V>(&dst.values()[vn1], value, valueSize);
    }
    template <class K, class V>
    void PageFunctions::pageReplace(
        const Page<K, V, true, true>& page,
        Page<K, V, true, true>& dst,
        PageIndex index,
        const V* value,
        const PageSize valueSize
    ) {
        V* sv = page.splitValue();
        K* k = page.keys();
        PageSize *ks = page.keyIndeces();
        PageIndex kn = page.keyIndex(page.header.count);
        V* v = page.values();
        PageIndex vn1 = page.valueIndex(index);
        PageIndex vn2 = page.valueIndex(page.header.count);
        PageSize size = page.valueSize(index);
        if (&page != &dst) {
            PageSize* vs = page.valueIndeces();
            dst.header.count = page.header.count;
            dst.header.split = page.header.split;
            copy<V>(dst.splitValue(), sv, dst.header.split);
            copy<PageSize>(dst.keyIndeces(), ks, dst.header.count);
            copy<PageSize>(dst.valueIndeces(), vs, dst.header.count);
            copy<K>(dst.keys(), k, kn);
            copy<V>(dst.values(), v, vn1);
        }
        copy<V>(&dst.values()[vn1 + valueSize], &v[vn1 + size], (vn2 - (vn1 + size)));
        if (size != valueSize) {
            for (int o = index; o < dst.header.count; o++) {
                dst.valueIndeces()[o] = ((dst.valueIndeces()[o] + valueSize) - size);
            }
        }
        copy<V>(&dst.values()[vn1], value, valueSize);
    }
    template <class K, class V>
    void PageFunctions::pageReplace(
        const Page<K, V, false, false>& page,
        Page<K, V, false, false>& dst,
        PageIndex index,
        const K& key,
        const V& value
    ) {
        if (&page != &dst) {
            V& sv = page.splitValue();
            K* k = page.keys();
            V* v = page.values();
            dst.header.count = page.header.count;
            dst.header.split = page.header.split;
            copy<V>(&dst.splitValue(), &sv, dst.header.split);
            copy<K>(dst.keys(), k, index);
            copy<K>(&dst.keys()[index + 1], &k[index + 1], (dst.header.count - index - 1));
            copy<V>(dst.values(), v, index);
            copy<V>(&dst.values()[index + 1], &v[index + 1], (dst.header.count - index - 1));
        }
        dst.keys()[index] = key;
        dst.values()[index] = value;
    }
    template <class K, class V>
    void PageFunctions::pageReplace(
        const Page<K, V, true, false>& page,
        Page<K, V, true, false>& dst,
        PageIndex index,
        const K* key,
        PageSize keySize,
        const V& value
    ) {
        V& sv = page.splitValue();
        K* k = page.keys();
        PageSize* ks = page.keyIndeces();
        PageSize kn1 = page.keyIndex(index);
        PageSize kn2 = page.keyIndex(page.header.count);
        PageSize size = page.keySize(index);
        V* v = page.values();
        if (&page != &dst) {
            dst.header.count = page.header.count;
            dst.header.split = page.header.split;
            copy<V>(&dst.splitValue(), &sv, dst.header.split);
            copy<V>(dst.values(), v, index);
            copy<V>(&dst.values()[index + 1], &v[index + 1], (dst.header.count - index - 1));
            copy<PageSize>(dst.keyIndeces(), ks, dst.header.count);
            copy<K>(dst.keys(), k, kn1);
        }
        copy<K>(&dst.keys()[kn1 + keySize], &k[kn1 + size], (kn2 - (kn1 + size)));
        if (size != keySize) {
            for (int o = index; o < dst.header.count; o++) {
                dst.keyIndeces()[o] = ((dst.keyIndeces()[o] + keySize) - size);
            }
        }
        copy<K>(&dst.keys()[kn1], key, keySize);
        dst.values()[index] = value;
    }
    template <class K, class V>
    void PageFunctions::pageReplace(
        const Page<K, V, false, true>& page,
        Page<K, V, false, true>& dst,
        PageIndex index,
        const K& key,
        const V* value,
        const PageSize valueSize
    ) {
        V* sv = page.splitValue();
        K* k = page.keys();
        V* v = page.values();
        PageIndex vn1 = page.valueIndex(index);
        PageIndex vn2 = page.valueIndex(page.header.count);
        PageSize size = page.valueSize(index);
        if (&page != &dst) {
            PageSize* vs = page.valueIndeces();
            dst.header.count = page.header.count;
            dst.header.split = page.header.split;
            copy<V>(dst.splitValue(), sv, dst.header.split);
            copy<K>(dst.keys(), k, index);
            copy<K>(&dst.keys()[index + 1], &k[index + 1], (dst.header.count - index - 1));
            copy<PageSize>(dst.valueIndeces(), vs, dst.header.count);
            copy<V>(dst.values(), v, vn1);
        }
        copy<V>(&dst.values()[vn1 + valueSize], &v[vn1 + size], (vn2 - (vn1 + size)));
        if (size != valueSize) {
            for (int o = index; o < dst.header.count; o++) {
                dst.valueIndeces()[o] = ((dst.valueIndeces()[o] + valueSize) - size);
            }
        }
        dst.keys()[index] = key;
        copy<V>(&dst.values()[vn1], value, valueSize);
    }
    template <class K, class V>
    void PageFunctions::pageReplace(
        const Page<K, V, true, true>& page,
        Page<K, V, true, true>& dst,
        PageIndex index,
        const K* key,
        PageSize keySize,
        const V* value,
        const PageSize valueSize
    ) {
        V* sv = page.splitValue();
        K* k = page.keys();
        PageSize* ks = page.keyIndeces();
        PageSize kn1 = page.keyIndex(index);
        PageSize kn2 = page.keyIndex(page.header.count);
        PageSize kSize = page.keySize(index);
        V* v = page.values();
        PageIndex vn1 = page.valueIndex(index);
        PageIndex vn2 = page.valueIndex(page.header.count);
        PageSize vSize = page.valueSize(index);
        // New value data array address only valid after cumulative size update (alignement?)
        uint8_t* adrv = (reinterpret_cast<uint8_t *>(v) + (keySize * sizeof(K)) + (2 * sizeof(PageSize)));
        V* nv = reinterpret_cast<V* >(reinterpret_cast<uint8_t *>(&dst) + (adrv - reinterpret_cast<uint8_t *>(const_cast<Page<K, V, true, true>*>(&page))));
        if (&page != &dst) {
            PageSize* vs = page.valueIndeces();
            dst.header.count = page.header.count;
            dst.header.split = page.header.split;
            copy<V>(dst.splitValue(), sv, dst.header.split);
            copy<PageSize>(dst.keyIndeces(), ks, dst.header.count);
            copy<PageSize>(dst.valueIndeces(), vs, dst.header.count);
            copy<K>(dst.keys(), k, kn1);
            copy<V>(nv, v, vn1);
        }
        copy<K>(&dst.keys()[kn1 + keySize], &k[kn1 + kSize], (kn2 - (kn1 + kSize)));
        if (kSize != keySize) {
            for (int o = index; o < dst.header.count; o++) {
                dst.keyIndeces()[o] = ((dst.keyIndeces()[o] + keySize) - kSize);
            }
        }
        copy<V>(&dst.keys()[kn1], key, keySize);
        copy<V>(nv[vn1 + valueSize], &v[vn1 + vSize], (vn2 - (vn1 + vSize)));
        if (vSize != valueSize) {
            for (int o = index; o < dst.header.count; o++) {
                dst.valueIndeces()[o] = ((dst.valueIndeces()[o] + valueSize) - vSize);
            }
        }
        copy<V>(&dst.values()[vn1], value, valueSize);
    }
    template <class K, class V>
    void PageFunctions::pageRemove(
        const Page<K, V, false, false>& page,
        Page<K, V, false, false>& dst,
        PageIndex index
    ) {
        K* k = page.keys();
        V* v = page.values();
        dst.header.count = (page.header.count - 1);
        dst.header.split = page.header.split;
        copy<V>(&dst.splitValue(), &page.splitValue(), dst.header.split);
        copy<K>(dst.keys(), k, index);
        copy<K>(&dst.keys()[index], &k[index + 1], (dst.header.count - index));
        copy<V>(dst.values(), v, index);
        copy<V>(&dst.values()[index], &v[index + 1], (dst.header.count - index));
    }
    template <class K, class V>
    void PageFunctions::pageRemove(
        const Page<K, V, true, false>& page,
        Page<K, V, true, false>& dst,
        PageIndex index
    ) {
        K* k = page.keys();
        PageSize keySize = page.keySize(index);
        PageSize *ks = page.keyIndeces();
        PageIndex kn1 = page.keyIndex(index + 1);
        PageIndex kn2 = page.keyIndex(page.header.count);
        V* v = page.values();
        dst.header.count = (page.header.count - 1);
        dst.header.split = page.header.split;
        copy<V>(&dst.splitValue(), &page.splitValue(), dst.header.split);
        copy<V>(dst.values(), v, index);
        copy<V>(&dst.values()[index], &v[index + 1], (dst.header.count - index));
        copy<PageSize>(dst.keyIndeces(), ks, index);
        copy<PageSize>(&dst.keyIndeces()[index], &ks[index + 1], (dst.header.count - index));
        for (int o = index; o < dst.header.count; o++) dst.keyIndeces()[o] -= keySize;
        copy<K>(dst.keys(), k, (kn1 - keySize));
        copy<K>(&dst.keys()[kn1 - keySize], &k[kn1], (kn2 - kn1));
    }
    template <class K, class V>
    void PageFunctions::pageRemove(
        const Page<K, V, false, true>& page,
        Page<K, V, false, true>& dst,
        PageIndex index
    ) {
        K* k = page.keys();
        V* v = page.values();
        PageSize *vs = page.valueIndeces();
        PageSize valueSize = page.valueSize(index);
        PageIndex vn1 = page.valueIndex(index + 1);
        PageIndex vn2 = page.valueIndex(page.header.count);
        dst.header.count = (page.header.count - 1);
        dst.header.split = page.header.split;
        copy<V>(dst.splitValue(), page.splitValue(), dst.header.split);
        copy<K>(dst.keys(), k, index);
        copy<K>(&dst.keys()[index], &k[index + 1], (dst.header.count - index));
        copy<PageSize>(dst.valueIndeces(), vs, index);
        copy<PageSize>(&dst.valueIndeces()[index], &vs[index + 1], (dst.header.count - index));
        for (int o = index; o < dst.header.count; o++) dst.valueIndeces()[o] -= valueSize;
        copy<V>(dst.values(), v, (vn1 - valueSize));
        copy<V>(&dst.values()[vn1 - valueSize], &v[vn1], (vn2 - vn1));
    }
    template <class K, class V>
    void PageFunctions::pageRemove(
        const Page<K, V, true, true>& page,
        Page<K, V, true, true>& dst,
        PageIndex index
    ) {
        K* k = page.keys();
        PageSize keySize = page.keySize(index);
        PageSize *ks = page.keyIndeces();
        PageIndex kn1 = page.keyIndex(index + 1);
        PageIndex kn2 = page.keyIndex(page.header.count);
        V* v = page.values();
        PageSize *vs = page.valueIndeces();
        PageSize valueSize = page.valueSize(index);
        PageIndex vn1 = page.valueIndex(index + 1);
        PageIndex vn2 = page.valueIndex(page.header.count);
        dst.header.count = (page.header.count - 1);
        dst.header.split = page.header.split;
        copy<V>(dst.splitValue(), page.splitValue(), dst.header.split);
        copy<PageSize>(dst.keyIndeces(), ks, index);
        copy<PageSize>(&dst.keyIndeces()[index], &ks[index + 1], (dst.header.count - index));
        for (int o = index; o < dst.header.count; o++) dst.keyIndeces()[o] -= keySize;
        copy<PageSize>(dst.valueIndeces(), vs, index);
        copy<PageSize>(&dst.valueIndeces()[index], &vs[index + 1], (dst.header.count - index));
        for (int o = index; o < dst.header.count; o++) dst.valueIndeces()[o] -= valueSize;
        copy<K>(dst.keys(), k, (kn1 - keySize));
        copy<K>(&dst.keys()[kn1 - keySize], &k[kn1], (kn2 - kn1));
        copy<V>(dst.values(), v, (vn1 - valueSize));
        copy<V>(&dst.values()[vn1 - valueSize], &v[vn1], (vn2 - vn1));
    }
    template <class K, class V>
    void PageFunctions::pageShiftRight(
        const Page<K, V, false, false>& page,
        const Page<K, V, false, false>& right,
        Page<K, V, false, false>& dst,
        Page<K, V, false, false>& dstRight,
        PageIndex index
    ) {
        PageSize shift = (page.header.count - index);
        K* rk = right.keys();
        V* rv = right.values();
        PageSize n = right.header.count;
        dstRight.header.count = (right.header.count + shift);
        dstRight.header.split = right.header.split;
        copy<V>(&dstRight.values()[shift], rv, n);
        copy<K>(&dstRight.keys()[shift], rk, n);
        copy<V>(&dstRight.splitValue(), &right.splitValue(), dstRight.header.split);
        copy<K>(dstRight.keys(), &page.keys()[index], shift);
        copy<V>(dstRight.values(), &page.values()[index], shift);
        V& sv = page.splitValue();
        K* k = page.keys();
        V* v = page.values();
        dst.header.count = (page.header.count - shift);
        dst.header.split = page.header.split;
        copy<V>(&dst.splitValue(), &sv, dst.header.split);
        copy<K>(dst.keys(), k, dst.header.count);
        copy<V>(dst.values(), v, dst.header.count);
    }
    template <class K, class V>
    void PageFunctions::pageShiftRight(
        const Page<K, V, true, false>& page,
        const Page<K, V, true, false>& right,
        Page<K, V, true, false>& dst,
        Page<K, V, true, false>& dstRight,
        PageIndex index
    ) {
        PageSize shift = (page.header.count - index);
        PageSize rn = right.header.count;
        PageIndex kn1 = page.keyIndex(index);
        PageIndex kn2 = page.keyIndex(page.header.count);
        K* rk = right.keys();
        PageSize *rks = right.keyIndeces();
        PageSize rkn = right.keyIndex(right.header.count);
        V* rv = right.values();
        dstRight.header.count = (right.header.count + shift);
        dstRight.header.split = right.header.split;
        // Make room
        copy<K>(&dstRight.keys()[kn2 - kn1], rk, rkn);
        copy<PageSize>(&dstRight.keyIndeces()[shift], rks, rn);
        copy<V>(&dstRight.values()[shift], rv, rn);
        // Shift content
        copy<V>(&dstRight.splitValue(), &right.splitValue(), dstRight.header.split);
        copy<V>(dstRight.values(), &page.values()[index], shift);
        copy<PageSize>(dstRight.keyIndeces(), &page.keyIndeces()[index], shift);
        for (int i = 0; i < shift; i++) dstRight.keyIndeces()[i] -= page.keyIndex(page.header.count - shift);
        for (int i = shift; i < dstRight.header.count; i++) dstRight.keyIndeces()[i] += dstRight.keyIndex(shift);
        copy<K>(dstRight.keys(), &page.keys()[kn1], (kn2 - kn1));
        // Pack
        PageSize *ks = page.keyIndeces();
        V& sv = page.splitValue();
        K* k = page.keys();
        V* v = page.values();
        dst.header.count = (page.header.count - shift);
        dst.header.split = page.header.split;
        copy<V>(&dst.splitValue(), &sv, dst.header.split);
        copy<V>(dst.values(), v, dst.header.count);
        copy<PageSize>(dst.keyIndeces(), ks, dst.header.count);
        copy<K>(dst.keys(), k, dst.keyIndex(dst.header.count));
    }
    template <class K, class V>
    void PageFunctions::pageShiftRight(
        const Page<K, V, false, true>& page,
        const Page<K, V, false, true>& right,
        Page<K, V, false, true>& dst,
        Page<K, V, false, true>& dstRight,
        PageIndex index
    ) {
        PageSize shift = (page.header.count - index);
        PageSize rn = right.header.count;
        K* rk = right.keys();
        PageIndex vn1 = page.valueIndex(index);
        PageIndex vn2 = page.valueIndex(page.header.count);
        V* rv = right.values();
        PageSize *rvs = right.valueIndeces();
        PageSize rvn = right.valueIndex(right.header.count);
        dstRight.header.count = (right.header.count + shift);
        dstRight.header.split = right.header.split;
        // Make room
        copy<V>(&dstRight.values()[vn2 - vn1], rv, rvn);
        copy<PageSize>(&dstRight.valueIndeces()[shift], rvs, rn);
        copy<K>(&dstRight.keys()[shift], rk, rn);
        // Shift content
        copy<V>(dstRight.splitValue(), right.splitValue(), dstRight.header.split);
        copy<PageSize>(dstRight.valueIndeces(), &page.valueIndeces()[index], shift);
        for (int i = 0; i < shift; i++) dstRight.valueIndeces()[i] -= page.valueIndex(page.header.count - shift);
        for (int i = shift; i < dstRight.header.count; i++) dstRight.valueIndeces()[i] += dstRight.valueIndex(shift);
        copy<K>(dstRight.keys(), &page.keys()[index], shift);
        copy<V>(dstRight.values(), &page.values()[vn1], (vn2 - vn1));
        // Pack
        V* sv = page.splitValue();
        K* k = page.keys();
        PageSize *vs = page.valueIndeces();
        V* v = page.values();
        dst.header.count = (page.header.count - shift);
        dst.header.split = page.header.split;
        copy<V>(dst.splitValue(), sv, dst.header.split);
        copy<K>(dst.keys(), k, dst.header.count);
        copy<PageSize>(dst.valueIndeces(), vs, dst.header.count);
        copy<V>(dst.values(), v, dst.valueIndex(dst.header.count));
    }
    template <class K, class V>
    void PageFunctions::pageShiftRight(
        const Page<K, V, true, true>& page,
        const Page<K, V, true, true>& right,
        Page<K, V, true, true>& dst,
        Page<K, V, true, true>& dstRight,
        PageIndex index
    ) {
        PageSize shift = (page.header.count - index);
        PageSize rn = right.header.count;
        PageIndex kn1 = page.keyIndex(index);
        PageIndex kn2 = page.keyIndex(page.header.count);
        K* rk = right.keys();
        PageSize *rks = right.keyIndeces();
        PageSize rkn = right.keyIndex(right.header.count);
        PageIndex vn1 = page.valueIndex(index);
        PageIndex vn2 = page.valueIndex(page.header.count);
        V* rv = right.values();
        PageSize *rvs = right.valueIndeces();
        PageSize rvn = right.valueIndex(right.header.count);
        dstRight.header.count = (right.header.count + shift);
        dstRight.header.split = right.header.split;
        // Make room
        uint8_t* adrv = (reinterpret_cast<uint8_t *>(rv) + ((kn2 - kn1) * sizeof(K)) + (2 * shift * sizeof(PageSize)));
        V* nrv = reinterpret_cast<V* >(reinterpret_cast<uint8_t *>(&dstRight) + (adrv - reinterpret_cast<uint8_t *>(const_cast<Page<K, V, true, true>*>(&right))));
        copy<V>(&nrv[vn2 - vn1], rv, rvn);
        copy<K>(&dstRight.keys()[kn2 - kn1], rk, rkn);
        copy<PageSize>(&dstRight.valueIndeces()[shift], rvs, rn);
        copy<PageSize>(&dstRight.keyIndeces()[shift], rks, rn);
        // Shift content
        copy<V>(dstRight.splitValue(), right.splitValue(), dstRight.header.split);
        copy<PageSize>(dstRight.keyIndeces(), &page.keyIndeces()[index], shift);
        for (int i = 0; i < shift; i++) dstRight.keyIndeces()[i] -= page.keyIndex(page.header.count - shift);
        for (int i = shift; i < dstRight.header.count; i++) dstRight.keyIndeces()[i] += dstRight.keyIndex(shift);
        copy<PageSize>(dstRight.valueIndeces(), &page.valueIndeces()[index], shift);
        for (int i = 0; i < shift; i++) dstRight.valueIndeces()[i] -= page.valueIndex(page.header.count - shift);
        for (int i = shift; i < dstRight.header.count; i++) dstRight.valueIndeces()[i] += dstRight.valueIndex(shift);
        copy<K>(dstRight.keys(), &page.keys()[kn1], (kn2 - kn1));
        copy<V>(dstRight.values(), &page.values()[vn1], (vn2 - vn1));
        // Pack
        V* sv = page.splitValue();
        PageSize *ks = page.keyIndeces();
        PageSize *vs = page.valueIndeces();
        K* k = page.keys();
        V* v = page.values();
        dst.header.count = (page.header.count - shift);
        dst.header.split = page.header.split;
        copy<V>(dst.splitValue(), sv, dst.header.split);
        copy<PageSize>(dst.keyIndeces(), ks, dst.header.count);
        copy<PageSize>(dst.valueIndeces(), vs, dst.header.count);
        copy<K>(dst.keys(), k, dst.keyIndex(dst.header.count));
        copy<V>(dst.values(), v, dst.valueIndex(dst.header.count));
    }
    template <class K, class V>
    void PageFunctions::pageShiftLeft(
        const Page<K, V, false, false>& page,
        const Page<K, V, false, false>& left,
        Page<K, V, false, false>& dst,
        Page<K, V, false, false>& dstLeft,
        PageIndex index
    ) {
        PageSize ln = left.header.count; 
        V& lsv = left.splitValue();
        K* lk = left.keys();
        V* lv = left.values();
        dstLeft.header.count = (left.header.count + index);
        dstLeft.header.split = left.header.split;
        copy<V>(dstLeft.values(), lv, ln);
        copy<K>(dstLeft.keys(), lk, ln);
        copy<V>(&dstLeft.splitValue(), &lsv, dstLeft.header.split);
        copy<K>(&dstLeft.keys()[ln], page.keys(), index);
        copy<V>(&dstLeft.values()[ln], page.values(), index);
        V& sv = page.splitValue();
        K* k = page.keys();
        V* v = page.values();
        dst.header.count = (page.header.count - index);
        dst.header.split = page.header.split;
        copy<V>(&dst.splitValue(), &sv, dst.header.split);
        copy<K>(dst.keys(), &k[index], dst.header.count);
        copy<V>(dst.values(), &v[index], dst.header.count);
    }
    template <class K, class V>
    void PageFunctions::pageShiftLeft(
        const Page<K, V, true, false>& page,
        const Page<K, V, true, false>& left,
        Page<K, V, true, false>& dst,
        Page<K, V, true, false>& dstLeft,
        PageIndex index
    ) {
        PageSize ln = left.header.count;
        PageIndex kn = page.keyIndex(index);
        V& lsv = left.splitValue();
        K* lk = left.keys();
        PageSize *lks = left.keyIndeces();
        PageIndex lkn = left.keyIndex(left.header.count);
        V* lv = left.values();
        dstLeft.header.count = (left.header.count + index);
        dstLeft.header.split = left.header.split;
        // Make room in left page (high to low)
        copy<K>(dstLeft.keys(), lk, lkn);
        copy<PageSize>(dstLeft.keyIndeces(), lks, ln);
        copy<V>(dstLeft.values(), lv, ln);
        copy<V>(&dstLeft.splitValue(), &lsv, dstLeft.header.split);
        // Shift content
        copy<PageSize>(&dstLeft.keyIndeces()[ln], page.keyIndeces(), index);
        for (int i = (dstLeft.header.count - index); i < dstLeft.header.count; i++) dstLeft.keyIndeces()[i] += lkn;
        copy<K>(&dstLeft.keys()[lkn], page.keys(), kn);
        copy<V>(&dstLeft.values()[ln], page.values(), index);
        // Pack content (low to high)
        V& sv = page.splitValue();
        K* k = page.keys();
        PageSize *ks = page.keyIndeces();
        V* v = page.values();
        dst.header.count = (page.header.count - index);
        dst.header.split = page.header.split;
        copy<V>(&dst.splitValue(), &sv, dst.header.split);
        copy<V>(dst.values(), &v[index], dst.header.count);
        copy<PageSize>(dst.keyIndeces(), &ks[index], dst.header.count);
        for (int i = 0; i < dst.header.count; i++) dst.keyIndeces()[i] -= kn;
        copy<K>(dst.keys(), &k[kn], dst.keyIndex(dst.header.count));
    }
    template <class K, class V>
    void PageFunctions::pageShiftLeft(
        const Page<K, V, false, true>& page,
        const Page<K, V, false, true>& left,
        Page<K, V, false, true>& dst,
        Page<K, V, false, true>& dstLeft,
        PageIndex index
    ) {
        PageSize ln = left.header.count;
        PageIndex vn = page.valueIndex(index);
        V* lsv = left.splitValue();
        K* lk = left.keys();
        V* lv = left.values();
        PageSize *lvs = left.valueIndeces();
        PageIndex lvn = left.valueIndex(left.header.count);
        dstLeft.header.count = (left.header.count + index);
        dstLeft.header.split = left.header.split;
        copy<V>(dstLeft.values(), lv, lvn);
        copy<K>(dstLeft.keys(), lk, ln);
        copy<PageSize>(dstLeft.valueIndeces(), lvs, ln);
        copy<V>(dstLeft.splitValue(), lsv, dstLeft.header.split);
        // Shift content
        copy<K>(&dstLeft.keys()[ln], page.keys(), index);
        copy<PageSize>(&dstLeft.valueIndeces()[ln], page.valueIndeces(), index);
        for (int i = (dstLeft.header.count - index); i < dstLeft.header.count; i++) dstLeft.valueIndeces()[i] += lvn;
        copy<V>(&dstLeft.values()[lvn], page.values(), vn);
        // Pack content (low to high)
        V* sv = page.splitValue();
        K* k = page.keys();
        V* v = page.values();
        PageSize *vs = page.valueIndeces();
        dst.header.count = (page.header.count - index);
        dst.header.split = page.header.split;
        copy<V>(dst.splitValue(), sv, dst.header.split);
        copy<K>(dst.keys(), &k[index], dst.header.count);
        copy<PageSize>(dst.valueIndeces(), &vs[index], dst.header.count);
        for (int i = 0; i < dst.header.count; i++) dst.valueIndeces()[i] -= vn;
        copy<V>(dst.values(), &v[vn], dst.valueIndex(dst.header.count));
    }
    template <class K, class V>
    void PageFunctions::pageShiftLeft(
        const Page<K, V, true, true>& page,
        const Page<K, V, true, true>& left,
        Page<K, V, true, true>& dst,
        Page<K, V, true, true>& dstLeft,
        PageIndex index
    ) {
        PageSize ln = left.header.count;
        PageIndex kn = page.keyIndex(index);
        PageIndex vn = page.valueIndex(index);
        V* lsv = left.splitValue();
        K* lk = left.keys();
        PageSize *lks = left.keyIndeces();
        PageIndex lkn = left.keyIndex(left.header.count);
        V* lv = left.values();
        PageSize *lvs = left.valueIndeces();
        PageIndex lvn = left.valueIndex(left.header.count);
        dstLeft.header.count = (left.header.count + index);
        dstLeft.header.split = left.header.split;
        // Make room in left page (high to low)
        uint8_t* adrv = (reinterpret_cast<uint8_t *>(lv) + (kn * sizeof(K)) + (2 * index * sizeof(PageSize)));
        V* nlv = reinterpret_cast<V* >(reinterpret_cast<uint8_t *>(&dstLeft) + (adrv - reinterpret_cast<uint8_t *>(const_cast<Page<K, V, true, true>*>(&left))));
        copy<V>(nlv, lv, lvn);
        copy<K>(dstLeft.keys(), lk, lkn);
        copy<PageSize>(dstLeft.valueIndeces(), lvs, ln);
        copy<PageSize>(dstLeft.keyIndeces(), lks, ln);
        copy<V>(dstLeft.splitValue(), lsv, dstLeft.header.split);
        // Shift content
        copy<PageSize>(&dstLeft.keyIndeces()[ln], page.keyIndeces(), index);
        for (int i = (dstLeft.header.count - index); i < dstLeft.header.count; i++) dstLeft.keyIndeces()[i] += lkn;
        copy<PageSize>(&dstLeft.valueIndeces()[ln], page.valueIndeces(), index);
        for (int i = (dstLeft.header.count - index); i < dstLeft.header.count; i++) dstLeft.valueIndeces()[i] += lvn;
        copy<K>(&dstLeft.keys()[lkn], page.keys(), kn);
        copy<V>(&dstLeft.values()[lvn], page.values(), vn);
        // Pack content (low to high)
        V* sv = page.splitValue();
        K* k = page.keys();
        PageSize *ks = page.keyIndeces();
        V* v = page.values();
        PageSize *vs = page.valueIndeces();
        dst.header.count = (page.header.count - index);
        dst.header.split = page.header.split;
        copy<V>(dst.splitValue(), sv, dst.header.split);
        copy<PageSize>(dst.keyIndeces(), &ks[index], dst.header.count);
        for (int i = 0; i < dst.header.count; i++) dst.keyIndeces()[i] -= kn;
        copy<PageSize>(dst.valueIndeces(), &vs[index], dst.header.count);
        for (int i = 0; i < dst.header.count; i++) dst.valueIndeces()[i] -= vn;
        copy<K>(dst.keys(), &k[kn], dst.keyIndex(dst.header.count));
        copy<V>(dst.values(), &v[vn], dst.valueIndex(dst.header.count));
    }

}; // namespace BTRee

#endif // BTREE_PAGE_FUNCTIONS_IMP_H