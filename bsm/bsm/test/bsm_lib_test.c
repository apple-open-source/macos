/*
 * Copyright (c) 2004, Apple Computer, Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 * 
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <libbsm.h>

void test_class() 
{
	struct au_class_ent *c;

	setauclass();

	while((c = getauclassent()) != NULL) {
		printf("class = %u\t", c->ac_class);
		printf("name = %s\t", c->ac_name);
		printf("desc = %s\n", c->ac_desc);
		free_au_class_ent(c);
	}

	printf("\n");

	endauclass();

}

void test_control() 
{
	int ret, minval;
	char auditstr[MAX_AUDITSTRING_LEN];
	char dir[100]; /* assume max dir size*/

	setauclass();
	while((ret = getacdir(dir, 100)) >= 0) {
		printf("dir = %s\t", dir); 
		printf("dir ret = %d\t", ret);
	}
	printf("dir ret = %d\n", ret);

	if(0 == (ret = getacmin(&minval))) {
		printf("min free = %d\t", minval);
	} 
	printf("min ret = %d\n", ret);

	if(0 == (ret = getacflg(auditstr, MAX_AUDITSTRING_LEN))) {
		printf("flag = %s\t", auditstr);
	} 
	printf("flag ret = %d\n", ret);

	if(0 == (ret = getacna(auditstr, MAX_AUDITSTRING_LEN))) {
		printf("acna = %s\t", auditstr);
	} 
	printf("acna ret = %d\n", ret);
	printf("\n");

	endac();

}


void test_event() 
{
	struct au_event_ent *e;
	setauevent();

	while((e = getauevent()) != NULL) {
		printf("name = %s\t", e->ae_name);
		printf("desc = %s\t", e->ae_desc);
		printf("event = %d\t", e->ae_number);
		printf("class = %u\n", e->ae_class);
		free_au_event_ent(e);
	}
	printf("\n");

	endauevent();
}

void test_user()
{
	char auditstr[MAX_AUDITSTRING_LEN];
	int ret;
	struct au_user_ent *u;

	setauuser();
	while((u = getauuserent()) != NULL)
 	{
		printf("name = %s\n", u->au_name);

		printf("about to convert into always mask (+) : %d\n", u->au_always.am_success);	
		printf("about to convert into always mask (-) : %d\n", u->au_always.am_failure);	
		if(-1 != (ret = getauditflagschar(auditstr, &(u->au_always), 0))) {
			printf("always mask is: %s\n", auditstr);
		}

		printf("about to convert into never mask (+) : %d\n", u->au_never.am_success);	
		printf("about to convert into never mask (-) : %d\n", u->au_never.am_failure);	
		if(-1 != (ret = getauditflagschar(auditstr, &(u->au_never), 0))) {
			printf("never mask is: %s\n", auditstr);
		}
		printf("\n");
	}

	printf("\n");

	endauuser();
}

void test_helper()
{
	au_mask_t maskp;

	printf("Testing au_preselect() --- XXXX\n");

	printf("Testing au_user_mask\n"); 
	au_user_mask("audit", &maskp);
	printf("success = %u", maskp.am_success);
	printf("failure = %u", maskp.am_failure);

}

int main()
{ 

        printf("Testing Class \n "); test_class();
        printf("Testing control \n "); test_control();
        printf("Testing event \n "); test_event();
        printf("Testing user \n "); test_user(); 
	printf("Testing helper \n "); test_helper();


	return 1;
}

