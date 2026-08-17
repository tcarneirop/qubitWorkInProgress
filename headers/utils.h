#ifndef UTILS_H
#define UTILS_H



inline unsigned long long factorial(unsigned long long n)
{
    return (n == 1 || n == 0) ? 1 : factorial(n - 1) * n;
}


void check(const long long m, const long long d, const unsigned long long nsols)
{
    unsigned long long mfat   =   factorial((unsigned long long)m);
    unsigned long long mmdfat =   factorial((unsigned long long)(m - d));

    if (mfat / (mmdfat) != nsols)
    {
        printf("\n############ ERROR - WRONG NUMBER OF SOLS #############\n");
        exit(1);
    }
    else
    {
        printf("\n############ %llu NUM SOLS OK #############\n", nsols);
    }
}

#endif
