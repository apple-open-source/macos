#include <stdio.h>
#include <stdint.h>

class test1abcdefg
{
public:
	test1abcdefg();
	virtual ~test1abcdefg();
        test1abcdefg(const test1abcdefg &rhs);
	
	bool empty() const;
	size_t size() const;
	uint32_t m_uint;
protected:

private:
};

