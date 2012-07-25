#ifndef __OVERUNDERFLOWCHECK__
#define __OVERUNDERFLOWCHECK__

inline uint32 CheckUInt32Add(uint32 a, uint32 b)
{
	uint32 c = a + b;
	if (c < a)	
	{
		CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);
	}
	
	return c;
}



inline uint32 CheckUInt32Subtract(uint32 a, uint32 b)
{
	if (a < b)
	{
		CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);
	}

	return a - b;
}



inline uint32 CheckUInt32Multiply(uint32 a, uint32 b)
{
	uint32 c = a * b;
	uint64 cc = ((uint64) a) * ((uint64) b);
	if (c != cc)
	{
		CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);
	}
	
	return c;
}



inline uint64 Check64BitAdd(uint64 a, uint64 b)
{
	uint64 c = a + b;
	if (c < a)
	{
		CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);
	}
	
	return c;
}



inline uint64 Check64BitSubtract(uint64 a, uint64 b)
{
	if (a < b)
	{
		CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);
	}

	return a - b;
}


	
inline uint64 Check64BitMultiply(uint64 a, uint64 b)
{
	if (a != 0)
	{
		uint64 max = (uint64) -1;
		uint64 limit = max / a;
		if (b > limit)
		{
			CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);
		}
	}
	
	return a * b;
}



#endif
