int foo(int a, int b){
    int x;
    int y = 2;

    if (a > 10){
        x = 1;
    } else if (a > 5){
        x = 3;
    } else if (a > 0){
        x = 100;
    } else {
        x = a;
    }

    D: {
        if (x == 8){
            y = a;
        } else if (x == 9){
            y = b;
        } else {
            y = a + b;
        }
    }

    y = x;

    if (y > a){
        y = b;
    }else {
        y = a;
    }


    return (x + y);
}
