extern "C" int printf(const char*, ...);
int main() {
printf( "-ss%u  ", sizeof(short) );
printf( "-si%u  ", sizeof(int) );
printf( "-sl%u  ", sizeof(long) );
printf( "-sll%u  ", sizeof(long long) );
printf( "-sf%u  ", sizeof(float) );
printf( "-sd%u  ", sizeof(double) );
printf( "-sld%u  ", sizeof(long double) );
printf( "-sp%u  ", sizeof(void*) );
printf( "-sw%u  ", sizeof(wchar_t) );
printf( "\n" );
}

