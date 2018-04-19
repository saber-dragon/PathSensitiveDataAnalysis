int foo(int a){

    int x, y;

    x = a + 10;
    y = x + a;
    x = a + y;

    if (x > 100){
        y = 10;
    } else {
        y = 0;
    }

    return y;
}