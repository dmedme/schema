--
-- This is a SQLITE3-compatible schema for a TNF
-- representation of the schema underlying PATH, that
-- Ruby-On-Rails can use to auto-generate maintenance
-- code. It follows the Ruby-On-Rails conventions for
-- primary key names, concurrency management columns
-- etc., so that all the auto-generatable features are
-- fully engaged.
--
-- Stuff that comes from fdvars
--
drop table environmentals;
create table environmentals (
id integer primary key autoincrement,
variable varchar(255) not null,
value varchar(255) not null,
created_on timestamp(14) not null,
updated_on timestamp(14) not null,
lock_version integer not null,
description varchar(255),
constraint environmental_uk unique (variable)
);
--
-- Known Operating Systems
--
drop table oses;
create table oses (
id integer primary key autoincrement,
os varchar(255) not null,
selfex varchar(255) not null,
setup varchar(255) not null,
created_on timestamp(14) not null,
updated_on timestamp(14) not null,
lock_version integer not null,
description varchar(255),
constraint os_uk unique (os)
);
--
-- Operating System Packaged Components
--
drop table os_components;
create table os_components (
id integer primary key autoincrement,
os_id integer not null,
os varchar(255) not null,
path varchar(255) not null,
created_on timestamp(14) not null,
updated_on timestamp(14) not null,
lock_version integer not null,
description varchar(255),
constraint os_component_uk unique (os, path),
constraint os_component_os foreign key (os_id) references oses(id)
);
--
-- Known Hosts
--
drop table hosts;
create table hosts (
id integer primary key autoincrement,
os_id integer not null,
host varchar(255) not null,
port numeric not null,
os varchar(255) not null,
relative_capability numeric not null,
status varchar(255) not null,
created_on timestamp(14) not null,
updated_on timestamp(14) not null,
lock_version integer not null,
constraint host_uk unique (host, port),
constraint host_os foreign key (os_id) references oses(id)
);
--
-- Scripts
--
drop table scripts;
create table scripts (
id integer primary key autoincrement,
script varchar(255) not null,
created_on timestamp(14) not null,
updated_on timestamp(14) not null,
lock_version integer not null,
description varchar(255),
constraint script_uk unique (script)
);
--
-- Events
--
drop table events;
create table events (
id integer primary key autoincrement,
script_id integer not null,
script varchar(255) not null,
event_abbrev varchar(3) not null,
description varchar(255) not null,
created_on timestamp(14) not null,
updated_on timestamp(14) not null,
lock_version integer not null,
constraint event_pk unique (script,event_abbrev),
constraint event_script foreign key (script_id) references scripts(id)
);
--
-- Datasets
--
drop table datasets;
create table datasets (
id integer primary key autoincrement,
dataset varchar(255) not null,
reusable varchar(2) not null,
refresh_command varchar(255) not null,
created_on timestamp(14) not null,
updated_on timestamp(14) not null,
lock_version integer not null,
original_cnt bigint,
current_cnt bigint,
description varchar(255),
constraint dataset_uk unique (dataset)
);
--
-- Fields
--
drop table fields;
create table fields (
id integer primary key autoincrement,
dataset_id integer not null,
dataset varchar(255) not null,
field varchar(255) not null,
created_on timestamp(14) not null,
updated_on timestamp(14) not null,
lock_version integer not null,
description varchar(255),
constraint field_uk unique (dataset,field),
constraint field_dataset foreign key (dataset_id) references datasets(id)
);
--
-- Substitutions
--
drop table substitutions;
create table substitutions (
id integer primary key autoincrement,
script_id integer not null,
dataset_id integer not null,
field_id integer not null,
script varchar(255) not null,
dataset varchar(255) not null,
field varchar(255) not null,
to_match varchar(255) not null,
created_on timestamp(14) not null,
updated_on timestamp(14) not null,
lock_version integer not null,
constraint substitution_uk unique (script,dataset,field),
constraint substitution_dataset
    foreign key (dataset_id) references datasets(id),
constraint substitution_script
    foreign key (script_id) references scripts(id),
constraint substitution_field
    foreign key (field_id) references fields(id)
);
--
-- Edits
--
drop table edits;
create table edits (
id integer primary key autoincrement,
substitution_id integer not null,
line bigint not null,
created_on timestamp(14) not null,
updated_on timestamp(14) not null,
lock_version integer not null,
constraint edit_substitution
    foreign key (substitution_id) references substitutions(id)
);
--
-- Scenarios
--
drop table scenarios;
create table scenarios (
id integer primary key autoincrement,
scenario varchar(255) not null,
created_on timestamp(14) not null,
updated_on timestamp(14) not null,
lock_version integer not null,
default_stagger numeric,
description varchar(255),
constraint scenario_uk unique (scenario)
);
--
-- Tranches
--
drop table tranches;
create table tranches (
id integer primary key autoincrement,
parent_scenario_id integer not null,
child_scenario_id integer not null,
parent varchar(255) not null,
child varchar(255) not null,
duration numeric not null,
created_on timestamp(14) not null,
updated_on timestamp(14) not null,
lock_version integer not null,
constraint tranche_parent foreign key (parent_scenario_id)
            references scenarios(id),
constraint tranche_child foreign key (child_scenario_id)
            references scenarios(id)
);
--
-- Bundles
--
drop table bundles;
create table bundles (
id integer primary key autoincrement,
scenario_id integer not null,
script_id integer not null,
scenario varchar(255) not null,
script varchar(255) not null,
user_cnt bigint not null,
tran_cnt bigint not null,
think_time numeric not null,
stagger numeric not null,
created_on timestamp(14) not null,
updated_on timestamp(14) not null,
lock_version integer not null,
actor bigint,
typing_rate numeric,
extra_args varchar(255),
extra_env varchar(255),
constraint bundle_scenario
         foreign key (scenario_id) references scenarios (id),
constraint bundle_script
         foreign key (script_id) references scripts(id)
);
--
-- Scenario Executions
-- 
drop table executions;
create table executions (
id integer primary key autoincrement,
scenario_id integer not null,
script_id integer not null,
scenario varchar(255) not null,
default_share_clone varchar(2) not null,
default_include_executables varchar(2) not null,
created_on timestamp(14) not null,
updated_on timestamp(14) not null,
lock_version integer not null,
description varchar(255),
constraint execution_scenario
    foreign key (scenario_id) references scenarios (id)
);
--
-- Scenario Execution Hosts
-- 
drop table execution_hosts;
create table execution_hosts (
id integer primary key autoincrement,
execution_id integer not null,
host_id integer not null,
scenario varchar(255) not null,
host varchar(255) not null,
share_clone numeric not null,
include_executables varchar(2) not null,
created_on timestamp(14) not null,
updated_on timestamp(14) not null,
lock_version integer not null,
constraint execution_parent
    foreign key (execution_id) references executions (id),
constraint execution_host
    foreign key (host_id) references hosts (id)
);
--
-- Runouts; per host tranche bundles
-- 
drop table runout_lines;
create table runout_lines (
id integer primary key autoincrement,
execution_scenario varchar(255) not null,
execution_host_id integer not null,
host_id integer not null,
host varchar(255) not null,
tranche_id integer not null,
script_id integer not null,
bundle_scenario varchar(255) not null,
bundle_id integer not null,
script varchar(255) not null,
user_cnt bigint not null,
tran_cnt bigint not null,
think_time numeric not null,
stagger numeric not null,
created_on timestamp(14) not null,
updated_on timestamp(14) not null,
lock_version integer not null,
actor bigint,
typing_rate numeric,
extra_args varchar(255),
extra_env varchar(255),
constraint runout_line_uk unique (execution_host_id,
       tranche_id, bundle_id),
constraint runout_line_execution_host
    foreign key (execution_host_id) references
           execution_hosts (id),
constraint runout_line_tranche
    foreign key (tranche_id)
             references tranches (id),
constraint runout_line_bundle
    foreign key (bundle_id)
             references bundles (id),
constraint runout_line_script
    foreign key (script_id)
             references scripts (id)
);
--
-- Runs; actual invocation of the sub-systems
--
drop table runs;
create table runs (
id integer primary key autoincrement,
execution_id integer not null,
scenario varchar(255) not null,
start_time datetime not null,
end_time datetime,
description varchar(255),
created_on timestamp(14) not null,
updated_on timestamp(14) not null,
lock_version integer not null,
constraint run_parent
    foreign key (execution_id) references executions (id)
);
--
-- Results; general bucket that records everything in one place.
--
drop table results;
create table results (
id integer primary key autoincrement,
run_id integer not null,
timestamp datetime not null,
created_on timestamp(14) not null,
updated_on timestamp(14) not null,
lock_version integer not null,
elapsed numeric,
host_id bigint,
host varchar(255),
tranche_id bigint,
bundle_scenario varchar(255),
bundle_id bigint,
rope_id bigint,
event_id bigint,
script varchar(255),
event_abbrev varchar(255),
category varchar(255),
value numeric,
value1 numeric,
value2 numeric,
value3 numeric,
value4 numeric,
value5 numeric,
value6 numeric,
value7 numeric,
value8 numeric,
value9 numeric,
value10 numeric,
value11 numeric,
value12 numeric,
value13 numeric,
value14 numeric,
value15 numeric,
value16 numeric,
value17 numeric,
value18 numeric,
value19 numeric,
value20 numeric,
constraint result_run
    foreign key (run_id) references runs (id)
);
