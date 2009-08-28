#include <sys/types.h>

#include "mdX.h"

void
MDXInit(MDX_CTX *context)
{
	MDX_Init(context);
}

void
MDXUpdate(MDX_CTX *context, const void *data, unsigned int len)
{
	MDX_Update(context, data, len);
}

void
MDXFinal(unsigned char digest[16], MDX_CTX *context)
{
	MDX_Final(digest, context);
}
