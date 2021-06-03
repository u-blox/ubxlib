extern "C" int printf(const char*, ...);
int main() {
printf( "-ss%lu  ", sizeof(short) );
printf( "-si%lu  ", sizeof(int) );
printf( "-sl%lu  ", sizeof(long) );
printf( "-sll%lu  ", sizeof(long long) );
printf( "-sf%lu  ", sizeof(float) );
printf( "-sd%lu  ", sizeof(double) );
printf( "-sld%lu  ", sizeof(long double) );
printf( "-sp%lu  ", sizeof(void*) );
printf( "-sw%lu  ", sizeof(wchar_t) );
printf( "\n" );
}

