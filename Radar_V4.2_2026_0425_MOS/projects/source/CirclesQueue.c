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
#include "CirclesQueue.h"

/*
	CirclesQueue.c
*/

 
/*循环队列初始化*/
int init(CirclesQueue *Q)
{
	Q->front = Q->rear = 0;
	return 0;
}
 
 
/*入队*/
int enqueue(CirclesQueue *Q, DataType x)
{
	if(isfull(Q))
	{
		//printf("队列已满！100001\n");
		return -1;
	}
    __disable_irq();
	Q->rear = (Q->rear+1) % MAXSIZE;
	Q->data[Q->rear] = x;
     __enable_irq();
	return 0;
}
 
/*队满？*/
int isfull(CirclesQueue *Q)
{
	return (Q->rear+1)%MAXSIZE == Q->front ? 1 : 0;
}
 
 
/*出队*/
int dequeue(CirclesQueue *Q, DataType *x)
{
	if(isempty(Q))
	{
		//printf("队列为空！100002\n");
		return -1;
	}
    __disable_irq();
	Q->front = (Q->front+1) % MAXSIZE;
	*x = Q->data[Q->front];
     __enable_irq();
	return 0;
}
 
/*队空*/
int isempty(CirclesQueue *Q)
{
	return (Q->front == Q->rear) ? 1 : 0;
}
 
/*输出队列*/
 
void printcq(CirclesQueue *Q) 
{
	int i;
	if(isempty(Q))
	{
		//printf("队列为空！100002\n");
		return ;
	}
	
	i=(Q->front)%MAXSIZE;
	do{
		//printf("%d  ",Q->data[(i+1 %MAXSIZE)]);
		i=(i+1)%MAXSIZE;
	} while(i!=Q->rear);
    
    return ;
} 
 
/*获取队列中元素个数*/
 
int getSize(CirclesQueue *Q)
{
	return (Q->rear-Q->front+MAXSIZE)%MAXSIZE;
 } 
 
/*获取队首元素*/ 
int getFront(CirclesQueue *Q,DataType *x)
{
	if(isempty(Q))
	{
		//printf("队列为空！100002\n");
		return -1;
	}
	int i;
	i = (Q->front+1) % MAXSIZE;
	*x = Q->data[i];
	return  0;
}
 
/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
