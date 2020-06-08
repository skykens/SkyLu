//
// Created by jimlu on 2020/6/5.
//

#include "bplustree.h"

Node* BPlusTree::NewNode()
{
    Node *node = (Node *)malloc(sizeof(Node));
    if(nullptr == node) {
        return nullptr;
    }

    for(int & i : node->k) {
        i = 0;
    }

    for(auto & i : node->p) {
        i = nullptr;
    }

    node->num = 0;
    node->isLeaf = true;
    node->prev = nullptr;
    node->next = nullptr;
    return node;
}

Node *BPlusTree::BTreeCreate()
{
    Node *node = NewNode();
    if(nullptr == node) {
        return nullptr;
    }

    node->next = node;
    node->prev = node;

    return node;
}

int BPlusTree::BTreeSplitChild(Node *parent, int pos, Node *child)
{
    Node *new_child = NewNode();
    if(nullptr == new_child) {
        return -1;
    }

    new_child->isLeaf = child->isLeaf;
    new_child->num = M - 1;

    for(int i = 0; i < M - 1; i++) {
        new_child->k[i] = child->k[i+M];
    }

    if(!new_child->isLeaf) {
        for(int i = 0; i < M; i++) {
            new_child->p[i] = child->p[i+M];
        }
    }

    child->num = M - 1;
    if(child->isLeaf) {
        child->num++;  // if leaf, keep the middle ele, put it in the left
    }

    for(int i = parent->num; i > pos; i--) {
        parent->p[i+1] = parent->p[i];
    }
    parent->p[pos+1] = new_child;

    for(int i = parent->num - 1; i >= pos; i--) {
        parent->k[i+1] = parent->k[i];
    }
    parent->k[pos] = child->k[M-1];

    parent->num += 1;

    // update link
    if(child->isLeaf) {
        new_child->next = child->next;
        child->next->prev = new_child;
        new_child->prev = child;
        child->next = new_child;
    }
    return 1;
}

void BPlusTree::BTreeInsertNonfull(Node *node, int target)
{
    if(node->isLeaf) {
        int pos = node->num;
        while(pos >= 1 && target < node->k[pos-1]) {
            node->k[pos] = node->k[pos-1];
            pos--;
        }

        node->k[pos] = target;
        node->num += 1;
        node_num_++;

    } else {
        int pos = node->num;
        while(pos > 0 && target < node->k[pos-1]) {
            pos--;
        }

        if(2 * M -1 == node->p[pos]->num) {
            BTreeSplitChild(node, pos, node->p[pos]);
            if(target > node->k[pos]) {
                pos++;
            }
        }

        BTreeInsertNonfull(node->p[pos], target);
    }
}

Node* BPlusTree::BTreeInsert(Node *root, int target)
{
    if(nullptr == root) {
        return nullptr;
    }

    if(2 * M - 1 == root->num) {
        Node *node = NewNode();
        if(nullptr == node) {
            return root;
        }

        node->isLeaf = false;
        node->p[0] = root;
        BTreeSplitChild(node, 0, root);
        BTreeInsertNonfull(node, target);
        return node;
    } else {
        BTreeInsertNonfull(root, target);
        return root;
    }
}

void BPlusTree::BTreeMergeChild(Node *root, int pos, Node *y, Node *z)
{
    if(y->isLeaf) {
        y->num = 2 * M - 2;
        for(int i = M; i < 2 * M - 1; i++) {
            y->k[i-1] = z->k[i-M];
        }
    } else {
        y->num = 2 * M - 1;
        for(int i = M; i < 2 * M - 1; i++) {
            y->k[i] = z->k[i-M];
        }
        y->k[M-1] = root->k[pos];
        for(int i = M; i < 2 * M; i++) {
            y->p[i] = z->p[i-M];
        }
    }

    for(int j = pos + 1; j < root->num; j++) {
        root->k[j-1] = root->k[j];
        root->p[j] = root->p[j+1];
    }

    root->num -= 1;

    // update link
    if(y->isLeaf) {
        y->next = z->next;
        z->next->prev = y;
    }

    free(z);
}

Node *BPlusTree::BTreeDelete(Node *root, int target)
{
    if(1 == root->num) {
        Node *y = root->p[0];
        Node *z = root->p[1];
        if(nullptr != y && nullptr != z &&
           M - 1 == y->num && M - 1 == z->num) {
            BTreeMergeChild(root, 0, y, z);
            free(root);
            BTreeDeleteNonone(y, target);
            return y;
        } else {
            BTreeDeleteNonone(root, target);
            return root;
        }
    } else {
        BTreeDeleteNonone(root, target);
        return root;
    }
}

