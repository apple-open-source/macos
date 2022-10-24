// @header
// iOS data migrator for keychain items.
// It's a simple bundle loaded by DataMigrator at runtime.
// It calls a securityd SPI to perform the migration.

#pragma once

#import <Foundation/Foundation.h>
#import <DataMigration/DataMigration.h>

@interface KeychainMigrator : DataClassMigrator
@end
