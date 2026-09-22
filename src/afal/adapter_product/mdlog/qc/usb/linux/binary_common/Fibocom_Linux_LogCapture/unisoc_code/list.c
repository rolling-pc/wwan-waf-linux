#include <pthread.h>
#include "list.h"

// 初始化头结构体
void initHead(struct Head *head)
{
    head->first = NULL;
    head->count = 0;
    pthread_mutex_init(&head->mutex, NULL);
}

// 尾插法插入节点
void insertNode(struct Head *head, struct Node *node)
{
    pthread_mutex_lock(&head->mutex);
    if (head->first == NULL)
    { // 链表为空
        head->first = node;
    }
    else
    {
        struct Node *p = head->first;
        while (p->next != NULL)
        {
            p = p->next;
        }
        p->next = node;
    }
    head->count++;
    pthread_mutex_unlock(&head->mutex);
}

// 头删法删除节点
void deleteNode(struct Head *head)
{
    pthread_mutex_lock(&head->mutex);
    if (head->first != NULL)
    { // 链表非空
        struct Node *p = head->first;
        head->first = p->next;
        free(p->rbuf);
        free(p);
        head->count--;
    }
    pthread_mutex_unlock(&head->mutex);
}

#if 0
int main()
{
    struct Head head;
    initHead(&head);

    for (int i = 0; i < 5; i++)
    {
        struct Node *node = (struct Node *)malloc(sizeof(struct Node));
        node->logtype = i;
        node->size = i + 1;
        node->rbuf = (uint8_t *)malloc(node->size * sizeof(uint8_t));
        for (int j = 0; j < node->size; j++)
        {
            node->rbuf[j] = i + j;
        }
        node->next = NULL;
        insertNode(&head, node);
    }

    printf("链表节点数：%d\n", head.count);
    struct Node *p = head.first;
    while (p != NULL)
    {
        printf("logtype: %d, size: %d, rbuf: ", p->logtype, p->size);
        for (int i = 0; i < p->size; i++)
        {
            printf("%d ", p->rbuf[i]);
        }
        printf("\n");
        p = p->next;
    }

    deleteNode(&head);
    deleteNode(&head);

    printf("链表节点数：%d\n", head.count);
    p = head.first;
    while (p != NULL)
    {
        printf("logtype: %d, size: %d, rbuf: ", p->logtype, p->size);
        for (int i = 0; i < p->size; i++)
        {
            printf("%d ", p->rbuf[i]);
        }
        printf("\n");
        p = p->next;
    }

    // 释放动态分配的内存
    p = head.first;
    while (p != NULL)
    {
        struct Node *tmp = p;
        p = p->next;
        free(tmp->rbuf);
        free(tmp);
    }

    return 0;
}
#endif