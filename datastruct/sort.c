#include "../module_common.h"

#define NUM 10
struct timeval start, end;

void print(int *, int);

/************************************quick sort***************************************/
static int adjust_array(int *arr, int left, int right)
{
    int i = left;
    int j = right;

    int tmp = arr[left];


    while (i < j)
    {
        while (i < j && arr[j] >= tmp)
        {
            --j;
        }

        if (i < j)
        {
            arr[i] = arr[j];
            ++i;
        }


        while (i < j && arr[i] < tmp)
        {
            ++i;
        }

        if (i < j)
        {
            arr[j] = arr[i];
            --j;
        }
    }

    arr[i] = tmp;

    return i;
}


static void _quick_sort(int *arr, int left, int right)
{
    int i;
    if (left < right)
    {
        i = adjust_array(arr, left, right);
        _quick_sort(arr, left, i - 1);
        _quick_sort(arr, i + 1, right);
    }
}


void quick_sort(int *a, int n)
{
    gettimeofcurrent(&start);
    _quick_sort(a, 0, n - 1);
    gettimeofcurrent(&end);
    TIME_VAL_SUB(end, start);
    printf("quick time: %ldms\n", TIME_VAL_MSEC(end));
}


/************************************merge sort***************************************/
static void merge_array(int *a, int m, int *b, int n, int *c)
{
    int i = 0;
    int j = 0;
    int k = 0;

    while (i < m && j < n)
    {
        if (a[i] < b[j])
        {
            c[k++] = a[i++];
        }
        else
        {
            c[k++] = b[j++];
        }
    }

    while (i < m)
    {
        c[k++] = a[i++];
    }

    while (j < n)
    {
        c[k++] = b[j++];
    }
}

void mergearray(int a[], int first, int mid, int last, int temp[])
{
    int i = first, j = mid + 1;
    int m = mid,   n = last;
    int k = 0;

    while (i <= m && j <= n)
    {
        if (a[i] <= a[j])
            temp[k++] = a[i++];
        else
            temp[k++] = a[j++];
    }

    while (i <= m)
        temp[k++] = a[i++];

    while (j <= n)
        temp[k++] = a[j++];

    for (i = 0; i < k; i++)
        a[first + i] = temp[i];
}


static void _merge_sort(int *a, int first, int last, int *res)
{
    if (first < last)
    {
        int mid = (first + last)/2;
        _merge_sort(a, first, mid, res);
        _merge_sort(a, mid + 1, last, res);
        //merge_array(a, mid - first + 1, a + mid + 1, last - mid, res);
        mergearray(a, first, mid, last, res);
        printf("recursion completed\n");
    }
}


int *merge_sort(int *a, int n)
{
    int *res = malloc(sizeof(int) * n);
    gettimeofcurrent(&start);
    _merge_sort(a, 0, n - 1, res);
    gettimeofcurrent(&end);
    TIME_VAL_SUB(end, start);
    printf("merge time: %ldms\n", TIME_VAL_MSEC(end));

    return res;
}



/************************************bubble sort***************************************/
void bubble_sort(int *a, int n)
{
    int i, j;

    gettimeofcurrent(&start);
    for (i = 0; i < n - 1; ++i)
    {
        for (j = 0; j < n - 1 - i; ++j)
        {
            if (a[j + 1] < a[j])
            {
                int tmp = a[j];
                a[j] = a[j + 1];
                a[j + 1] = tmp;
            }
        }
    }
    gettimeofcurrent(&end);
    TIME_VAL_SUB(end, start);
    printf("bubble time: %ldms\n", TIME_VAL_MSEC(end));
}


void print(int *a, int n)
{
    int i;

    for (i = 0; i < n; ++i)
    {
        printf("%d\t", a[i]);
    }

    printf("\n");
}


int main(void)
{
    int i;
    int arr[NUM];

    srand(getpid());
    for (i = 0; i < NUM; ++i)
    {
        arr[i] = 1 + rand() % 100;
    }
    print(arr, NUM);

    // quick sort
    int *p = merge_sort(arr, NUM);

    print(p, NUM);

    return 0;
}
























