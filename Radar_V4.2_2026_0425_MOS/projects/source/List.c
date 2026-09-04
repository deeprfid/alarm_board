/**
 *******************************************************************************
 * @file  usart/usart_uart_int/source/ring_buf.c
 * @brief This file provides firmware functions to manage the ring buffer.
 @verbatim
   Change Logs:
   Date             Author          Notes
   2022-03-31       CDT             First version
 @endverbatim
 *******************************************************************************
 * Copyright (C) 2022-2023, Xiaohua Semiconductor Co., Ltd. All rights reserved.
 *
 * This software component is licensed by XHSC under BSD 3-Clause license
 * (the "License"); You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                    opensource.org/licenses/BSD-3-Clause
 *
 *******************************************************************************
 */

/*******************************************************************************
 * Include files
 ******************************************************************************/
#include <string.h>
#include "hc32_ll_utility.h"
#include "List.h"
 
//动态申请一个结点
LTNode* BuyListNode(LTDataType x)
{
	LTNode* node = (LTNode*)malloc(sizeof(LTNode));
	if (node == NULL)//如果malloc失败
	{
		perror("malloc fail");
		return NULL;
	}
	//如果malloc成功
	//初始化一下，防止野指针，如果看到返回的是空指针，那逻辑可能有些错误
	node->next = NULL;
	node->prev = NULL;
	node->data = x;
 
	return node;
}
 
// 双向链表的初始化
LTNode* LTInit(void)
{
	LTNode* phead = BuyListNode((LTDataType)-1);
	//自己指向自己
	phead->next = phead;
	phead->prev = phead;
 
	return phead;
}
 
// 双向链表在pos位置之前进行插入x
void LTInsert(LTNode* pos, LTDataType x)
{
	//assert(pos);//pos肯定不为空
 
	LTNode* prev = pos->prev;
	LTNode* newnode = BuyListNode(x);//创建一个需要插入的结点
 
	prev->next = newnode;
	newnode->prev = prev;
 
	newnode->next = pos;
	pos->prev = newnode;
}
 
// 双向链表的打印
void LTPrint(LTNode* phead)
{
	//assert(phead);//有哨兵位
	//printf("<=>phead<=>");
	LTNode* cur = phead->next;//cur指向第一个要打印的结点
	while (cur != phead)//cur等于头结点时打印就结束了
	{
		//printf("%d<=>", cur->data);
		cur = cur->next;
	}
	//printf("\n");
}
 
// 双向链表删除pos位置的结点
void LTErase(LTNode* pos)
{
	//assert(pos);//pos肯定不为空
 
	LTNode* posprev = pos->prev;
	LTNode* posnext = pos->next;
 
	posprev->next = posnext;
	posnext->prev = posprev;
 
	free(pos);
	pos = NULL;
	//这个置空其实已经没有意义了，形参的改变不会改变实参
}
 
// 双向链表的尾插
void LTPushBack(LTNode* phead, LTDataType x)
{
	//assert(phead);//有哨兵位
 
	//法一:（便于新手更好地理解双向链表的尾插）
	//一步就可完成链表为空/不为空的尾插
	//LTNode* newnode = BuyListNode(x);
	//LTNode* tail = phead->prev;
 
	//tail->next = newnode;
	//newnode->prev = tail;
	//newnode->next = phead;
	//phead->prev = newnode;
 
	//法二:函数复用（简单方便）
	LTInsert(phead, x);
}
 
// 双向链表的判空
bool LTEmpty(LTNode* phead)
{
	//assert(phead);
 
	return phead->next == phead;
	//两者相等就是空链表(返回真),两者不相等就不是空链表(返回假)
}
 
// 双向链表的尾删
void LTPopBack(LTNode* phead)
{
	//assert(phead);//有哨兵位
 
	//法一:（便于新手更好地理解双向链表的尾删）
	////assert(!LTEmpty(phead));//判空
 
	//LTNode* tail = phead->prev;
	//LTNode* tailPrev = tail->prev;
 
	//tailPrev->next = phead;
	//phead->prev = tailPrev;
	//free(tail);
	//tail = NULL;
 
	//法二:函数复用
	LTErase(phead->prev);
}
 
