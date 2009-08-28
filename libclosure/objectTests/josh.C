// CONFIG C++ GC RR open rdar://6347910



struct MyStruct {
    int something;
};

struct TestObject {

        void test(void){
            {
                MyStruct first;   // works
            }
            void (^b)(void) = ^{ 
                MyStruct inner;  // fails to compile!
            };
        }
};

    

int main(char *argc, char *argv[]) {
    printf("%s: Success\n", argv[0]);
    return 0;
}
