-- 
-- Created by SQL::Translator::Producer::SQLite
-- Created on Sat Jan 30 19:18:55 2010
-- 
;

--
-- Table: artist
--
CREATE TABLE artist (
  artistid INTEGER PRIMARY KEY NOT NULL,
  name varchar(100),
  rank integer NOT NULL DEFAULT '13',
  charfield char(10)
);

CREATE INDEX artist_name_hookidx ON artist (name);

--
-- Table: bindtype_test
--
CREATE TABLE bindtype_test (
  id INTEGER PRIMARY KEY NOT NULL,
  bytea blob,
  blob blob,
  clob clob
);

--
-- Table: collection
--
CREATE TABLE collection (
  collectionid INTEGER PRIMARY KEY NOT NULL,
  name varchar(100) NOT NULL
);

--
-- Table: employee
--
CREATE TABLE employee (
  employee_id INTEGER PRIMARY KEY NOT NULL,
  position integer NOT NULL,
  group_id integer,
  group_id_2 integer,
  group_id_3 integer,
  name varchar(100)
);

--
-- Table: encoded
--
CREATE TABLE encoded (
  id INTEGER PRIMARY KEY NOT NULL,
  encoded varchar(100)
);

--
-- Table: event
--
CREATE TABLE event (
  id INTEGER PRIMARY KEY NOT NULL,
  starts_at datetime NOT NULL,
  created_on timestamp NOT NULL,
  varchar_date varchar(20),
  varchar_datetime varchar(20),
  skip_inflation datetime,
  ts_without_tz datetime
);

--
-- Table: file_columns
--
CREATE TABLE file_columns (
  id INTEGER PRIMARY KEY NOT NULL,
  file varchar(255) NOT NULL
);

--
-- Table: fourkeys
--
CREATE TABLE fourkeys (
  foo integer NOT NULL,
  bar integer NOT NULL,
  hello integer NOT NULL,
  goodbye integer NOT NULL,
  sensors character(10) NOT NULL,
  read_count integer,
  PRIMARY KEY (foo, bar, hello, goodbye)
);

--
-- Table: genre
--
CREATE TABLE genre (
  genreid INTEGER PRIMARY KEY NOT NULL,
  name varchar(100) NOT NULL
);

CREATE UNIQUE INDEX genre_name ON genre (name);

--
-- Table: link
--
CREATE TABLE link (
  id INTEGER PRIMARY KEY NOT NULL,
  url varchar(100),
  title varchar(100)
);

--
-- Table: money_test
--
CREATE TABLE money_test (
  id INTEGER PRIMARY KEY NOT NULL,
  amount money
);

--
-- Table: noprimarykey
--
CREATE TABLE noprimarykey (
  foo integer NOT NULL,
  bar integer NOT NULL,
  baz integer NOT NULL
);

CREATE UNIQUE INDEX foo_bar ON noprimarykey (foo, bar);

--
-- Table: onekey
--
CREATE TABLE onekey (
  id INTEGER PRIMARY KEY NOT NULL,
  artist integer NOT NULL,
  cd integer NOT NULL
);

--
-- Table: owners
--
CREATE TABLE owners (
  id INTEGER PRIMARY KEY NOT NULL,
  name varchar(100) NOT NULL
);

--
-- Table: producer
--
CREATE TABLE producer (
  producerid INTEGER PRIMARY KEY NOT NULL,
  name varchar(100) NOT NULL
);

CREATE UNIQUE INDEX prod_name ON producer (name);

--
-- Table: self_ref
--
CREATE TABLE self_ref (
  id INTEGER PRIMARY KEY NOT NULL,
  name varchar(100) NOT NULL
);

--
-- Table: sequence_test
--
CREATE TABLE sequence_test (
  pkid1 integer NOT NULL,
  pkid2 integer NOT NULL,
  nonpkid integer NOT NULL,
  name varchar(100),
  PRIMARY KEY (pkid1, pkid2)
);

--
-- Table: serialized
--
CREATE TABLE serialized (
  id INTEGER PRIMARY KEY NOT NULL,
  serialized text NOT NULL
);

--
-- Table: treelike
--
CREATE TABLE treelike (
  id INTEGER PRIMARY KEY NOT NULL,
  parent integer,
  name varchar(100) NOT NULL
);

CREATE INDEX treelike_idx_parent ON treelike (parent);

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

CREATE INDEX twokeytreelike_idx_parent1_parent2 ON twokeytreelike (parent1, parent2);

CREATE UNIQUE INDEX tktlnameunique ON twokeytreelike (name);

--
-- Table: typed_object
--
CREATE TABLE typed_object (
  objectid INTEGER PRIMARY KEY NOT NULL,
  type varchar(100) NOT NULL,
  value varchar(100) NOT NULL
);

--
-- Table: artist_undirected_map
--
CREATE TABLE artist_undirected_map (
  id1 integer NOT NULL,
  id2 integer NOT NULL,
  PRIMARY KEY (id1, id2)
);

CREATE INDEX artist_undirected_map_idx_id1 ON artist_undirected_map (id1);

CREATE INDEX artist_undirected_map_idx_id2 ON artist_undirected_map (id2);

--
-- Table: bookmark
--
CREATE TABLE bookmark (
  id INTEGER PRIMARY KEY NOT NULL,
  link integer
);

CREATE INDEX bookmark_idx_link ON bookmark (link);

