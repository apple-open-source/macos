--
-- Copyright (c) 2011 Apple Inc. All Rights Reserved.
-- 
-- @APPLE_LICENSE_HEADER_START@
-- 
-- This file contains Original Code and/or Modifications of Original Code
-- as defined in and that are subject to the Apple Public Source License
-- Version 2.0 (the 'License'). You may not use this file except in
-- compliance with the License. Please obtain a copy of the License at
-- http://www.opensource.apple.com/apsl/ and read it before using this
-- file.
--
-- The Original Code and all software distributed under the License are
-- distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
-- EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
-- INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
-- FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
-- Please see the License for the specific language governing rights and
-- limitations under the License.
-- 
-- @APPLE_LICENSE_HEADER_END@
--
--
-- System Policy master database - file format and initial contents
--
-- This is currently for sqlite3
--
PRAGMA foreign_keys = true;



--
-- The primary authority. This table is conceptually scanned
-- in priority order, with the highest-priority matching record
-- determining the outcome.
-- 
CREATE TABLE authority (
	id INTEGER PRIMARY KEY,
	type INTEGER NOT NULL,
	requirement TEXT NOT NULL,
	allow INTEGER NOT NULL,
	expires INTEGER NULL,
	priority REAL NOT NULL DEFAULT (0),
	label TEXT NULL,
	inhibit_cache INTEGER NULL,
	flags INTEGER NOT NULL DEFAULT (0),
	-- following fields are for documentation only
	remarks TEXT NULL
);

-- any Apple-signed installers of any kind
insert into authority (type, allow, priority, label, requirement)
	values (2, 1, -1, 'Apple Installer', 'anchor apple generic');

-- Apple code signing
insert into authority (type, allow, label, requirement)
	values (1, 1, 'Apple', 'anchor apple');

-- Mac App Store signing
insert into authority (type, allow, label, requirement)
	values (1, 1, 'Mac App Store', 'anchor apple generic and certificate leaf[field.1.2.840.113635.100.6.1.9] exists');

insert into authority (type, allow, label, requirement)
	values (1, 1, 'Developer Seed', 'anchor apple generic and certificate 1[field.1.2.840.113635.100.6.2.6] exists and certificate leaf[field.1.2.840.113635.100.6.1.13] exists');
insert into authority (type, allow, label, requirement)
	values (2, 1, 'Developer Seed', 'anchor apple generic and certificate 1[field.1.2.840.113635.100.6.2.6] exists and certificate leaf[field.1.2.840.113635.100.6.1.14] exists');


--
-- The cache table lists previously determined outcomes
-- for individual objects (by object hash). Entries come from
-- full evaluations of authority records, or by explicitly inserting
-- override rules that preempt the normal authority.
--
CREATE TABLE object (
	id INTEGER PRIMARY KEY,
	type INTEGER NOT NULL,
	hash CDHASH NOT NULL UNIQUE,
	allow INTEGER NOT NULL,
	expires INTEGER NULL,
	label TEXT NULL,
	authority INTEGER NULL REFERENCES authority(id),
	-- following fields are for documentation only
	path TEXT NULL,
	created INTEGER NOT NULL default (strftime('%s','now')),
	remarks TEXT NULL
);
