
BSDPClientRef
BSDPClientCreateWithInterface(BSDPClientStatus * status_p,
			      const char * ifname);

BSDPClientRef
BSDPClientCreateWithInterfaceAndAttributes(BSDPClientStatus * status_p,
					   const char * ifname,
					   const u_int16_t * attrs,
					   int n_attrs);

