/*
 *  ismember_main.c
 *  MembershipAPI
 *
 *  Created by John Anderson on Fri Aug 13 2004.
 *  Copyright (c) 2004 Apple. All rights reserved.
 *
 */

#include <membership.h>
#include "membershipPriv.h"
#import <stdlib.h>
#import <string.h>
#import <stdio.h>

void printUsage(char *errorMessage)
{
	printf("Error: ");
	printf(errorMessage);
	printf("\n");
	printf("usage: ismember_blojsom_helper username -g | -s groupname/servicename\n");
}

int main (int argc, char * const argv[])
{
	char *searchType;
	char *groupOrServiceName;

	if ( argc == 4 )
	{
		searchType = argv[2];
		groupOrServiceName = argv[3];
	}
	else if ( argc == 3 )
	{
		searchType = "-s";
		groupOrServiceName = argv[2];
	}
	else
	{
		printUsage("Wrong number of arguments.");
		return 1;
	}
	
	uuid_t user;
	int result = mbr_user_name_to_uuid(argv[1], user);
	int isMember = 0;
	
	if ( result != 0 )
	{
		printf("Invalid user\n");
		return 1;
	}
	
	if ( strncmp(searchType, "-s", 2) == 0 )
	{
		result = mbr_check_service_membership(user, groupOrServiceName, &isMember);
		
		if ( ( isMember == 1 ) || ( result == 2 ) )
		{
			printf("Member (%i, %i)\n", isMember, result);
			return 0;
		}
		else
		{
			printf("Not a member (%i, %i)\n", isMember, result);
			return 1;
		}
	}
	else if ( strncmp(searchType, "-g", 2) == 0 )
	{
		uuid_t group;
		result = mbr_group_name_to_uuid(groupOrServiceName, group);

		if ( result != 0 )
		{
			printf("Invalid group\n");
			return 1;
		}
		
		result = mbr_check_membership(user, group, &isMember);
		
		if ( ( result == 0 ) && ( isMember == 1 ) )
		{
			printf("Member (%i, %i)\n", isMember, result);
			return 0;
		}
		else
		{
			printf("Not a member (%i, %i)", isMember, result);
			return 1;
		}
	}
	else
	{
		printUsage("Use -g to search for groups, or -s to search for a service.");
		return 1;
	}
}
