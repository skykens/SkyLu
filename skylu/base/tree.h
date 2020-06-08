//
// Created by jimlu on 2020/6/5.
//

#ifndef SKYLU_MEMCACHED_TREE_H
#define SKYLU_MEMCACHED_TREE_H
#include <cstdio>

const static int M = 2;
struct Node{
    int k[2*M-1];  // key
    Node * p[2*M];  //point
    int num;
    bool isLeaf;
    Node * prev;
    Node * next;
};
/**
 * M阶树  作为B树和 B+树的抽象基类
 *
 */
class Tree
{

public:

    Tree()
    {
        node_num_=0;
    };
    virtual ~Tree()
    {
        node_num_=0;
        delete root_;
    };

    /*
    * @param target: 插入
    */
    void insert(int target)
    {
        root_ = BTreeInsert(root_,target);
        Save(root_);  // 即时保存
    };

    /**
     * @brief level print the btree
     */
    void LevelPrint()
    {
        BTreeLevelPrint(root_);
    };

    /**
     * @brief level print the btree
     */
    void Del(int target)
    {
        root_ = BTreeDelete(root_, target);

        Save(root_);  // 即时保存
    };

    void InorderPrint()
    {
        BTreeInorderPrint(root_);
    };

    // tree node num
    int node_num() const
    {
        return node_num_;
    };

protected:
    /**
     * 创建ROOT NODE
     * @return
     */
    virtual Node *BTreeCreate()=0;

    /**
     * new出一个新的NODE，isLeaf = TRUE
     * @return
     */
    virtual Node *NewNode()=0;


    /**
     * @brief 当NUM为2M-1的时候分裂节点 到父节点上
     *
     * @param parent: child of parent
     * @param pos: p[pos] 父节点上挂载 child的位置
     * @param child:
     *
     * @return
     */
    virtual int BTreeSplitChild(Node *parent, int pos, Node *child)=0;

    /**
     * @brief 插入一个key值到node（NUM < 2M-1）
     *
     * @param node:
     * @param target:
     */
    virtual void BTreeInsertNonfull(Node *node, int target)=0;

    /**
     * @brief merge y, z and root->k[pos] to left
     *
     * this appens while y and z both have M-1 keys
     *
     * @param root: parent node
     * @param pos: postion of y
     * @param y: left node to merge
     * @param z: right node to merge
     */
    virtual void BTreeMergeChild(Node *root, int pos, Node *y, Node *z)=0;

    /**
     * @brief 删除一个节点
     * root至少要有M个节点
     *
     * @param root: btree root
     * @param target: target to delete
     *
     * @return
     */
    virtual void BTreeDeleteNonone(Node *root, int target)=0;

    /**
     * @brief 最小key
     *
     * @param root:
     *
     * @return:
     */
    virtual int BTreeSearchSuccessor(Node *root)=0;

    /**
     * @brief 最大key
     *
     * @param root:
     *
     * @return:
     */
    virtual int BTreeSearchPredecessor(Node *root)=0;

    /**
     * @brief 将y的值左移到它的左兄弟上
     *
     * @param root:
     * @param pos: root->k[pos] = y->key
     * @param y: 左节点
     * @param z: 右节点
     */
    virtual void BTreeShiftToLeftChild(Node *root, int pos, Node *y, Node *z)=0;

    /**
     * @brief
     *
     * @param root:
     * @param pos:
     * @param y:
     * @param z:
     */
    virtual void BTreeShiftToRightChild(Node *root, int pos, Node *y, Node *z)=0;

    /**
     * 向root插入一个key值
     * @param root
     * @param target
     * @return  新的root
     */
    virtual Node* BTreeInsert(Node *root, int target)=0;

    /**
     * 删除
     * @param root
     * @param target
     * @return  新的root
     */
    virtual Node *BTreeDelete(Node *root, int target)=0;


    /**
     * @brief 中序遍历 并打印
     *
     * @param root:
     */
    virtual void BTreeInorderPrint(Node *root)=0;


    /**
     * @brief 层序遍历
     *
     * @param root:
     */
    virtual void BTreeLevelPrint(Node *root)=0;

    /**
     * @Save 保存到文件里
     *
     * @param root:
     */
    virtual void Save(Node *root)=0;


protected:
    Node *root_;
    FILE *pfile_;
    int node_num_;  //记录多少个树结点： how many  Node
};

#endif //SKYLU_MEMCACHED_TREE_H
