//
// Created by jimlu on 2020/6/5.
//

#include "btree.h"

Node* BTree::NewNode()
{
    auto* node = (Node *)malloc(sizeof(Node));
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
    node->isLeaf = true;     //默认为叶子

    return node;
}

Node* BTree::BTreeCreate()
{
    Node *node = NewNode();
    if(nullptr == node) {
        return nullptr;
    }
    return node;
}

int BTree::BTreeSplitChild(Node *parent, int pos, Node *child)
{
    Node *new_child = NewNode();
    if(nullptr == new_child) {
        return -1;
    }
    // 新节点的isLeaf与child相同，key的个数为M-1
    new_child->isLeaf = child->isLeaf;
    new_child->num = M - 1;

    // 将child后半部分的key拷贝给新节点
    for(int i = 0; i < M - 1; i++) {
        new_child->k[i] = child->k[i+M];
    }

    // 如果child不是叶子，还需要把指针拷过去，指针比节点多1
    if(!new_child->isLeaf) {
        for(int i = 0; i < M; i++) {
            new_child->p[i] = child->p[i+M];
        }
    }

    child->num = M - 1;

    // child的中间节点需要插入parent的pos处，更新parent的key和pointer
    for(int i = parent->num; i > pos; i--) {
        parent->p[i+1] = parent->p[i];
    }
    parent->p[pos+1] = new_child;

    for(int i = parent->num - 1; i >= pos; i--) {
        parent->k[i+1] = parent->k[i];
    }
    parent->k[pos] = child->k[M-1];

    parent->num += 1;
    return 0;
}

// 执行该操作时，node->num < 2M-1
void BTree::BTreeInsertNonfull(Node *node, int target)
{
    if(1 == node->isLeaf) {
        // 如果在叶子中找到，直接删除
        int pos = node->num;
        while(pos >= 1 && target < node->k[pos-1]) {
            node->k[pos] = node->k[pos-1];
            pos--;
        }

        node->k[pos] = target;
        node->num += 1;
        node_num_++;

    } else {
        // 沿着查找路径下降
        int pos = node->num;
        while(pos > 0 && target < node->k[pos-1]) {
            pos--;
        }

        if(2 * M -1 == node->p[pos]->num) {
            // 如果路径上有满节点则分裂
            BTreeSplitChild(node, pos, node->p[pos]);
            if(target > node->k[pos]) {
                pos++;
            }
        }

        BTreeInsertNonfull(node->p[pos], target);
    }
}

