
\echo Use "CREATE EXTENSION table_log" to load this file. \quit
-- This is a blackhole
CREATE TABLE public.table_log(timestamp timestamp,remote_host varchar, "user" varchar, dbname varchar, duration decimal, message text);

GRANT SELECT, INSERT ON table_log TO PUBLIC;