--
-- Table: books
--
CREATE TABLE books (
  id INTEGER PRIMARY KEY NOT NULL,
  source varchar(100) NOT NULL,
  owner integer NOT NULL,
  title varchar(100) NOT NULL,
  price integer
);

CREATE INDEX books_idx_owner ON books (owner);

--
-- Table: forceforeign
--
CREATE TABLE forceforeign (
  artist INTEGER PRIMARY KEY NOT NULL,
  cd integer NOT NULL
);

--
-- Table: self_ref_alias
--
CREATE TABLE self_ref_alias (
  self_ref integer NOT NULL,
  alias integer NOT NULL,
  PRIMARY KEY (self_ref, alias)
);

CREATE INDEX self_ref_alias_idx_alias ON self_ref_alias (alias);

CREATE INDEX self_ref_alias_idx_self_ref ON self_ref_alias (self_ref);

--
-- Table: track
--
CREATE TABLE track (
  trackid INTEGER PRIMARY KEY NOT NULL,
  cd integer NOT NULL,
  position int NOT NULL,
  title varchar(100) NOT NULL,
  last_updated_on datetime,
  last_updated_at datetime,
  small_dt smalldatetime
);

CREATE INDEX track_idx_cd ON track (cd);

CREATE UNIQUE INDEX track_cd_position ON track (cd, position);

CREATE UNIQUE INDEX track_cd_title ON track (cd, title);

--
-- Table: cd
--
CREATE TABLE cd (
  cdid INTEGER PRIMARY KEY NOT NULL,
  artist integer NOT NULL,
  title varchar(100) NOT NULL,
  year varchar(100) NOT NULL,
  genreid integer,
  single_track integer
);

CREATE INDEX cd_idx_artist ON cd (artist);

CREATE INDEX cd_idx_genreid ON cd (genreid);

CREATE INDEX cd_idx_single_track ON cd (single_track);

CREATE UNIQUE INDEX cd_artist_title ON cd (artist, title);

--
-- Table: collection_object
--
CREATE TABLE collection_object (
  collection integer NOT NULL,
  object integer NOT NULL,
  PRIMARY KEY (collection, object)
);

CREATE INDEX collection_object_idx_collection ON collection_object (collection);

CREATE INDEX collection_object_idx_object ON collection_object (object);

--
-- Table: lyrics
--
CREATE TABLE lyrics (
  lyric_id INTEGER PRIMARY KEY NOT NULL,
  track_id integer NOT NULL
);

CREATE INDEX lyrics_idx_track_id ON lyrics (track_id);

--
-- Table: cd_artwork
--
CREATE TABLE cd_artwork (
  cd_id INTEGER PRIMARY KEY NOT NULL
);

--
-- Table: liner_notes
--
CREATE TABLE liner_notes (
  liner_id INTEGER PRIMARY KEY NOT NULL,
  notes varchar(100) NOT NULL
);

--
-- Table: lyric_versions
--
CREATE TABLE lyric_versions (
  id INTEGER PRIMARY KEY NOT NULL,
  lyric_id integer NOT NULL,
  text varchar(100) NOT NULL
);

CREATE INDEX lyric_versions_idx_lyric_id ON lyric_versions (lyric_id);

--
-- Table: tags
--
CREATE TABLE tags (
  tagid INTEGER PRIMARY KEY NOT NULL,
  cd integer NOT NULL,
  tag varchar(100) NOT NULL
);

CREATE INDEX tags_idx_cd ON tags (cd);

--
-- Table: cd_to_producer
--
CREATE TABLE cd_to_producer (
  cd integer NOT NULL,
  producer integer NOT NULL,
  attribute integer,
  PRIMARY KEY (cd, producer)
);

CREATE INDEX cd_to_producer_idx_cd ON cd_to_producer (cd);

CREATE INDEX cd_to_producer_idx_producer ON cd_to_producer (producer);

--
-- Table: images
--
CREATE TABLE images (
  id INTEGER PRIMARY KEY NOT NULL,
  artwork_id integer NOT NULL,
  name varchar(100) NOT NULL,
  data blob
);

CREATE INDEX images_idx_artwork_id ON images (artwork_id);

--
-- Table: twokeys
--
CREATE TABLE twokeys (
  artist integer NOT NULL,
  cd integer NOT NULL,
  PRIMARY KEY (artist, cd)
);

CREATE INDEX twokeys_idx_artist ON twokeys (artist);

--
-- Table: artwork_to_artist
--
CREATE TABLE artwork_to_artist (
  artwork_cd_id integer NOT NULL,
  artist_id integer NOT NULL,
  PRIMARY KEY (artwork_cd_id, artist_id)
);

CREATE INDEX artwork_to_artist_idx_artist_id ON artwork_to_artist (artist_id);

CREATE INDEX artwork_to_artist_idx_artwork_cd_id ON artwork_to_artist (artwork_cd_id);

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
  pilot_sequence integer,
  PRIMARY KEY (f_foo, f_bar, f_hello, f_goodbye, t_artist, t_cd)
);

CREATE INDEX fourkeys_to_twokeys_idx_f_foo_f_bar_f_hello_f_goodbye ON fourkeys_to_twokeys (f_foo, f_bar, f_hello, f_goodbye);

CREATE INDEX fourkeys_to_twokeys_idx_t_artist_t_cd ON fourkeys_to_twokeys (t_artist, t_cd);

--
-- View: year2000cds
--
CREATE VIEW year2000cds AS
    SELECT cdid, artist, title, year, genreid, single_track FROM cd WHERE year = "2000"