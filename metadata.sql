-- Driver script metadata SQLITE
create table user (
id integer primary key autoincrement,
name text not null,
description text not null,
validation_scheme text not null,
validation_scheme_data text);
--
create table entity (
id integer primary key autoincrement,
name text not null,
description text not null,
abbreviation text not null); /* Multiple inheritance through the role table */
--
create table domain (
id integer primary key autoincrement,
name text not null,
description text not null,
colhead text,
prompt text,
tooltip text);   /* Multiple inheritance may not be necessary but saves code */
--
create table rule (
id integer primary key autoincrement,
name text not null,
description text not null,
ftype text not null, /* From and to; entities, domains, users, roles */
fid integer not null,
ttype text not null,
tid integer not null,
seq integer not null, /* To order collections */
relationship text not null, /* At least, parent, table, permission */
value text);
--
create table role (
id integer primary key autoincrement,
name text not null,
description text not null);
--
create table session (
id integer primary key autoincrement,
starts datetime not null,
ends datetime,
us_id integer not null,
description text not null);
--
create table workflow (
id integer primary key autoincrement,
rl_id integer not null, /* A role */
seq integer not null);
--
create table journal (
id integer primary key autoincrement,
ss_id integer not null,
wf_id integer not null,
wf_seq integer not null,
trans integer not null, /* Transaction ID; the same for all members of the instance */
overarch integer not null, /* Over-arching role */
master integer not null, /* Master role */
detail integer not null, /* Detail role */
subseq integer not null, /* Zero would have column names, 1-n data rows */
value01 text,
value02 text,
value03 text,
value04 text,
value05 text,
value06 text,
value07 text,
value08 text,
value09 text,
value10 text,
value11 text,
value12 text,
value13 text,
value14 text,
value15 text,
value16 text,
value17 text,
value18 text,
value19 text,
value20 text,
value21 text,
value22 text,
value23 text,
value24 text,
value25 text,
value26 text,
value27 text,
value28 text,
value29 text,
value30 text,
value31 text,
value32 text,
value33 text,
value34 text,
value35 text,
value36 text,
value37 text,
value38 text,
value39 text,
value40 text,
value41 text,
value42 text,
value43 text,
value44 text,
value45 text,
value46 text,
value47 text,
value48 text,
value49 text,
value50 text,
value51 text,
value52 text,
value53 text,
value54 text,
value55 text,
value56 text,
value57 text,
value58 text,
value59 text,
value60 text,
value61 text,
value62 text,
value63 text,
value64 text,
value65 text,
value66 text,
value67 text,
value68 text,
value69 text,
value70 text,
value71 text,
value72 text,
value73 text,
value74 text,
value75 text,
value76 text,
value77 text,
value78 text,
value79 text,
value80 text,
value81 text,
value82 text,
value83 text,
value84 text,
value85 text,
value86 text,
value87 text,
value88 text,
value89 text,
value90 text,
value91 text,
value92 text,
value93 text,
value94 text,
value95 text,
value96 text,
value97 text,
value98 text,
value99 text);
--
create table validation (
id integer primary key autoincrement,
name text not null,
description text not null,
dom_id integer not null,
rl_id integer,    /* The context; can be null */
min_value text,
max_value text,
message text );
--
create table task (
id integer primary key autoincrement,
workflow_id integer not null,
session integer not null,
status integer not null,
);
--
