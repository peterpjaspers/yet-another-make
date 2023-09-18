#ifndef BTREE_TRAIL_H
#define BTREE_TRAIL_H

#include "Page.h"
#include "PagePool.h"
#include "TreeBase.h"
#include <array>

namespace BTree {

    // A Trail maintains a path through a B-tree.
    class Trail {
    public:
        // Maximum height of a B-tree.
        // Enforcing a maximum height enables static allocation of Trail objects.
        static const PageDepth MaxHeight = 16;
        // The position in a Page of a Trail entry.
        enum Position {
            Undefined   = 0, // Position undefined
            OnIndex     = 1, // On page index
            AfterIndex  = 2, // After page index; i.e., between two indeces or after last index
            OnSplit     = 3, // On page split
            AfterSplit  = 4  // After page split; i.e., before first index (if any)
        };
        // The position is OnIndex or OnSplit if the Trail is positioned at an existing key.
        // The position is AfterSplit or AfterIndex if the Trail is positioned at a non-existing key.
    private:
        // An TrailEntry points to a particular key-value position in a BTree Page.
        // A position in the B-tree is either a located key-value for an existing key or a location at which
        // the key-value pair would be inserted if it does not exist.
        struct TrailEntry
        {
            // The PageLink associated with the Page in a Trail, either a Leaf Page or a Node Page.
            const PageHeader* header;
            // Position of Trail in this Page.
            Position position;
            // The index in the Page for the associated with the Page position.
            // In range [ 0, count ). 0 for split positions.
            PageIndex index;
        };
        const TreeBase& tree;
        std::array<TrailEntry,MaxHeight> stack;
        PageDepth height;
        inline bool isSplit(Position position) const {
            return ((position == OnSplit) || (position == AfterSplit));
        }
        inline bool isIndex(Position position) const {
            return ((position == OnIndex) || (position == AfterIndex));
        }
    public:
        Trail() = delete;
        inline Trail( const TreeBase& base ) : tree( base ), height( 0 ) {
            stack[height++] = { tree.root, Undefined, 0 };
        }
        Trail( const Trail& trail ) : tree( trail.tree ) {
            height = trail.height;
            for (PageDepth i = 0; i < height; i++) stack[i] = trail.stack[i];
        }
        Trail& operator=( const Trail& trail ) {
            static const char* signature("Trail& Trail::operator=( const Trail& trail ");
            if (&tree.pool != &trail.tree.pool) throw std::string( signature ) + " - Only valid on same page pool";
            height = trail.height;
            for (PageDepth i = 0; i < height; i++) stack[i] = trail.stack[i];
            return *this;
        }
        friend bool operator==(const Trail& a, const Trail& b);
        friend bool operator!=(const Trail& a, const Trail& b);
        const TreeBase& sourceTree() const { return tree; }
        inline Trail& push(const PageHeader *header) {
            static const char* signature("Trail& Trail::push( const PageHeader* header )");
            if (height == MaxHeight) throw std::string( signature ) + " - trail overflow.";
            stack[height++] = {header, Undefined, 0};
            return *this;
        }
        inline Trail& push(const PageHeader *header, Position position, PageIndex index = 0) {
            static const char* signature("Trail& Trail::push( const PageHeader* header, Position position, PageIndex index )");
            if (height == MaxHeight) throw std::string( signature ) + " - trail overflow.";
            stack[height++] = {header, position, index};
            return *this;
        }
        inline Trail& pushSplit(const PageHeader *header) {
            return push( header, ( (header->depth == 0) ? Trail::OnSplit : Trail::AfterSplit ) );
        }
        inline Trail& pushIndex(const PageHeader *header, PageIndex index, bool on = true ) {
            return push( header, (on ? OnIndex : AfterIndex) , index );
        }
        inline Trail& pop() {
            static const char* signature("Trail& Trail::pop()");
            if (height == 0) throw std::string( signature ) + " - trail underflow.";
            height--;
            return *this;
        }
        inline PageDepth depth() const { return height; }
        // Return header of current Page on Trail
        inline const PageHeader *header(PageDepth offset = 0) const {
            static const char* signature("const PageHeader* Trail::header( PageDepth offset ) const");
            if (height < (offset + 1)) throw std::string( signature ) + " - Invalid offset.";
            return stack[height - offset - 1].header;
        }
        // Return current Page position on Trail
        inline Position position(PageDepth offset = 0) const {
            static const char* signature("Position Trail::position( PageDepth offset ) const");
            if (height < (offset + 1)) throw std::string( signature ) + " - Invalid offset.";
            return stack[height - offset - 1].position;
        }
        // Return current Page index on Trail
        inline PageIndex index( PageDepth offset = 0 ) const {
            static const char* signature("PageIndex Trail::index( PageDepth offset ) const");
            if (height < (offset + 1)) throw std::string( signature ) + " - Invalid offset.";
            return stack[height - offset - 1].index;
        }
        // Set Page position on Trail
        inline void position( Position position, PageIndex index = 0, PageDepth offset = 0 ) {
            static const char* signature("void Trail::position( Position position, PageIndex index, PageDepth offset )");
            if (height < (offset + 1)) throw std::string( signature ) + " - Invalid offset.";
            if (position == Undefined) throw std::string( signature ) + " - Invalid Undefined position.";
            if (isSplit(position) && (index != 0)) throw std::string( signature ) + " - Split index must be zero.";
            const PageHeader *header = this->header(offset);
            if (isIndex(position) && (header->count <= index)) throw std::string( signature ) + " - Invalid Page index.";
            stack[height - offset - 1].position = position;
            stack[height - offset - 1].index = index;
        }
        // Determine if trail is positioned at a split.
        // Returns true if positioned on or after a spilt, false otherwise.
        inline bool atSplit(PageDepth offset = 0) const {
            static const char* signature("bool Trail::atSplit( PageDepth offset ) const");
            if (height < (offset + 1)) throw std::string( signature ) + " - Invalid offset.";
            return isSplit(stack[height - offset - 1].position);
        }
        // Determine if trail is positioned on a split.
        // Returns true if positioned on a spilt, false otherwise.
        inline bool onSplit(PageDepth offset = 0) const {
            static const char* signature("bool Trail::onSplit( PageDepth offset ) const");
            if (height < (offset + 1)) throw std::string( signature ) + " - Invalid offset.";
            return (stack[height - offset - 1].position == OnSplit);
        }
        // Determine if trail is positioned at a key-value index.
        // Returns true if positioned on or after an index, false otherwise.
        inline bool atIndex(PageDepth offset = 0) const {
            static const char* signature("bool Trail::atIndex( PageDepth offset ) const");
            if (height < (offset + 1)) throw std::string( signature ) + " - Invalid offset.";
            return isIndex(stack[height - offset - 1].position);
        }
        // Determine if trail is positioned on a key-value index.
        // Returns true if positioned on an index, false otherwise.
        inline bool onIndex(PageDepth offset = 0) const {
            static const char* signature("bool Trail::onIndex( PageDepth offset ) const");
            if (height < (offset + 1)) throw std::string( signature ) + " - Invalid offset.";
            return (stack[height - offset - 1].position == OnIndex);
        }
        // Determine offset in Trail to a key match; offset to a search key.
        // Returns zero if no match was found.
        inline PageDepth match(PageDepth initial = 0) const {
            for (PageDepth offset = initial; offset < height; offset++) {
                Position position = stack[height - offset - 1].position;
                if ((position == OnIndex) || (position == AfterIndex)) return (offset);
            }
            return (0);
        }
        // Determine offset in Trail to an exact key match; i.e., non-zero offset to a split key.
        // Returns zero if no match was found.
        inline PageDepth exactMatch() const {
            for (PageDepth offset = 0; offset < height; offset++) {
                Position position = stack[height - offset - 1].position;
                if (position == OnIndex) return (offset);
            }
            return (0);
        }
        // Pop the Trail to a key match; i.e., go up all split key levels.
        // Does nothing if no match was found.
        inline Trail& popToMatch() { height -= match(); return *this; }
        // Empty the trail to the root page header
        inline Trail& clear() {
            height = 0;
            stack[height++] = {tree.root, Undefined, 0};
            return *this;
        }
        // Update trail to reflect deleted index.
        Trail& deletedIndex( PageDepth offset = 0 ) {
            static const char* signature("void Trail::deletedIndex( PageDepth offset )");
            if (height < (offset + 1)) throw std::string( signature ) + " - Invalid offset.";
            PageIndex index = this->index( offset );
            if (0 < index) {
                position( AfterIndex, (index - 1), offset );
            } else if (0 < header()->split) {
                position( AfterSplit, 0, offset );
            } else {
                // Only Page that does not have a split value is very first Leaf Page.
                stack[height - offset - 1].position = Undefined;
                stack[height - offset - 1].index = 0;
            }
            return *this;
        }
        // Return pointer to PageHeader of Page referred by the current Trail position.
        // Trail must be positioned in a Node page.
        template <class K, bool KA>
        const PageHeader *pageHeader(PageDepth offset = 0) const {
            static const char* signature("const PageHeader* Trail::pageHeader( PageDepth offset ) const");
            if (height < (offset + 1)) throw std::string( signature ) + " - Invalid offset.";
            const PageHeader *header = this->header(offset);
            if (header->depth == 0) throw std::string( signature ) + " - Trail does not reference a Node Page";
            const Page<K,PageLink,KA,false> *nodePage = tree.pool.page<K,PageLink,KA,false>(header);
            if (atSplit(offset)) {
                return tree.pool.reference(nodePage->split());
            } else {
                return tree.pool.reference(nodePage->value(index(offset)));
            }
        };
        // Navigate to next position in Trail
        // Returns true if a next position exists, false otherwise.
        template <class K, bool KA>
        bool next() {
            static const char* signature("bool Trail::next()");
            if (atSplit() && (0 < header()->count)) {
                position( OnIndex, 0 );
                return true;
            } else if ((index() + 1) < header()->count) {
                position( OnIndex, (index() + 1) );
                return true;
            }
            if (1 < height) {
                pop();
                if (next<K,KA>()) {
                    const PageHeader *header = pageHeader<K, KA>();
                    if (0 < header->split) {
                        push(header, OnSplit);
                    } else {
                        if (header->count == 0) throw std::string( signature ) + " - Navigating to empty page.";
                        push(header, OnIndex, 0);
                    }
                    return true;
                }
            } else {
                // No next entry, set to end
                end<K,KA>();
            }
            return false;
        }
        // Navigate to previous position in trail.
        // Returns true if a previous position exists, false otherwise.
        template <class K, bool KA>
        bool previous() {
            static const char* signature("bool Trail::previous()");
            if (atSplit()) {
                if (height <= 1) return false;
                pop();
                if (previous<K,KA>()) {
                    const PageHeader *header = pageHeader<K, KA>();
                    if (0 < header->count) {
                        push( header, OnIndex, (header->count - 1) );
                        return true;
                    } else {
                        if (header->split == 0) throw std::string( signature ) + " - Navigating to empty page.";
                        push( header, OnSplit );
                        return true;
                    }
                }
            } else if (0 < index()) {
                position( OnIndex, (index() - 1) );
                return true;
            } else if (0 < header()->split) {
                // Only Page that does not have a split value is very first Leaf Page.
                position( OnSplit );
                return true;
            }
            return false;
        }
        // Set trail to begin
        template< class K, bool KA >
        Trail& begin() {
            static const char* signature( "Trail& Trail::begin()" );
            const PageHeader* header = tree.root;
            if (header->free == 1)  throw std::string(signature) + " - Accessing freed page.";
            height = 0;
            while (0 < header->depth) {
                const Page<K,PageLink,KA,false>* node = tree.pool.page<K,PageLink,KA,false>( header );
                push( header, AfterSplit );
                header = &tree.pool.access( node->split() );
                if (header->free == 1)  throw std::string(signature) + " - Accessing freed page.";
            }
            push( header, OnIndex, 0 );
            return *this;
        }
        // Set trail to end
        template< class K, bool KA >
        Trail& end() {
            static const char* signature( "Trail& Trail::end()" );
            const PageHeader* header = tree.root;
            if (header->free == 1)  throw std::string(signature) + " - Accessing freed page.";
            height = 0;
            while (0 < header->depth) {
                const Page<K,PageLink,KA,false>* node = tree.pool.page<K,PageLink,KA,false>( header );
                if (0 < header->count) {
                    push( header, OnIndex, (header->count - 1) );
                    header = &tree.pool.access( node->value( header->count - 1 ) );
                } else {
                    push( header, OnSplit );
                    header = &tree.pool.access( node->split() );
                }
                if (header->free == 1)  throw std::string(signature) + " - Accessing freed page.";
            }
            push( header, OnIndex, header->count );
            return *this;
        }

        template< class K, class V, bool KA, bool VA >
        Page<K,V,KA,VA>* page( PageDepth offset = 0 ) const { return tree.pool.page<K,V,KA,VA>( header( offset ) ); }

    }; // class Trail

    bool operator==(const Trail& a, const Trail& b) {
        if (&a.tree != &b.tree) return false;
        if (a.height != b.height) return false;
        for (PageDepth i = 0; i < a.height; i++) {
            if (
                (a.stack[i].header != b.stack[i].header) ||
                (a.stack[i].position != b.stack[i].position) ||
                (a.stack[i].index != b.stack[i].index)
            ) return false;
        }
        return true;
    };
    bool operator!=(const Trail& a, const Trail& b) {
        if (&a.tree != &b.tree) return true;
        if (a.height != b.height) return true;
        for (PageDepth i = 0; i < a.height; i++) {
            if (
                (a.stack[i].header != b.stack[i].header) ||
                (a.stack[i].position != b.stack[i].position) ||
                (a.stack[i].index != b.stack[i].index)
            ) return true;
        }
        return false;
    };

} // namespace BTree

#endif // BTREE_TRAIL_H
