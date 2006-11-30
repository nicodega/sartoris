/*
*
*	Copyright (C) 2002, 2003, 2004, 2005
*       
*	Santiago Bazerque 	sbazerque@gmail.com			
*	Nicolas de Galarreta	nicodega@gmail.com
*
*	
*	Redistribution and use in source and binary forms, with or without 
* 	modification, are permitted provided that conditions specified on 
*	the License file, located at the root project directory are met.
*
*	You should have received a copy of the License along with the code,
*	if not, it can be downloaded from our project site: sartoris.sourceforge.net,
*	or you can contact us directly at the email addresses provided above.
*
*
*/

#include <lib/iolib.h>

/* We have a class, and global variables */
class MyAdd
{
protected:
	int v;
public:
	MyAdd()
	{
		v = 0;
	}
	MyAdd(int n)
	{
		v = n;
	}
	virtual void Method(int t)
	{
		v += t;
	}
	int Result()
	{
		return v;
	}
};

class MyMult : public MyAdd
{
public:
	MyMult() : MyAdd(){}
	MyMult(int n) : MyAdd(n){}
	void Method(int t)
	{
		v *= t;
	}
};

MyAdd add(1);

int main(int argc, char **argv) 
{
	MyAdd *add2 = new MyAdd();
	MyAdd *add3 = new MyAdd(5);
	MyMult *mul = new MyMult();
	MyMult *mul2 = new MyMult(6);

	delete add2;
	delete add3;
	delete mul;
	delete mul2;

	return 0;
} 


	
