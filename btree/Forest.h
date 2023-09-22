#ifndef BTREE_FOREST_H
#define BTREE_FOREST_H

#include "TreeBase.h"
#include "BTree.h"
#include "StreamingBTree.h"
#include <map>

namespace BTree {

    // A B-Tree Forest is a B-Tree that contains a number of B-Trees.
    //
    // Transaction semantics are simultaneously applied to all B-Trees in the Forest.
    // The B-Trees in a Forest share a single (persistent) PagePool as well
    // as update mode.
    //
    // Each B-Tree can have its own key and value class template values
    // allowing a hetrogeneous collection of B-Trees to reside in the Forest.
    //
    // B-Trees in a Forest are created by plating them in the Forest. See "plant" method.
    //
    // To recover a B-Tree from a persistent store, each B-Tree is associated
    // with an (integer) TreeIndex which a application can use to access a Forest
    // created/restored from persistent storage. The TreeIndex is generated when a
    // B-Tree is first planted in the Forest. See "access" method.
    //
    // The application must supply the proper key and value class template (and optional
    // key compare) arguments when accessing a B-Tree in a Forest. Behaviour is undefined
    // when such arguments do not match those used when the B-Tree was planted.
    //
    class Forest : public Tree< TreeIndex, PageLink > {
        std::map<TreeIndex,const TreeBase*> trees;
        friend std::ostream & operator<<( std::ostream & o, const Forest& tree );
    public:
        // Create an empty Forest
        Forest( PagePool& pagePool, UpdateMode updateMode = UpdateMode::Auto ) :
            Tree< TreeIndex, PageLink >( pagePool, defaultCompareScalar<TreeIndex>, updateMode )
        {}
        // Destroy Forest and all B-Trees in the Forest.
        ~Forest() { 
            for ( auto tree : trees ) 
                delete tree.second; 
        }
        // Plant a B-Tree in the Forest with key and value class template arguments and
        // an optional key comparison function. The created B-Tree shares the PagePool
        // and page UpdateMode with all other B-Trees in the Forest
        // ToDo; plant with user defined index...
        template< class K, class V, std::enable_if_t<(S<K>),bool> = true >
        std::pair< Tree<K,V>*, TreeIndex > plant( typename Tree<K,V>::ScalarKeyCompare compareKey = defaultCompareScalar<K> ) {
            TreeIndex index = uniqueIndex();
            return{ plant<K,V>( uniqueIndex(), compareKey ), index };
        }
        template< class K, class V, std::enable_if_t<(A<K>),bool> = true >
        std::pair< Tree<K,V>*, TreeIndex > plant( typename Tree<K,V>::ArrayKeyCompare compareKey = defaultCompareArray<B<K>> ) {
            TreeIndex index = uniqueIndex();
            return{ plant<K,V>( uniqueIndex(), compareKey ), index };
        }
        template< class K, class V, std::enable_if_t<(S<K>),bool> = true >
        Tree<K,V>* plant( TreeIndex index, typename Tree<K,V>::ScalarKeyCompare compareKey = defaultCompareScalar<K> ) {
            static const char* signature = "Tree<K,V>* Forest::plant( TreeIndex, typename Tree<K,V>::ScalarKeyCompare )";
            if (contains( index )) throw std::string( signature ) + " - TreeIndex already in use";
            Tree<K,V>* tree = new Tree<K,V>( pool, compareKey, mode, nullptr );
            trees[ index ] = tree;
            Tree<TreeIndex,PageLink>::insert( index, tree->root->page );
            return tree;
        }
        template< class K, class V, std::enable_if_t<(A<K>),bool> = true >
        Tree<K,V>* plant( TreeIndex index, typename Tree<K,V>::ArrayKeyCompare compareKey = defaultCompareArray<B<K>> ) {
            static const char* signature = "Tree<K,V>* Forest::plant( TreeIndex, typename Tree<K,V>::ArrayKeyCompare )";
            if (contains( index )) throw std::string( signature ) + " - TreeIndex already in use";
            Tree<K,V>* tree = new Tree<K,V>( pool, compareKey, mode, nullptr );
            trees[ index ] = tree;
            Tree<TreeIndex,PageLink>::insert( index, tree->root->page );
            return tree;
        }
      
        template< class K, class V >
        std::pair< Tree<K,V>*, TreeIndex > plant( const Tree<K,V>& source ) {
            TreeIndex index = uniqueIndex();
            return{ plant<K,V>( uniqueIndex(), source ), index };
        }
        template< class K, class V >
        Tree<K,V>* plant( TreeIndex index, const Tree<K,V>& source ) {
            static const char* signature = "std::pair< Tree<K,V>*, TreeIndex > Forest::plant( TreeIndex, const Tree<K,V>& )";
            if (contains( index )) throw std::string( signature ) + " - TreeIndex already in use";
            Tree<K,V>* tree = new Tree<K,V>( pool, source.keyCompare(), mode, nullptr );
            tree->assign( source );
            trees[ index ] = tree;
            Tree<TreeIndex,PageLink>::insert( index, tree->root->page );
            return tree;
        }

