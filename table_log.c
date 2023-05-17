#include <unistd.h>
#include <sys/time.h>

#include "postgres.h"
#include "fmgr.h"
#include "miscadmin.h"

#include "access/xact.h"
#include "access/transam.h"
#include "lib/stringinfo.h"
#include "libpq/libpq.h"
#include "postmaster/bgworker.h"
#include "postmaster/syslogger.h"
#include "storage/proc.h"
#include "tcop/tcopprot.h"
#if PG_VERSION_NUM >= 140000
#include "utils/backend_status.h"
#endif
#include "utils/elog.h"
#include "utils/guc.h"
#include "utils/json.h"
#include "utils/ps_status.h"
#include "executor/spi.h"
#include "postmaster/bgworker.h"

#if PG_VERSION_NUM < 90600
#error Minimum version of PostgreSQL required is 9.6
#endif

/* Allow load of this module in shared libs */
PG_MODULE_MAGIC;

void		_PG_init(void);
void		_PG_fini(void);

/* Hold previous logging hook */
static emit_log_hook_type prev_log_hook = NULL;


static char *table_log_enable = NULL;

/* Log timestamp */
#define FORMATTED_TS_LEN 128
static char formatted_log_time[FORMATTED_TS_LEN];
static pg_tz *utc_tz = NULL;

static void log_table(ErrorData *edata);

static void
setup_formatted_log_time(void)
{
    
	struct timeval tv;
	pg_time_t	stamp_time;
	char		msbuf[8];

	gettimeofday(&tv, NULL);
	stamp_time = (pg_time_t) tv.tv_sec;

	/*
	 * Note: we ignore log_timezone as JSON is meant to be machine-readable so
	 * load arbitrarily UTC. Users can use tools to display the timestamps in
	 * their local time zone. jq in particular can only handle timestamps with
	 * the iso-8601 "Z" suffix representing UTC.
	 *
	 * Note that JSON does not specify the format of dates and timestamps,
	 * however Javascript enforces a somewhat-widely spread format like what
	 * is done in Date's toJSON. The main reasons to do so are that this is
	 * conform to ISO 8601 and that this is rather established.
	 *
	 * Take care to leave room for milliseconds which we paste in.
	 */

	/* Load timezone only once */
	if (!utc_tz)
		utc_tz = pg_tzset("PRC");

	pg_strftime(formatted_log_time, FORMATTED_TS_LEN,
				"%Y-%m-%dT%H:%M:%S %Z",
				pg_localtime(&stamp_time, utc_tz));

	/* 'paste' milliseconds into place... */
	sprintf(msbuf, ".%03d", (int) (tv.tv_usec / 1000));
	memcpy(formatted_log_time + 19, msbuf, 4);
}

/*
 * log_table
 * Write logs to table.
 */
static void
log_table(ErrorData *edata)
{

	/* Timestamp */
	setup_formatted_log_time();
    
    if(strcmp(table_log_enable, "on") == 0 && strstr(edata->message, "duration:") 
        ){
        StartTransactionCommand();
        PushActiveSnapshot(GetTransactionSnapshot());
        int ret = SPI_connect();
        if(ret == SPI_OK_CONNECT){
            
            char query_info[205];
            strncpy(query_info, edata->message, 200);
            float duration;
            sscanf(query_info, "duration: %f ms", &duration);
            char query[1024];
            sprintf(query,"INSERT INTO table_log values('%s','%s','%s','%s','%f','%s')", formatted_log_time,
                MyProcPort->remote_host, MyProcPort->user_name, MyProcPort->database_name, duration, query_info);
            // printf("query=%s\n", query);
            PG_TRY();
            {
                int ret2 = SPI_exec(query, 0);
            }
            PG_FINALLY();
            {
                SPI_finish();
            }
            PG_END_TRY();
            
            
        }
        PopActiveSnapshot();
        CommitTransactionCommand();
    }
    

	/* Continue chain to previous hook */
	if (prev_log_hook)
		(*prev_log_hook) (edata);

    
}

/*
 * _PG_init
 * Entry point loading hooks
 */
void
_PG_init(void)
{
    DefineCustomStringVariable("table_log.enable",
							   "whther log to table",
							   "Default value is \"on\".",
							   &table_log_enable,
							   "off",
							   PGC_SIGHUP,
							   0, NULL, NULL, NULL);

    printf("pg_init\n");

	prev_log_hook = emit_log_hook;
	emit_log_hook = log_table;
    
}

/*
 * _PG_fini
 * Exit point unloading hooks
 */
void
_PG_fini(void)
{
    printf("pg_fini\n");
	emit_log_hook = prev_log_hook;
}