// 双向链表头插
void LTPushFront(LTNode* phead, LTDataType x)
{
	//assert(phead);//有哨兵位
 
	//LTNode* newnode = BuyListNode(x);//创建一个要插入的结点
 
	//法一:只用phead和newnode两个指针（便于新手更好地理解双向链表的头插）
	//newnode->next = phead->next;
	//phead->next->prev = newnode;
 
	//phead->next = newnode;
	//newnode->prev = phead;
 
	//法二:多用了first先记住第一个结点（便于新手更好地理解双向链表的头插）
	//LTNode* first = phead->next;
	//phead->next = newnode;
	//newnode->prev = phead;
 
	//newnode->next = first;
	//first->prev = newnode;
 
	//法三:函数复用(简单方便)
	LTInsert(phead->next, x);
}
 
// 双向链表头删
void LTPopFront(LTNode* phead)
{
	//assert(phead);//有哨兵位
	//assert(!LTEmpty(phead));//判空
 
	//法一:（便于新手更好地理解双向链表的头删）
	//LTNode* head = phead->next;
	//LTNode* headnext = head->next;
 
	//phead->next = headnext;
	//headnext->prev = phead;
 
	//free(head);
	//head = NULL;
 
	//法二:函数复用（简单方便）
	LTErase(phead->next);
}
 
// 双向链表查找值为x的结点
LTNode* LTFind(LTNode* phead, LTDataType x)
{
	//assert(phead);//有哨兵位
 
	LTNode* cur = phead->next;
	while (cur != phead)//让cur去遍历
	{
		if (cur->data == x)
		{
			return cur;
		}
		cur = cur->next;
	}
	return NULL;
}
 
// 双向链表的销毁
void LTDestory(LTNode* phead)
{
	//assert(phead);
 
	LTNode* cur = phead->next;
	while (cur != phead)
	{
		LTNode* curnext = cur->next;
		free(cur);
		cur = curnext;
	}
	free(phead);
	phead = NULL;
	//这个置空其实已经没有意义了，形参的改变不会改变实参
	//我们为了保持接口的一致性,不传二级指针,选择在测试的时候置空
}
 
// 双向链表的修改,修改pos位置的值为x
void LTModify(LTNode* pos, LTDataType x)
{
	//assert(pos);//pos肯定不为空
	pos->data = x;
}
 
// 双向链表删除值为x的结点
void LTRemove(LTNode* phead, LTDataType x)
{
	//assert(phead);//有哨兵位
	LTNode* pos = phead->next;
	while (pos != phead)
	{
		pos = LTFind(phead, x);
		if (pos == NULL)//如果遍历完
		{
			return ;
		}
		LTErase(pos);
		pos = pos->next;
	}
}
 
// 双向链表计算结点总数(不计phead)
int LTTotal(LTNode* phead)
{
	//assert(phead);//有哨兵位
 
	int count = 0;//count来计数
	LTNode* cur = phead->next;//让cur去遍历
	while (cur != phead)
	{
		count++;
		cur = cur->next;
	}
	return count;
}
 
// 双向链表获取第i位置的结点
LTNode* LTGet(LTNode* phead, int i)
{
	//assert(phead);//有哨兵位
 
	int length = LTTotal(phead);
	LTNode* cur = phead->next;
	if (i == 0)
	{
		return phead;
	}
	else if (i<0 || i>length)//位置不合法
	{
		return NULL;
	}
	else if (i <= (length / 2))//从表头开始遍历
	{
		cur = phead->next;
		for (int count = 1; count < i; count++)
		{
			cur = cur->next;
		}
	}
	else//从表尾开始遍历
	{
		cur = phead->prev;
		for (int count = 1; count <= length - i; count++)
		{
			cur = cur->prev;
		}
	}
	return cur;
}
 
// 双向链表的清空
void LTClear(LTNode* phead)
{
	//assert(phead);//有哨兵位
 
	while (!LTEmpty(phead))//如果不为空就一直头删
	{
		LTPopFront(phead);
	}
}

/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
