// { dg-do assemble  }
// GROUPS passed scoping
class Integer {
public:
    int i;
};

class Type {
    // APPLE LOCAL mainline 2006-10-13 3904173
    enum Klasse { ENUM, INTEGER };

    class Description {
    public:
        
    };

    class Integer: public Description {
    public:
        ::Integer low;
        ::Integer high;
    };
};
