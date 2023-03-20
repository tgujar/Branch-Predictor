#include "predictor.c"

void test_getLowerNBits()
{
    const int size = 2;
    int vals[size] = {72, 1365};
    int ans[size] = {8, 21};
    int q[size] = {4, 5};
    for (int i = 0; i < size; i++)
    {
        if (ans[i] != getLowerNBits(vals[i], q[i]))
        {
            printf("FAIL: %d lower bits of %d are %d, should be %d\n", q[i], vals[i], getLowerNBits(vals[i], q[i]), ans[i]);
        }
    }
    printf("PASS: test_getLowerNBits()\n");
}

void test_Counter()
{
    Counter c;
    int table_size = getTableSize(10);
    int *arr = calloc(table_size, sizeof(int));
    counter_init(&c, arr, table_size, 3);

    increment(&c, 0);
    increment(&c, 0);
    increment(&c, 0);

    if (c.counts[0] != 3)
    {
        printf("FAIL: Count should be 3, is %d\n", c.counts[0]);
        return;
    }

    increment(&c, 0);
    if (c.counts[0] != 3)
    {
        printf("FAIL: Count should be 3, after increasing 4 times, is %d\n", c.counts[0]);
        return;
    }

    decrement(&c, 0);
    decrement(&c, 0);
    decrement(&c, 0);
    decrement(&c, 0);
    if (c.counts[0] != 0)
        printf("FAIL: Count should be 0");

    printf("PASS: test_Counter()\n");
}

void test_Bimodal()
{
    Counter c;
    int table_size = getTableSize(10);
    int *arr = calloc(table_size, sizeof(int));
    srand(0);
    for (int i = 0; i < table_size; i++)
    {
        arr[i] = rand() % 4;
    }
    BimodalCounter bc;
    bimodalCounter_init(&bc, &c, arr, table_size);
    for (int i = 0; i < 100; i++)
    {
        int idx = rand() % table_size;
        uint8_t o = getOutcome(&bc, idx);
        uint8_t e = c.counts[idx] >= 2;
        if (o != e)
        {
            printf("FAIL: Counts unequal, expected %d, got %d\n", e, o);
        }
    }
    printf("PASS: test_Bimodal()\n");
}

void test_gshare()
{
    Gshare gshare;
    ghistoryBits = 10;
    int table_size = getTableSize(ghistoryBits);
    int *arr = calloc(table_size, sizeof(int));
    bimodalCounter_init(&gshare_bc, &gshare_c, arr, table_size);
    gshare_init(&gshare, &gshare_bc, ghistoryBits);
    if (gshare.ghistoryMask != 1023)
    {
        printf("FAIL: History mask incorrest, expected 1023, got %d \n", gshare.ghistoryMask);
    }

    uint8_t hist[12] = {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1};
    for (int i = 0; i < 12; i++)
    {
        gshare_add_history(&gshare, hist[i] == 1);
    }
    if (gshare.ghistory != 3)
    {
        printf("FAIL: History incorrect, expected 3, got %d \n", gshare.ghistory);
    }

    printf("PASS: test_gshare()\n");
}

int main()
{
    test_getLowerNBits();
    test_Counter();
    test_Bimodal();
    test_gshare();
}