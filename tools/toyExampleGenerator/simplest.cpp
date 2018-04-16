#include <iostream>
using namespace std;
void foo(int a){
    int x, y;

    if (a){
        x = 1;
    } else {
        x = 2;
    }

    int w;
    cin >> w;
    if (w > a){
        cout << x << endl;
    } else {
        cout << w << endl;
    }

    y = x;

    if (y > 1){
        cout << "You Win" << endl;
    }else {
        cout << "You lose" << endl;
    }


    cout << "Game over" << endl;


}
