//
// Created by jimlu on 2020/6/5.
//

#ifndef SKYLU_MEMCACHED_BPLUSTREE_H
#define SKYLU_MEMCACHED_BPLUSTREE_H


#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include "tree.h"

class BPlusTree :
        public Tree
{
protected:

    Node* BTreeCreate() override ;
    Node* NewNode() override;
    int BTreeSplitChild(Node *parent, int pos, Node *child)override ;
    void BTreeInsertNonfull(Node *node,int target) override ;
    void BTreeMergeChild(Node *root, int pos, Node *y, Node *z) override ;
    void BTreeDeleteNonone(Node *root, int target) override ;
    int BTreeSearchSuccessor(Node *root) override ;
    int BTreeSearchPredecessor(Node *root) override ;
    void BTreeShiftToLeftChild(Node *root, int pos, Node *y, Node *z) override ;
    void BTreeShiftToRightChild(Node *root, int pos, Node *y, Node *z) override ;
    Node* BTreeInsert(Node *root, int target) override ;
    Node *BTreeDelete(Node *root, int target) override ;
    void BTreeInorderPrint(Node *root) override ;
    void BTreeLevelPrint(Node *root)override ;
    void Save(Node *root)override ;
    /**
     * @brief 按行打印
     *
     * @param
     */
    static void BTreeLinearPrint(Node *root);

public:
    BPlusTree();
    ~BPlusTree() override;
    void LinearPrint();
};



#endif //SKYLU_MEMCACHED_BPLUSTREE_H
