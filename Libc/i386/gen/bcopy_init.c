/* Null function.  This actually does something on ppc, and this is needed
 * for the i386 version to link.
 */
int _cpu_capabilities;

void _bcopy_initialize()
{
	return;
}
