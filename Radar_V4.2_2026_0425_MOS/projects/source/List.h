/**
 *******************************************************************************
 * @file  usart/usart_uart_int/source/ring_buf.h
 * @brief This file contains all the functions prototypes of the ring buffer.
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
#include <stdbool.h>
#include "hc32_ll_def.h"
/*
	CirclesQueue.h
	循环队列
*/
 
//#pragma once//使同一个文件不会被包含(include)多次,不必担心宏名冲突
 
 
//先将可能使用到的头文件写上
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
 
typedef int64_t *LTDataType;//假设结点的数据域类型为 int
//给变量定义一个易于记忆且意义明确的新名字,并且便于以后存储其它类型时方便改动
//（比如我晚点想存double类型的数据，我就直接将 int 改为 double )
 
// 带哨兵位双向循环链表的结构体定义
typedef struct ListNode
{
	struct ListNode* prev;//前驱指针域:存放上一个结点的地址
	struct ListNode* next;//后继指针域:存放下一个结点的地址
	LTDataType data;//数据域
}LTNode;
//struct 关键字和 ListNode 一起构成了这个结构类型
//typedef 为这个结构起了一个别名,叫 LTNode,即：typedef struct ListNode LTNode 
//现在就可以像 int 和 double 那样直接使用 LTNode 来定义变量
 
 
 
// 双向链表的初始化
 
// 如果是单链表直接给个空指针就行，不需要单独写一个函数进行初始化
// 即：LTNode* plist = NULL;
// 那为什么顺序表、带头双向循环链表有呢？
// 因为顺序表、带头双向循环链表的结构并不简单，
// 如:顺序表顺序表为空size要为0,还要看capacity是否要开空间,
//若不开空间capacity=0,指针要给空,若开空间,还要检查malloc是否成功
// 带头双向循环链表要开个结点,检查malloc是否成功,然后让结点自己指向自己
// 顺序表和双向循环链表的初始化有点复杂,最好构建一个函数
LTNode *LTInit(void);
 
// 双向链表在pos位置之前进行插入x
void LTInsert(LTNode* pos, LTDataType x);
 
// 双向链表的打印
void LTPrint(LTNode* phead);
 
// 双向链表删除pos位置的结点
void LTErase(LTNode* pos);
 
//双向链表优于单链表的点——不需要找尾、二级指针
// （我们改的不是结构体的指针，改的是结构体的变量）
// 双向链表的尾插
void LTPushBack(LTNode* phead, LTDataType x);
 
// 双向链表的判空
bool LTEmpty(LTNode* phead);
 
// 双向链表的尾删
void LTPopBack(LTNode* phead);
 
// 双向链表头插
void LTPushFront(LTNode* phead, LTDataType x);
 
// 双向链表头删
void LTPopFront(LTNode* phead);
 
// 双向链表查找值为x的结点
LTNode* LTFind(LTNode* phead, LTDataType x);
 
// 双向链表的销毁
void LTDestory(LTNode* phead);
 
// 双向链表的修改,修改pos位置的值为x
void LTModify(LTNode* pos, LTDataType x);
 
// 双向链表删除值为x的结点
void LTRemove(LTNode* phead, LTDataType x);
 
// 双向链表计算结点总数(不计phead)
int LTTotal(LTNode* phead);
 
// 双向链表获取第i位置的结点
LTNode* LTGet(LTNode* phead, int i);
 
// 双向链表的清空
void LTClear(LTNode* phead);


/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
