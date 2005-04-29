// APPLE LOCAL testcase for 3639865
// { dg-options "-O2" }

class TLine
{
public:
        ~TLine();
};

class TTypesetter
{
public:
        TTypesetter( const TTypesetter& other );
        virtual ~TTypesetter() { return; };
private:
        TLine fMaster;
};

class TTypesetterAttrString : public TTypesetter
{
public:
        TTypesetterAttrString( int string );
        ~TTypesetterAttrString() { return; };
};

class CFTLine
{

public:
        ~CFTLine() { ; return; };
};

typedef const struct __CTLine * CTLineRef;

CTLineRef CTLineCreateWithAttributedString(
        int string )
{
        TTypesetterAttrString setter( string );
        CFTLine* newLine = new CFTLine();
        return (CTLineRef) newLine;
}
