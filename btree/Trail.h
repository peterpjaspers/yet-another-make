#ifndef BTREE_TRAIL_H
#define BTREE_TRAIL_H

#include "Page.h"
#include "PagePool.h"
#include <array>

namespace BTree {

    // Maximum height of a B-tree.
    // Enforcing a maximum height enables various optimizations.
    const PageDepth MaxHeight = 8;

    // A Trail maintains a path through a B-tree.
    class Trail {
        // An TrailEntry points to a particular key-value pair in a BTree Page.
        struct TrailEntry {
            // The PageLink associated with the Page in which the key-value pair resides
            // This is either a Leaf Page or a Node Page.
            const PageHeader* header;
            // The index in the page at which the key-value pair is located
            // In range [ 0, size ). 0 for split value and first indexed key-value.
            PageIndex index;
            // Comparison result with key at index in node.
            // 0 if key compares equal to key at index in this Leaf or Node Page.
            // Negative is key compares less than key at index 0, only for split values.
            // Positive if key compares greater than key at index in Node Page.
            KeyCompare compare;
        };
        PagePool& pool;
        std::array< TrailEntry, MaxHeight > stack;
        PageDepth height;
    public:
    Trail() = delete;
        inline Trail( PagePool& p ) : pool( p ), height( 0 ) {}
        inline Trail( PagePool& pool, const PageHeader* header ) : Trail( pool ) {
            stack[ height++ ] = { header, 0, -1 };
        }
        Trail( const Trail& trail ) : Trail( trail.pool ) {
            height = trail.height;
            for (PageDepth i = 0; i < height; i++) stack[ i ] = trail.stack[ i ];
        }
        friend bool operator== (const Trail& a, const Trail& b) {
            if (a.height != b.height) return false;
            for (PageDepth i = 0; i < a.height; i++) {
                if (
                    (*(a.stack[ i ].header) != *(b.stack[ i ].header)) ||
                    (a.stack[ i ].index != b.stack[ i ].index) ||
                    (a.stack[ i ].compare != b.stack[ i ].compare)
                ) return false;
        	}
            return true;
        };
        friend bool operator!= (const Trail& a, const Trail& b) {
            if (a.height != b.height) return true;
            for (PageDepth i = 0; i < a.height; i++) {
                if (
                    (*(a.stack[ i ].header) != *(b.stack[ i ].header)) ||
                    (a.stack[ i ].index != b.stack[ i ].index) ||
                    (a.stack[ i ].compare != b.stack[ i ].compare)
                ) return true;
        	}
            return false;
        };
        