void BPlusTree::BTreeDeleteNonone(Node *root, int target)
{
    if(root->isLeaf) {
        int i = 0;
        while(i < root->num && target > root->k[i]) i++;
        if(target == root->k[i]) {
            for(int j = i + 1; j < 2 * M - 1; j++) {
                root->k[j-1] = root->k[j];
            }
            root->num -= 1;
            node_num_--;

        } else {
            printf("target not found\n");
        }
    } else {
        int i = 0;
        Node *y = nullptr, *z = nullptr;
        while(i < root->num && target > root->k[i]) i++;

        y = root->p[i];
        if(i < root->num) {
            z = root->p[i+1];
        }
        Node *p = nullptr;
        if(i > 0) {
            p = root->p[i-1];
        }

        if(y->num == M - 1) {
            if(i > 0 && p->num > M - 1) {
                BTreeShiftToRightChild(root, i-1, p, y);
            } else if(i < root->num && z->num > M - 1) {
                BTreeShiftToLeftChild(root, i, y, z);
            } else if(i > 0) {
                BTreeMergeChild(root, i-1, p, y);
                y = p;
            } else {
                BTreeMergeChild(root, i, y, z);
            }
            BTreeDeleteNonone(y, target);
        } else {
            BTreeDeleteNonone(y, target);
        }
    }
}

int BPlusTree::BTreeSearchPredecessor(Node *root)
{
    Node *y = root;
    while(!y->isLeaf) {
        y = y->p[y->num];
    }
    return y->k[y->num-1];
}

int BPlusTree::BTreeSearchSuccessor(Node *root)
{
    Node *z = root;
    while(!z->isLeaf) {
        z = z->p[0];
    }
    return z->k[0];
}

void BPlusTree::BTreeShiftToRightChild(Node *root, int pos,
                                       Node *y, Node *z)
{
    z->num += 1;

    if(!z->isLeaf) {
        z->k[0] = root->k[pos];
        root->k[pos] = y->k[y->num-1];
    } else {
        z->k[0] = y->k[y->num-1];
        root->k[pos] = y->k[y->num-2];
    }

    for(int i = z->num -1; i > 0; i--) {
        z->k[i] = z->k[i-1];
    }

    if(!z->isLeaf) {
        for(int i = z->num; i > 0; i--) {
            z->p[i] = z->p[i-1];
        }
        z->p[0] = y->p[y->num];
    }

    y->num -= 1;
}

void BPlusTree::BTreeShiftToLeftChild(Node *root, int pos,
                                      Node *y, Node *z)
{
    y->num += 1;

    if(!z->isLeaf) {
        y->k[y->num-1] = root->k[pos];
        root->k[pos] = z->k[0];
    } else {
        y->k[y->num-1] = z->k[0];
        root->k[pos] = z->k[0];
    }

    for(int j = 1; j < z->num; j++) {
        z->k[j-1] = z->k[j];
    }

    if(!z->isLeaf) {
        y->p[y->num] = z->p[0];
        for(int j = 1; j <= z->num; j++) {
            z->p[j-1] = z->p[j];
        }
    }

    z->num -= 1;
}

void BPlusTree::BTreeInorderPrint(Node *root)
{
    if(nullptr != root) {
        BTreeInorderPrint(root->p[0]);
        for(int i = 0; i < root->num; i++) {
            printf("%d ", root->k[i]);
            // 	fwrite(&root,sizeof(root),1,fp);
            BTreeInorderPrint(root->p[i+1]);
        }
    }
}

void BPlusTree::BTreeLinearPrint(Node *root)
{
    if(nullptr != root) {
        Node *leftmost = root;
        while(!leftmost->isLeaf) {
            leftmost = leftmost->p[0];
        }

        Node *iter = leftmost;
        do {
            for(int i = 0; i < iter->num; i++) {
                printf("%d ", iter->k[i]);
                //	fwrite(&root,sizeof(root),1,fp);
            }
            iter = iter->next;
        } while(iter != leftmost);
        printf("\n");
    }
}

void BPlusTree::Save(Node *root)
{
//	fwrite(root,sizeof(root),1,pfile);
}

void BPlusTree::BTreeLevelPrint(Node *root)
{
    // just for simplicty, can't exceed 200 nodes in the tree
    Node *queue[200] = {nullptr};
    int front = 0;
    int rear = 0;

    queue[rear++] = root;

    while(front < rear) {
        Node *node = queue[front++];

        printf("[");
        for(int i = 0; i < node->num; i++) {
            printf("%d ", node->k[i]);
        }
        printf("]");

        for(int i = 0; i <= node->num; i++) {
            if(nullptr != node->p[i]) {
                queue[rear++] = node->p[i];
            }
        }
    }
    printf("\n");
}

void BPlusTree::LinearPrint()
{
    BTreeLinearPrint(root_);
}

BPlusTree::BPlusTree()
{
    // 先判断文件是否存在
    // windows下，是io.h文件，linux下是 unistd.h文件
    // int access(const char *pathname, int mode);
    if(-1==access("define.Bdb",F_OK))
    {
        // 不存在 ,创建
//	   	pfile = fopen("bplus.bp","w");
        root_ = BTreeCreate();
    }
    else
    {
//	   	pfile = fopen("bplus.bp","r+");
        root_ = BTreeCreate();
//	   	fread(root_,sizeof(root_),1,pfile);
    }
}

BPlusTree::~BPlusTree()
= default;

