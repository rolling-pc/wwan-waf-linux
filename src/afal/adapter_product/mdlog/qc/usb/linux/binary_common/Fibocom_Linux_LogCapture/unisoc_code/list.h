#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>

// 节点结构体
struct Node
{
    struct Node *next;
    int logtype;
    int size;
    uint8_t *rbuf;
};

// 头结构体
struct Head
{
    struct Node *first;
    int count;
    pthread_mutex_t mutex;
};

// 初始化头结构体
void initHead(struct Head *head);

// 尾插法插入节点
void insertNode(struct Head *head, struct Node *node);

// 头删法删除节点
void deleteNode(struct Head *head);
