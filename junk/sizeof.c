#include <stdio.h>

#define S(x) printf(#x " %d\n", sizeof(x));

int main(int argc, char* argv[])
{
S(char);
S(short);
S(int);
S(long);
S(long long);
S(float);
S(double);
S(void*);
S(int*);
return 0;
}
