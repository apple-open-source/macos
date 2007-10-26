class Foo
{
    public:
        typedef struct {
            int value;
        } mystruct;
        
        mystruct   field;
        
        char c;

        Foo(const int f);
        int foo(const bool b, const int i) const;
};

Foo::Foo(const int f)
{
    field.value = f;
    c = 'a';
}

int Foo::foo(const bool b, const int i) const
{
    if (b)
        return i + 1;
    
    return 0;
}

int main (int argc, char * const argv[])
{
    Foo f(3);
    
    f.foo(false, 0);   /* stop here in main */
    f.foo(true, 0);

    return 0;
}
