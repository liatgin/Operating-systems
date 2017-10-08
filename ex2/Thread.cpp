/**
* Thread class
*/
#include "Thread.hpp"
using namespace std;

Thread::Thread(address_t sp, address_t pc, int id):
	sp(sp), pc(pc), ID(id), quantums(0)
{
	env[0] = sp;
	env[1] = pc;
}