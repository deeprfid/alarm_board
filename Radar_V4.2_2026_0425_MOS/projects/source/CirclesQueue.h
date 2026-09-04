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
 
#define MAXSIZE 512
 
typedef uint8_t DataType;
 
typedef struct
{
	DataType data[MAXSIZE];
	int front;
	int rear;
}CirclesQueue;
 
/*循环队列初始化*/
int init(CirclesQueue *Q);
 
/*入队*/
int enqueue(CirclesQueue *Q, DataType x);
 
/*队满？*/
int isfull(CirclesQueue *Q);
 
/*出队*/
int dequeue(CirclesQueue *Q, DataType *);
 
/*输出队列*/
void printcq(CirclesQueue *Q) ;
 
/*队空*/
int isempty(CirclesQueue *Q);
 
/*获取队列大小*/
int getSize(CirclesQueue *Q);
 
 
/*获取队首元素*/ 
int getFront(CirclesQueue *Q,DataType *x);
 



/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
