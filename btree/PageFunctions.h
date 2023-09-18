#ifndef BTREE_PAGE_FUNCTIONS_H
#define BTREE_PAGE_FUNCTIONS_H

#include <new>
#include <stdlib.h>
#include <cstdint>
#include <string.h>
#include <iostream>
#include <iomanip>

#include "Types.h"

namespace BTree {

    class PageFunctions {
    public:

        // Set scalar split value in a scalar key to scalar value page
        template <class K, class V>
        static void pageSplit(
            const Page<K, V, false, false>& page,
            Page<K, V, false, false>& dst,
            const V& value
        );
        // Set scalar split value in a array key to scalar value page
        template <class K, class V>
        static void pageSplit(
            const Page<K, V, true, false>& page,
            Page<K, V, true, false>& dst,
            const V& value
        );
        // Set array split value in scalar key key to array value page
        template <class K, class V>
        static void pageSplit(
            const Page<K, V, false, true>& page,
            Page<K, V, false, true>& dst,
            const V* value, PageSize valueSize
        );
        // Set array split value in array key to array value page
        template <class K, class V>
        static void pageSplit(
            const Page<K, V, true, true>& page,
            Page<K, V, true, true>& dst,
            const V* value, PageSize valueSize
        );

        // Remove split value in scalar key scalar value page
        template <class K, class V>
        static void pageRemoveSplit(
            const Page<K, V, false, false>& page,
            Page<K, V, false, false>& dst
        );
        // Remove split value in array key scalar value page
        template <class K, class V>
        static void pageRemoveSplit(
            const Page<K, V, true, false>& page,
            Page<K, V, true, false>& dst
        );
        // Remove split value in scalar key array value page
        template <class K, class V>
        static void pageRemoveSplit(
            const Page<K, V, false, true>& page,
            Page<K, V, false, true>& dst
        );
        // Remove split value in array key array value page
        template <class K, class V>
        static void pageRemoveSplit(
            const Page<K, V, true, true>& page,
            Page<K, V, true, true>& dst
        );
        // Insert an scalar key scalar value pair at an index
        template <class K, class V>
        static void pageInsert(
            const Page<K, V, false, false>& page,
            Page<K, V, false, false>& dst,
            PageIndex index,
            const K& key,
            const V& value
        );
        // Insert an array key scalar value pair at an index
        template <class K, class V>
        static void pageInsert(
            const Page<K, V, true, false>& page,
            Page<K, V, true, false>& dst,
            PageIndex index,
            const K* key, const PageSize keySize,
            const V& value
        );
        // Insert a scalar key array value pair at an index
        template <class K, class V>
        static void pageInsert(
            const Page<K, V, false, true>& page,
            Page<K, V, false, true>& dst,
            PageIndex index,
            const K& key,
            const V* value, const PageSize valueSize
        );
        // Insert an array key array value pair at an index
        template <class K, class V>
        static void pageInsert(
            const Page<K, V, true, true>& page,
            Page<K, V, true, true>& dst,
            PageIndex index,
            const K* key, const PageSize keySize,
            const V* value, const PageSize valueSize
        );
        // Replace a scalar value at an index
        template <class K, class V>
        static void pageReplace(
            const Page<K, V, false, false>& page,
            Page<K, V, false, false>& dst,
            PageIndex index,
            const V& value
        );
        // Replace a scalar value at an index
        template <class K, class V>
        static void pageReplace(
            const Page<K, V, true, false>& page,
            Page<K, V, true, false>& dst,
            PageIndex index,
            const V& value
        );
        // Replace an array value at an index
        template <class K, class V>
        static void pageReplace(
            const Page<K, V, false, true>& page,
            Page<K, V, false, true>& dst,
            PageIndex index,
            const V* value,
            const PageSize valueSize
        );
        // Replace an array value at an index
        template <class K, class V>
        static void pageReplace(
            const Page<K, V, true, true>& page,
            Page<K, V, true, true>& dst,
            PageIndex index,
            const V* value,
            const PageSize valueSize
        );
        // Replace a scalar key scalar value pair at an index
        template <class K, class V>
        static void pageReplace(
            const Page<K, V, false, false>& page,
            Page<K, V, false, false>& dst,
            PageIndex index,
            const K& key,
            const V& value
        );
        // Replace an array key scalar value pair at an index
        template <class K, class V>
        static void pageReplace(
            const Page<K, V, true, false>& page,
            Page<K, V, true, false>& dst,
            PageIndex index,
            const K* key,
            PageSize keySize,
            const V& value
        );
        // Replace a scalar key array value pair at an index
        template <class K, class V>
        static void pageReplace(
            const Page<K, V, false, true>& page,
            Page<K, V, false, true>& dst,
            PageIndex index,
            const K& key,
            const V* value,
            const PageSize valueSize
        );
        // Replace an array key array value pair at an index
        template <class K, class V>
        static void pageReplace(
            const Page<K, V, true, true>& page,
            Page<K, V, true, true>& dst,
            PageIndex index,
            const K* key,
            PageSize keySize,
            const V* value,
            const PageSize valueSize
        );

