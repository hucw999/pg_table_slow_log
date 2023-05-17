MODULES = table_log
DATA = table_log--1.0.sql
EXTENSION = table_log
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
pkglibdir = $(shell $(PG_CONFIG) --pkglibdir)
