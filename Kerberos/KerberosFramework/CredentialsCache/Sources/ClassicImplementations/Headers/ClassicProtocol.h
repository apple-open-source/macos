#pragma once

// Constants for CCache IPC AppleEvents

enum {
	ccClassic_CCacheAEType					= FOUR_CHAR_CODE ('CCae'),

	ccClassic_Key_Message					= FOUR_CHAR_CODE ('Cmsg'),
	ccClassic_Key_MessageID					= FOUR_CHAR_CODE ('Cmid'),					

	ccClassic_YellowServerSignature				= FOUR_CHAR_CODE ('CCae'),
	
	ccClassic_DiffCookie					= FOUR_CHAR_CODE ('diff'),
	ccClassic_ResponseCookie				= FOUR_CHAR_CODE ('resp'),

	ccClassic_Context_FirstMessage				= 0,
	ccClassic_Context_CreateCCache,
	ccClassic_Context_CreateDefaultCCache,
	ccClassic_Context_CreateNewCCache,
	ccClassic_Context_SyncWithYellowCache,
	ccClassic_Context_FabricateInitialDiffs,
	ccClassic_Context_LastMessage,

	ccClassic_CCache_FirstMessage				= 100,
	ccClassic_CCache_Destroy,
	ccClassic_CCache_SetDefault,
	ccClassic_CCache_SetPrincipal,
	ccClassic_CCache_CompatSetPrincipal,
	ccClassic_CCache_StoreConvertedCredentials,
	ccClassic_CCache_CompatStoreConvertedCredentials,
	ccClassic_CCache_RemoveCredentials,
	ccClassic_CCache_Move,
	ccClassic_CCache_SkipToID,
	ccClassic_CCache_LastMessage,

	ccClassic_Credentials_FirstMessage			= 200,
	ccClassic_Credentials_SkipToID,
	ccClassic_Credentials_LastMessage,
	
	ccClassic_Err_YellowServerRestarted			= 5000
};