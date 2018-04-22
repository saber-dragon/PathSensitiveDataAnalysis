int foo(int a, int x){
    int b, c, d;
    A : {
        a = 0;
    }

    B : {
        b = a + x;
        if (x) goto C;
        else goto D;
    }

    C : {
        a = 1;
    };

    D : {
        a = 2;
    };

    E : {
        if (b == 2) goto F;
        else goto B;
    }

    F : {
        return b;
    }
}
