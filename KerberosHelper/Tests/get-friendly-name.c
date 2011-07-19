
#include <Heimdal/krb5.h>
#include <err.h>
#include <stdio.h>

int
main(int argc, char **argv)
{
	krb5_context context;
	krb5_error_code ret;
	krb5_ccache id;
	krb5_data data;
	
	ret = krb5_init_context(&context);
	if (ret)
		errx(1, "krb5_init_context");
	
	ret = krb5_cc_default(context, &id);
	if (ret)
		errx(1, "krb5_cc_default");
	
	ret = krb5_cc_get_config(context, id, NULL, "FriendlyName", &data);
	if (ret)
		errx(1, "krb5_cc_get_config");
	
	printf("FriendlyName: %.*s\n", (int)data.length, (char *)data.data);
	
	return 0;
}