        template< class K >
        std::pair< StreamingTree<K>*, TreeIndex > plantStreamingTree() {
            TreeIndex index = uniqueIndex();
            return{ plantStreamingTree<K>(index), index };
        }
        template< class K >
        StreamingTree<K>* plantStreamingTree(TreeIndex index) {
            static const char* signature = "StreamingTree<K>* Forest::plantStreamingTree( TreeIndex )";
            if (contains(index)) throw std::string(signature) + " - TreeIndex already in use";
            StreamingTree<K>* tree = new StreamingTree<K>(pool, mode, nullptr);
            trees[index] = tree;
            Tree<TreeIndex, PageLink>::insert(index, tree->root->page);
            return tree;
        }

        // Access a B-Tree in the Forest via its TreeIndex obtained when the B-Tree was planted.
        // The key and value class template arguments (and optional key compae function) must match
        // those used to plant the B-Tree.
        template< class K, class V, std::enable_if_t<(S<K>),bool> = true >
        Tree<K,V>* access( TreeIndex index, typename Tree<K,V>::ScalarKeyCompare compareKey = defaultCompareScalar<B<K>> ) {
            static const char* signature = "Tree<K,V>* Forest::access( TreeIndex, typename Tree<K,V>::ScalarKeyCompare compareKey )";
            if (0 < trees.count( index )) return reinterpret_cast<Tree<K,V>*>( const_cast<TreeBase*>( trees[ index ] ) );
            if (!contains( index )) throw std::string( signature ) + " - Tree not in Forest";
            PageLink link = retrieve( index );
            PageHeader* rootPage = pool.reference( link );
            Tree<K,V>* tree = new Tree<K,V>( pool, compareKey, mode, rootPage );
            trees[ index ] = tree;
            return( tree );
        }
        template< class K, class V, std::enable_if_t<(A<K>),bool> = true >
        Tree<K,V>* access( TreeIndex index, typename Tree<K,V>::ArrayKeyCompare compareKey = defaultCompareArray<B<K>> ) {
            static const char* signature = "Tree<K,V>* Forest::access( TreeIndex, typename Tree<K,V>::ArrayKeyCompare compareKey )";
            if (0 < trees.count( index )) return reinterpret_cast<Tree<K,V>*>( const_cast<TreeBase*>( trees[ index ] ) );
            if (!contains( index )) throw std::string( signature ) + " - Tree not in Forest";
            PageLink link = retrieve( index );
            PageHeader* rootPage = pool.reference( link );
            Tree<K,V>* tree = new Tree<K,V>( pool, compareKey, mode, rootPage );
            trees[ index ] = tree;
            return( tree );
        }
        template< class K >
        StreamingTree<K>* accessStreamingTree(TreeIndex index) {
            static const char* signature = "StreamingTree<K* Forest::plantStreamingTree( TreeIndex )";
            if (0 < trees.count(index)) return reinterpret_cast<StreamingTree<K>*>(const_cast<TreeBase*>(trees[index]));
            if (!contains(index)) throw std::string(signature) + " - StreamingTree not in Forest";
            PageLink link = retrieve(index);
            PageHeader* rootPage = pool.reference(link);
            StreamingTree<K>* tree = new StreamingTree<K>(pool, mode, rootPage);
            trees[index] = tree;
            return(tree);
        }

        // Commit modifications to all B-Trees in the Forest.
        // The commit is atomic for the entire Forest.
        void commit() {
            if (stats) stats->commits += 1;
            for ( auto tree : trees ) { Tree<TreeIndex,PageLink>::replace( tree.first, tree.second->root->page ); }
            pool.commit( root->page, stats );
        }
        // Recover all B-Trees in the Forest to their last committed state.
        // The recover is atomic for the entire Forest.
        void recover() {
            if (stats) stats->recovers += 1;
            PageLink link( pool.recover() );
            Tree<TreeIndex,PageLink>::recoverTree( link );
            for ( auto tree : trees ) { const_cast<TreeBase*>(tree.second)->recoverTree( Tree<TreeIndex,PageLink>::retrieve( tree.first ) ); }
        }
    private:
        using Tree< TreeIndex, PageLink >::insert;
        using Tree< TreeIndex, PageLink >::replace;
        using Tree< TreeIndex, PageLink >::erase;
        // Generate a unique TreeIndex for a user
        TreeIndex uniqueIndex() { return ((1 << 31) + trees.size()); }
    }; // class Forest

    inline std::ostream & operator<<( std::ostream & o, const Forest& tree ) { tree.stream( o ); return o; }

}; // namespace BTree;

#endif // BTREE_FOREST_H