        inline Trail& push( const PageHeader* header, PageIndex index, KeyCompare compare  ) {
            static const std::string signature( "Trail& push( const PageHeader* header, PageIndex index, KeyCompare compare  )" );
            if (height == MaxHeight) throw signature + " - trail overflow.";
            stack[ height++ ] = { header, index, compare };
            return *this;
        }
        inline Trail& push( const PageHeader* header ) {
            static const std::string signature( "Trail& push( const PageHeader* header )" );
            if (height == MaxHeight) throw signature + " - trail overflow.";
            stack[ height++ ].header = header;
            return *this;
        }
        inline Trail&  pop() {
            static const std::string signature( "Trail& pop()" );
            if (height == 0) throw signature + " - trail underflow.";
            height--;
            return *this;
        }
        inline PageDepth depth() const { return height; }
        // Return header of current Page on Trail
        inline const PageHeader* header( PageDepth offset = 0 ) const {
            static const std::string signature( "const PageHeader* header( PageDepth offset = 0 ) const" );
            if (height < (offset + 1)) throw signature + " - Invalid offset.";
            return stack[ height - offset - 1 ].header;
        }
        // Return current Page index on Trail
        inline PageIndex index( PageDepth offset = 0 ) const {
            static const std::string signature( "PageIndex index( PageDepth offset = 0 ) const" );
            if (height < (offset + 1)) throw signature + " - Invalid offset.";
            return stack[ height - offset - 1 ].index;
        }
        // Return current comparison result on Trail
        inline KeyCompare compare( PageDepth offset = 0 ) const {
            static const std::string signature( "KeyCompare compare( PageDepth offset = 0 ) const" );
            if (height < (offset + 1)) throw signature + " - Invalid offset.";
            return stack[ height - offset - 1 ].compare;
        }
        inline void index( PageIndex i, KeyCompare c ) {
            stack[ height - 1 ].index = i; stack[ height - 1 ].compare = c;
        }
        inline void compare( KeyCompare c ) { stack[ height - 1 ].compare = c; }
        inline void compare( PageDepth offset, KeyCompare c ) {
            static const std::string signature( "void compare( PageDepth offset, KeyCompare c )" );
            if (height < (offset + 1)) throw signature + " - Invalid offset.";
            stack[ height - offset - 1 ].compare = c;
        }
        inline bool split( PageDepth offset = 0 ) const {
            static const std::string signature( "bool split( PageDepth offset = 0 ) const" );
            if (height < (offset + 1)) throw signature + " - Invalid offset.";
            return( (stack[ height - offset - 1 ].compare < 0) && (stack[ height - offset - 1 ].index == 0) );
        }
        // Determine level in tree at which a match was found.
        // Returns zero if no match was found.
        inline PageDepth match() const {
            for (PageDepth offset = 0; offset < height; offset++) {
                if (0 <= stack[ height - offset - 1 ].compare) return( offset );
            }
            return( 0 );
        }
        // Pop the trail to the level of a match
        inline void popToMatch() {
            static const std::string signature( "void popToMatch()" );
            if (height == 0) throw signature + " - trail underflow.";
            for (PageDepth offset = 0; offset < height; offset++) {
                if (0 <= stack[ height - offset - 1 ].compare) {
                    height -= offset;
                    break;
                }
            }
        }

        inline void clear( const PageHeader* header ) {
            height = 0;
            stack[ height++ ] = { header, 0, -1 };
        }
        // Return pointer to PageHeader indexed by trail.
        // Trail is assumed to access a Node page.
        template< class K, bool KA >
        const PageHeader* indexedHeader( PageDepth offset = 0 ) const {
            static const std::string signature( "const PageHeader* indexedHeader( PageDepth offset = 0 ) const" );
            if (height < (offset + 1)) throw signature + " - Invalid offset.";
            const PageHeader* header = this->header( offset );
            if (header->depth == 0) throw signature + " - Trail does not reference a Node Page";
            const Page<K,PageLink,KA,false>* nodePage = pool.page<K,PageLink,KA,false>( header );
            if (split( offset )) {
                return pool.reference( nodePage->split() );
            } else {
                return pool.reference( nodePage->value( index( offset ) ) );
            }
        };

        // Navigate to next key-value index in trail
        template< class K, bool KA >
        bool next() {
            if (split()) {
                compare( match(), +1 );
                index( 0, 0 );
                return true;
            } else if ((index() + 1) < header()->count) {
                index( (index() + 1), 0 );
                return true;
            } else if (1 < depth()) {
                pop();
                if (next<K,KA>()) {
                    const PageHeader* header = indexedHeader<K,KA>();
                    push( header, 0, -1 );
                    return true;
                }
            }
            return false;
        }
        // Navigate to next leaf Page in trail
        template< class K, bool KA >
        bool nextPage() {
            if ((1 < depth()) && (header()->depth == 0)) pop();
            return next<K,KA>();
        }

        // Navigate to previous key-value index in trail
        template< class K, bool KA >
        bool previous() {
            if (split()) {
                if (1 < depth()) {
                    pop();
                    if (previous<K,KA>()) {
                        const PageHeader* header = indexedHeader<K,KA>();
                        push( header, (header->count - 1), +1 );
                        return true;
                    }
                }
            } else if (0 < index()) {
                index( (index() - 1), 0 );
                return true;
            } else {
                index( 0, -1  );
                return true;
            }
            return false;
        }
        // Navigate to previous leaf Page in trail
        template< class K, bool KA >
        bool previousPage() {
            if ((1 < depth()) && (header()->depth == 0)) pop();
            return previous<K,KA>();
        }

    }; // class Trail

} // namespace BTree

#endif // BTREE_TRAIL_H