        // Remove the scalar key scalar value pair at an index
        template <class K, class V>
        static void pageRemove(
            const Page<K, V, false, false>& page,
            Page<K, V, false, false>& dst,
            PageIndex index
        );
        // Remove the array key scalar value pair at an index
        template <class K, class V>
        static void pageRemove(
            const Page<K, V, true, false>& page,
            Page<K, V, true, false>& dst,
            PageIndex index
        );
        // Remove the scalar key array value pair at an index
        template <class K, class V>
        static void pageRemove(
            const Page<K, V, false, true>& page,
            Page<K, V, false, true>& dst,
            PageIndex index
        );
        // Remove the array key array value pair at an index
        template <class K, class V>
        static void pageRemove(
            const Page<K, V, true, true>& page,
            Page<K, V, true, true>& dst,
            PageIndex index
        );
        // Move scalar key scalar value entries from indexed entry up to last entry
        // preprending entries to right page.
        template <class K, class V>
        static void pageShiftRight(
            const Page<K, V, false, false>& page,
            const Page<K, V, false, false>& right,
            Page<K, V, false, false>& dst,
            Page<K, V, false, false>& dstRight,
            PageIndex index
        );
        // Move array key scalar value entries from indexed entry up to last entry
        // preprending entries to right page.
        template <class K, class V>
        static void pageShiftRight(
            const Page<K, V, true, false>& page,
            const Page<K, V, true, false>& right,
            Page<K, V, true, false>& dst,
            Page<K, V, true, false>& dstRight,
            PageIndex index
        );
        // Move scalar key array value entries from indexed entry up to last entry
        // preprending entries to right page.
        template <class K, class V>
        static void pageShiftRight(
            const Page<K, V, false, true>& page,
            const Page<K, V, false, true>& right,
            Page<K, V, false, true>& dst,
            Page<K, V, false, true>& dstRight,
            PageIndex index
        );
        // Move array key array value entries from indexed entry up to last entry
        // preprending entries to right page.
        template <class K, class V>
        static void pageShiftRight(
            const Page<K, V, true, true>& page,
            const Page<K, V, true, true>& right,
            Page<K, V, true, true>& dst,
            Page<K, V, true, true>& dstRight,
            PageIndex index
        );
        // Move scalar key scalar value entries from first entry up to indexed entry
        // appending entries to left page.
        template <class K, class V>
        static void pageShiftLeft(
            const Page<K, V, false, false>& page,
            const Page<K, V, false, false>& left,
            Page<K, V, false, false>& dst,
            Page<K, V, false, false>& dstLeft,
            PageIndex index
        );
        // Move array key scalar value entries from first entry up to indexed entry
        // appending entries to left page.
        template <class K, class V>
        static void pageShiftLeft(
            const Page<K, V, true, false>& page,
            const Page<K, V, true, false>& left,
            Page<K, V, true, false>& dst,
            Page<K, V, true, false>& dstLeft,
            PageIndex index
        );
        // Move scalar key array value entries from first entry up to indexed entry
        // appending entries to left page.
        template <class K, class V>
        static void pageShiftLeft(
            const Page<K, V, false, true>& page,
            const Page<K, V, false, true>& left,
            Page<K, V, false, true>& dst,
            Page<K, V, false, true>& dstLeft,
            PageIndex index
        );
        // Move array key array value entries from first entry up to indexed entry
        // appending entries to left page.
        template <class K, class V>
        static void pageShiftLeft(
            const Page<K, V, true, true>& page,
            const Page<K, V, true, true>& left,
            Page<K, V, true, true>& dst,
            Page<K, V, true, true>& dstLeft,
            PageIndex index
        );

    }; // class PageFunctions

}; // namespace BTRee

#include "PageFunctions_Imp.h"

#endif // BTREE_PAGE_FUNCTIONS_H