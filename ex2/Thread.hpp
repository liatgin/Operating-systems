/**
* Thread
*/
#include "uthreds.h"

#ifndef THREAD_H
#define THREAD_H



/**
* Thread
*/
21 class Thread
22 {
23 public:	/**	* constructor	*/	Thread();	/**	* the id of the new thread	*/	int ID;	/**	* number of quantums in the running time.	*/	int quantums;	private:		sigjmp_buf env[2];	}