//插入入口
Node* BTree::BTreeInsert(Node *root, int target)
{
    if(nullptr == root) {
        return nullptr;
    }

    // 对根节点的特殊处理，如果根是满的，唯一使得树增高的情形
    // 先申请一个新的
    if(2 * M - 1 == root->num) {
        Node* node = NewNode();
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

// 将y，root->k[pos], z合并到y节点，并释放z节点，y,z各有M-1个节点
void BTree::BTreeMergeChild(Node *root, int pos, Node *y, Node *z)
{
    // 将z中节点拷贝到y的后半部分
    y->num = 2 * M - 1;
    for(int i = M; i < 2 * M - 1; i++) {
        y->k[i] = z->k[i-M];
    }
    y->k[M-1] = root->k[pos];// k[pos]下降为y的中间节点

    // 如果z非叶子，需要拷贝pointer
    if(!z->isLeaf) {
        for(int i = M; i < 2 * M; i++) {
            y->p[i] = z->p[i-M];
        }
    }

    // k[pos]下降到y中，更新key和pointer
    for(int j = pos + 1; j < root->num; j++) {
        root->k[j-1] = root->k[j];
        root->p[j] = root->p[j+1];
    }

    root->num -= 1;
    ::free(z);
}

/************  删除分析   **************
*
在删除B树节点时，为了避免回溯，当遇到需要合并的节点时就立即执行合并，B树的删除算法如下：从root向叶子节点按照search规律遍历：
（1）  如果target在叶节点x中，则直接从x中删除target，情况（2）和（3）会保证当再叶子节点找到target时，肯定能借节点或合并成功而不会引起父节点的关键字个数少于t-1。
（2）  如果target在分支节点x中：
（a）  如果x的左分支节点y至少包含t个关键字，则找出y的最右的关键字prev，并替换target，并在y中递归删除prev。
（b）  如果x的右分支节点z至少包含t个关键字，则找出z的最左的关键字next，并替换target，并在z中递归删除next。
（c）  否则，如果y和z都只有t-1个关键字，则将targe与z合并到y中，使得y有2t-1个关键字，再从y中递归删除target。
（3）  如果关键字不在分支节点x中，则必然在x的某个分支节点p[i]中，如果p[i]节点只有t-1个关键字。
（a）  如果p[i-1]拥有至少t个关键字，则将x的某个关键字降至p[i]中，将p[i-1]的最大节点上升至x中。
（b）  如果p[i+1]拥有至少t个关键字，则将x个某个关键字降至p[i]中，将p[i+1]的最小关键字上升至x个。
（c）  如果p[i-1]与p[i+1]都拥有t-1个关键字，则将p[i]与其中一个兄弟合并，将x的一个关键字降至合并的节点中，成为中间关键字。
*
*/

// 删除入口
Node* BTree::BTreeDelete(Node* root, int target)
{
    // 特殊处理，当根只有两个子女，切两个子女的关键字个数都为M-1时，合并根与两个子女
    // 这是唯一能降低树高的情形
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

// root至少有个t个关键字，保证不会回溯
void BTree::BTreeDeleteNonone(Node *root, int target)
{
    if(root->isLeaf) {
        // 如果在叶子节点，直接删除
        int i = 0;
        while(i < root->num && target > root->k[i]) i++;
        if(target == root->k[i]) {
            for(int j = i + 1; j < 2 * M - 1; j++) {
                root->k[j-1] = root->k[j];
            }
            root->num -= 1;

            node_num_++;
        } else {
            printf("target not found\n");
        }
    } else {
        int i = 0;
        Node *y = nullptr, *z = nullptr;
        while(i < root->num && target > root->k[i]) i++;
        if(i < root->num && target == root->k[i]) {
            // 如果在分支节点找到target
            y = root->p[i];
            z = root->p[i+1];
            if(y->num > M - 1) {
                // 如果左分支关键字多于M-1，则找到左分支的最右节点prev，替换target
                // 并在左分支中递归删除prev,情况2（a)
                int pre = BTreeSearchPredecessor(y);
                root->k[i] = pre;
                BTreeDeleteNonone(y, pre);
            } else if(z->num > M - 1) {
                // 如果右分支关键字多于M-1，则找到右分支的最左节点next，替换target
                // 并在右分支中递归删除next,情况2(b)
                int next = BTreeSearchSuccessor(z);
                root->k[i] = next;
                BTreeDeleteNonone(z, next);
            } else {
                // 两个分支节点数都为M-1，则合并至y，并在y中递归删除target,情况2(c)
                BTreeMergeChild(root, i, y, z);
                BTreeDelete(y, target);
            }
        } else {
            // 在分支没有找到，肯定在分支的子节点中
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
                    // 左邻接节点关键字个数大于M-1
                    //情况3(a)
                    BTreeShiftToRightChild(root, i-1, p, y);
                } else if(i < root->num && z->num > M - 1) {
                    // 右邻接节点关键字个数大于M-1
                    // 情况3(b)
                    BTreeShiftToLeftChild(root, i, y, z);
                } else if(i > 0) {
                    // 情况3（c)
                    BTreeMergeChild(root, i-1, p, y); // note
                    y = p;
                } else {
                    // 情况3(c)
                    BTreeMergeChild(root, i, y, z);
                }
                BTreeDeleteNonone(y, target);
            } else {
                BTreeDeleteNonone(y, target);
            }
        }

    }
}

//寻找rightmost，以root为根的最大关键字
int BTree::BTreeSearchPredecessor(Node *root)
{
    Node *y = root;
    while(!y->isLeaf) {
        y = y->p[y->num];
    }
    return y->k[y->num-1];
}

// 寻找leftmost，以root为根的最小关键字
int BTree::BTreeSearchSuccessor(Node *root)
{
    Node *z = root;
    while(!z->isLeaf) {
        z = z->p[0];
    }
    return z->k[0];
}

// z向y借节点，将root->k[pos]下降至z，将y的最大关键字上升至root的pos处
void BTree::BTreeShiftToRightChild(Node *root, int pos,
                                   Node *y, Node *z)
{
    z->num += 1;
    for(int i = z->num -1; i > 0; i--) {
        z->k[i] = z->k[i-1];
    }
    z->k[0]= root->k[pos];
    root->k[pos] = y->k[y->num-1];

    if(!z->isLeaf) {
        for(int i = z->num; i > 0; i--) {
            z->p[i] = z->p[i-1];
        }
        z->p[0] = y->p[y->num];
    }

    y->num -= 1;
}

// y向借节点，将root->k[pos]下降至y，将z的最小关键字上升至root的pos处
void BTree::BTreeShiftToLeftChild(Node *root, int pos,
                                  Node *y, Node *z)
{
    y->num += 1;
    y->k[y->num-1] = root->k[pos];
    root->k[pos] = z->k[0];

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

void BTree::BTreeInorderPrint(Node *root)
{
    if(nullptr != root) {
        BTreeInorderPrint(root->p[0]);
        for(int i = 0; i < root->num; i++) {
            printf("%d ", root->k[i]);
            BTreeInorderPrint(root->p[i+1]);
        }
    }
}

void BTree::BTreeLevelPrint(Node *root)
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

void BTree::Save(Node *root)
{
    /*
    storage_struct ss;

    // malloc len space
    ss.len = Node_num;
    ss.snode = (storage_node *)malloc(sizeof(storage_node)*ss.len);

    ss.snode[0].bnode = *root;
    for(int i=1;i<ss.len;i++)
    {
        Node *node = NewNode();
        if(nullptr == node) {
            return;
        }
    }

//	fwrite(&ss,sizeof(ss),1,pfile);
*/
}

BTree::BTree()
{
    // 先判断文件是否存在
    // windows下，是io.h文件，linux下是 unistd.h文件
    // int access(const char *pathname, int mode);
    if(-1==access("define.Bdb",F_OK))
    {
        // 不存在 ,创建
        //   	pfile = fopen("bstree.b","w");
        root_ = BTreeCreate();
    }
    else
    {
//	   	pfile = fopen("bstree.b","r+");
        root_ = BTreeCreate();
//	   	fread(root_,sizeof(root_),1,pfile);
    }

}

BTree::~BTree() = default;



