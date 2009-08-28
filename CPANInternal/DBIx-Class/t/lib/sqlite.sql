-- 
-- Created by SQL::Translator::Producer::SQLite
-- Created on Tue Aug  8 01:53:20 2006
-- 
BEGIN TRANSACTION;

--
-- Table: employee
--
CREATE TABLE employee (
  employee_id INTEGER PRIMARY KEY NOT NULL,
  position integer NOT NULL,
  group_id integer,
  group_id_2 integer,  
  name varchar(100)
);

--
-- Table: serialized
--
CREATE TABLE serialized (
  id INTEGER PRIMARY KEY NOT NULL,
  serialized text NOT NULL
);

--
-- Table: liner_notes
--
CREATE TABLE liner_notes (
  liner_id INTEGER PRIMARY KEY NOT NULL,
  notes varchar(100) NOT NULL
);

--
-- Table: cd_to_producer
--
CREATE TABLE cd_to_producer (
  cd integer NOT NULL,
  producer integer NOT NULL,
  PRIMARY KEY (cd, producer)
);

--
-- Table: artist
--
CREATE TABLE artist (
  artistid INTEGER PRIMARY KEY NOT NULL,
  name varchar(100)
);

--
-- Table: twokeytreelike
--
CREATE TABLE twokeytreelike (
  id1 integer NOT NULL,
  id2 integer NOT NULL,
  parent1 integer NOT NULL,
  parent2 integer NOT NULL,
  name varchar(100) NOT NULL,
  PRIMARY KEY (id1, id2)
);

--
-- Table: fourkeys_to_twokeys
--
CREATE TABLE fourkeys_to_twokeys (
  f_foo integer NOT NULL,
  f_bar integer NOT NULL,
  f_hello integer NOT NULL,
  f_goodbye integer NOT NULL,
  t_artist integer NOT NULL,
  t_cd integer NOT NULL,
  autopilot character NOT NULL,
  PRIMARY KEY (f_foo, f_bar, f_hello, f_goodbye, t_artist, t_cd)
);

--
-- Table: self_ref_alias
--
CREATE TABLE self_ref_alias (
  self_ref integer NOT NULL,
  alias integer NOT NULL,
  PRIMARY KEY (self_ref, alias)
);

--
-- Table: cd
--
CREATE TABLE cd (
  cdid INTEGER PRIMARY KEY NOT NULL,
  artist integer NOT NULL,
  title varchar(100) NOT NULL,
  year varchar(100) NOT NULL
);

--
-- Table: bookmark
--
CREATE TABLE bookmark (
  id INTEGER PRIMARY KEY NOT NULL,
  link integer NOT NULL
);

--
-- Table: track
--
CREATE TABLE track (
  trackid INTEGER PRIMARY KEY NOT NULL,
  cd integer NOT NULL,
  position integer NOT NULL,
  title varchar(100) NOT NULL,
  last_updated_on datetime NULL
);

--
-- Table: self_ref
--
CREATE TABLE self_ref (
  id INTEGER PRIMARY KEY NOT NULL,
  name varchar(100) NOT NULL
);

--
-- Table: link
--
CREATE TABLE link (
  id INTEGER PRIMARY KEY NOT NULL,
  url varchar(100),
  title varchar(100)
);

--
-- Table: file_columns
--
CREATE TABLE file_columns (
  id INTEGER PRIMARY KEY NOT NULL,
  file varchar(255)
);

--
-- Table: tags
--
CREATE TABLE tags (
  tagid INTEGER PRIMARY KEY NOT NULL,
  cd integer NOT NULL,
  tag varchar(100) NOT NULL
);

--
-- Table: treelike
--
CREATE TABLE treelike (
  id INTEGER PRIMARY KEY NOT NULL,
  parent integer NOT NULL,
  name varchar(100) NOT NULL
);

--
-- Table: event
--
CREATE TABLE event (
  id INTEGER PRIMARY KEY NOT NULL,
  starts_at datetime NOT NULL,
  created_on timestamp NOT NULL
);

--
-- Table: twokeys
--
CREATE TABLE twokeys (
  artist integer NOT NULL,
  cd integer NOT NULL,
  PRIMARY KEY (artist, cd)
);

--
-- Table: noprimarykey
--
CREATE TABLE noprimarykey (
  foo integer NOT NULL,
  bar integer NOT NULL,
  baz integer NOT NULL
);

--
-- Table: fourkeys
--
CREATE TABLE fourkeys (
  foo integer NOT NULL,
  bar integer NOT NULL,
  hello integer NOT NULL,
  goodbye integer NOT NULL,
  sensors character NOT NULL,
  PRIMARY KEY (foo, bar, hello, goodbye)
);

--
-- Table: artist_undirected_map
--
CREATE TABLE artist_undirected_map (
  id1 integer NOT NULL,
  id2 integer NOT NULL,
  PRIMARY KEY (id1, id2)
);

--
-- Table: producer
--
CREATE TABLE producer (
  producerid INTEGER PRIMARY KEY NOT NULL,
  name varchar(100) NOT NULL
);

--
-- Table: onekey
--
CREATE TABLE onekey (
  id INTEGER PRIMARY KEY NOT NULL,
  artist integer NOT NULL,
  cd integer NOT NULL
);

--
-- Table: typed_object
--
CREATE TABLE typed_object (
  objectid INTEGER PRIMARY KEY NOT NULL,
  type VARCHAR(100) NOT NULL,
  value VARCHAR(100)
);

--
-- Table: collection
--
CREATE TABLE collection (
  collectionid INTEGER PRIMARY KEY NOT NULL,
  name VARCHAR(100)
);

--
-- Table: collection_object
--
CREATE TABLE collection_object (
  collection INTEGER NOT NULL,
  object INTEGER NOT NULL
);

--
-- Table: owners
--
CREATE TABLE owners (
  ownerid INTEGER PRIMARY KEY NOT NULL,
  name varchar(100)
);

--
-- Table: books
--
CREATE TABLE books (
  id INTEGER PRIMARY KEY NOT NULL,
  owner INTEGER,
  source varchar(100),
  title varchar(100)
);


CREATE UNIQUE INDEX tktlnameunique_twokeytreelike on twokeytreelike (name);
CREATE UNIQUE INDEX cd_artist_title_cd on cd (artist, title);
CREATE UNIQUE INDEX track_cd_position_track on track (cd, position);
CREATE UNIQUE INDEX track_cd_title_track on track (cd, title);
CREATE UNIQUE INDEX foo_bar_noprimarykey on noprimarykey (foo, bar);
CREATE UNIQUE INDEX prod_name_producer on producer (name);
COMMIT;
