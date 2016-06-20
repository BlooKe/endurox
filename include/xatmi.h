/*
 * This file as per:
 *   The Open Group, Technical Standard
 *   Distributed Transaction Processing: The XATMI Specification
 * !!! Extended for Enduro/X !!!
 */
#ifndef __XATMI_H__
#define __XATMI_H__

#if defined(__cplusplus)
extern "C" {
#endif

/*---------------------------Includes-----------------------------------*/
#include <ndrx_config.h>
#include <stdint.h>
#include <ubf.h>
/*---------------------------Macros-------------------------------------*/
    
/*
 * flag bits for C language xatmi routines
 */
#define TPNOBLOCK	0x00000001
#define TPSIGRSTRT	0x00000002
#define TPNOREPLY	0x00000004
#define TPNOTRAN	0x00000008
#define TPTRAN		0x00000010
#define TPNOTIME	0x00000020
#define TPABSOLUTE	0x00000040
#define TPGETANY	0x00000080
#define TPNOCHANGE	0x00000100
#define TPCONV		0x00000400
#define TPSENDONLY	0x00000800
#define TPRECVONLY	0x00001000
#define TPACK		0x00002000
#define TPTRANSUSPEND	0x00040000	/* Suspend current transaction */

#define TPEVSERVICE	0x00000001
#define TPEVPERSIST	0x00000008

#define NDRX_XID_SERIAL_BUFSIZE     48 /* Serialized size (base64) xid */
#define NDRX_MAX_RMS                32  /* Number of resource managers supported */
#define TMTXFLAGS_IS_ABORT_ONLY     0x0001 /* transaction is marked as abort only */

#define ATMI_XA_TX_INFO_FIELDS      \
    short tmtxflags;                   /* See TMTXFLAGS_* */\
    char tmxid[NDRX_XID_SERIAL_BUFSIZE+1]; /* tmxid, serialized */\
    short tmrmid; /* initial resource manager id */\
    short tmnodeid; /* initial node id */\
    short tmsrvid; /* initial TM server id */\
    char tmknownrms[NDRX_MAX_RMS+1]; /* valid values 1..32-1, 0 - reserved + EOS */
/*
 * values for rval in tpreturn
 */
#define TPFAIL		0x0001
#define TPSUCCESS	0x0002
    
/*
 * Posix Queue processing path prefixes
 */
#define NDRX_FMT_SEP      ','                   /* Seperator in qnames */
#define NDRX_FMT_SEP_STR  ","                   /* Seperator in qnames */
#define NDRX_NDRXD        "%s,sys,bg,ndrxd"
#define NDRX_NDRXCLT      "%s,sys,bg,xadmin,%d"
#define NDRX_NDRXCLT_PFX  "%s,sys,bg,xadmin," /* Prefix for sanity check */


#define NDRX_SVC_QFMT     "%s,svc,%s"            /* Q format in epoll mode (one q multiple servers) */
#define NDRX_SVC_QFMT_SRVID "%s,svc,%s,%hd"       /* Q format in poll mode (use server id) */
#define NDRX_ADMIN_FMT    "%s,srv,admin,%s,%d,%d"

#define NDRX_SYS_SVC_PFX        "@" /* Prefix used for system services */
#define NDRX_SVC_BRIDGE_STATLEN   9              /* Static len of bridge name */
#define NDRX_SVC_BRIDGE   "@TPBRIDGE%03d"        /* Bridge service format */
#define NDRX_SVC_QBRDIGE  "%s,svc,@TPBRIDGE%03d" /* Bridge service Q format */

#define NDRX_SVC_RM       "@TM-%d"              /* resource_id */
#define NDRX_SVC_TM       "@TM-%d-%d"           /* Node_idresource_id */
#define NDRX_SVC_TM_I     "@TM-%d-%d-%d"        /* Node_id,resource_id,instance/srvid */
    
#define NDRX_SVC_TMQ       "@TMQ-%ld-%d"        /* Node_id,srvid */
/* QSPACE service format */
#define NDRX_SVC_QSPACE    "@QSP%s"            /* Q space format string (for service) */
    
#define NDRX_SVC_CPM      "@CPMSVC"             /* Client Process Monitor svc */

#define NDRX_ADMIN_FMT_PFX "%s,srv,admin," /* Prefix for sanity check. */

#define NDRX_SVR_QREPLY   "%s,srv,reply,%s,%d,%d"
#define NDRX_SVR_QREPLY_PFX "%s,srv,reply," /* Prefix for sanity check. */

/* this may end up in "112233-" if client is not properly initialized */
#define NDRX_CLT_QREPLY   "%s,clt,reply,%s,%d,%ld"
#define NDRX_CLT_QREPLY_PFX   "%s,clt,reply," /* Prefix for sanity check */
#define NDRX_CLT_QREPLY_CHK   ",clt,reply," /* (verify that it is reply q) */

#define NDRX_ADMIN_SVC     "%s-%d"

/* This queue basically links two process IDs for conversation */
#define NDRX_CONV_INITATOR_Q  "%s,cnv,c,%s,%d" /* Conversation initiator */
#define NDRX_CONV_SRV_Q       "%s,cnv,s,%s,%d,%s" /* Conversation server Q */

#define NDRX_MY_ID_SRV        "srv,%s,%d,%d,%d" /* binary name, server id, pid, nodeid */
#define NDRX_MY_ID_SRV_PARSE  "srv %s %d %d %d" /* binary name, server id, pid, nodeid for parse */
#define NDRX_MY_ID_CLT        "clt,%s,%d,%ld,%d"

/* Shared memory formats */
#define NDRX_SHM_SRVINFO        "%s,shm,srvinfo"        /* Server info SHM  */
#define NDRX_SHM_SVCINFO        "%s,shm,svcinfo"        /* Service info SHM */
#define NDRX_SHM_BRINFO         "%s,shm,brinfo"         /* Bridge info SHM */
#define NDRX_SEM_SVCOP          "%s,sem,svcop"          /* Service operations... */
#define NDRX_KEY_FMT            "-k %s"                 /* format string for process key */
/*
 * globals
 */
#define ATMI_MSG_MAX_SIZE   65536
#define NDRX_MAX_Q_SIZE     128
#define NDRX_MAX_ID_SIZE    96      /* pfx + binary name + server id + pid + nodeid */
#define NDRX_MAX_KEY_SIZE   128     /* Key size for random key                  */
#define NDRX_QDIAG_MSG_SIZE 256     /* Q diagnostic message size               */
/* List of configuration environment variables */
#define CONF_NDRX_TOUT           "NDRX_TOUT"
#define CONF_NDRX_ULOG           "NDRX_ULOG"
#define CONF_NDRX_QPREFIX        "NDRX_QPREFIX"
#define CONF_NDRX_SVCMAX         "NDRX_SVCMAX"
#define CONF_NDRX_SRVMAX         "NDRX_SRVMAX"
#define CONF_NDRX_CONFIG         "NDRX_CONFIG"
#define CONF_NDRX_QPATH          "NDRX_QPATH"
#define CONF_NDRX_SHMPATH        "NDRX_SHMPATH"
#define CONF_NDRX_CMDWAIT        "NDRX_CMDWAIT"     /* Command wait time */
#define CONF_NDRX_DPID           "NDRX_DPID"        /* PID file for backend-process */
#define CONF_NDRX_DMNLOG         "NDRX_DMNLOG"       /* Log file for backend process */
#define CONF_NDRX_LOG            "NDRX_LOG"        /* Log file for command line utilty */
#define CONF_NDRX_RNDK           "NDRX_RNDK"       /* Random key */
#define NDRX_MSGMAX              "NDRX_MSGMAX"     /* Posix queues, max msgs */
#define NDRX_MSGSIZEMAX          "NDRX_MSGSIZEMAX" /* Maximum size of message for posixq */
#define CONF_NDRX_SANITY         "NDRX_SANITY" /* Time in seconds after which do sanity check for dead processes */
#define CONF_NDRX_QPATH          "NDRX_QPATH" /* Path to place on fs where queues lives */
#define CONF_NDRX_IPCKEY         "NDRX_IPCKEY" /* IPC Key for shared memory */
#define CONF_NDRX_DQMAX          "NDRX_DQMAX" /* Internal NDRXD Q len (max msgs) */
#define CONF_NDRX_NODEID         "NDRX_NODEID"  /* Cluster Node Id */
#define CONF_NDRX_LDBAL          "NDRX_LDBAL"  /* Load balance, 0 = run locally, 100 = run on cluster */
#define CONF_NDRX_CLUSTERISED    "NDRX_CLUSTERISED" /* Do we run in clusterized environment? */
/* thise we can override with env_over: */
#define CONF_NDRX_XA_RES_ID      "NDRX_XA_RES_ID"   /* XA Resource ID           */
#define CONF_NDRX_XA_OPEN_STR    "NDRX_XA_OPEN_STR" /* XA Open string           */
#define CONF_NDRX_XA_CLOSE_STR   "NDRX_XA_CLOSE_STR"/* XA Close string          */
#define CONF_NDRX_XA_DRIVERLIB   "NDRX_XA_DRIVERLIB"/* Enduro RM Library        */
#define CONF_NDRX_XA_RMLIB       "NDRX_XA_RMLIB"    /* RM library               */
#define CONF_NDRX_XA_LAZY_INIT   "NDRX_XA_LAZY_INIT"/* 0 - load libs on 
                                                      init, 1 - load at use     */
#define CONF_NDRX_NRSEMS         "NDRX_NRSEMS"      /* Number of semaphores used for
                                                       service shared memory (for poll() mode)*/
#define CONF_NDRX_NRSEMS_DFLT           30          /* default number of semphoares  */
#define CONF_NDRX_MAXSVCSRVS     "NDRX_MAXSVCSRVS"  /* Max servers per service (only for poll() mode) */
#define CONF_NDRX_MAXSVCSRVS_DFLT       30          /* default for NDRX_MAXSVCSRVS param */
                                                      
#define tperrno	(*_exget_tperrno_addr())
#define tpurcode (*_exget_tpurcode_addr())

/*
 * error values for tperrno
 */
#define TPMINVAL	0	/* no error, min - fix*/ 
#define TPEABORT	1
#define TPEBADDESC	2
#define TPEBLOCK	3
#define TPEINVAL	4
#define TPELIMIT	5
#define TPENOENT	6
#define TPEOS		7
#define TPEPERM		8
#define TPEPROTO	9
#define TPESVCERR	10
#define TPESVCFAIL	11
#define TPESYSTEM	12
#define TPETIME		13
#define TPETRAN		14
#define TPGOTSIG	15
#define TPERMERR	16
#define TPEITYPE	17
#define TPEOTYPE	18
#define TPERELEASE	19
#define TPEHAZARD	20
#define TPEHEURISTIC	21
#define TPEEVENT	22
#define	TPEMATCH	23
#define TPEDIAGNOSTIC	24
#define TPEMIB		25    
#define TPERFU26        26 /* reserved for future use */
#define TPERFU27	27 /* reserved for future use */
#define TPERFU28	28 /* reserved for future use */
#define TPERFU29	29 /* reserved for future use */
#define TPINITFAIL	30
#define TPMAXVAL	31	/* max error */

/*
 * events returned during conversational communication
 */
#define TPEV_DISCONIMM	0x0001
#define TPEV_SVCERR	0x0002
#define TPEV_SVCFAIL	0x0004
#define TPEV_SVCSUCC	0x0008
#define TPEV_SENDONLY	0x0020

/*
 * X/Open defined typed buffers
 */
#define X_OCTET		"X_OCTET"
#define X_C_TYPE	"X_C_TYPE"
#define X_COMMON	"X_COMMON"

#define MAXTIDENT                   30		/* Internal identifed max len */
#define XATMI_SERVICE_NAME_LENGTH   MAXTIDENT
    
/* Range for cluster ID's
 * Currently we allow 254 nodes.
 */
#define CONF_NDRX_NODEID_MIN     1              /* Min Node ID                */
#define CONF_NDRX_NODEID_MAX     33             /* Max Node ID                */
/* Total count of cluster nodes */
#define CONF_NDRX_NODEID_COUNT (CONF_NDRX_NODEID_MAX-CONF_NDRX_NODEID_MIN)
#define CONF_NDRX_MAX_SRVIDS_XADMIN         512    /* max number of server IDs for 
                                                    xadmin printing in poll mode */
    
/* Max calls at the same tame
 * This is  used by Call Descritptor check 
 */
#define MAX_ASYNC_CALLS      16384

/* Bigger connection count may slowdown whole system. */
#define MAX_CONNECTIONS      5
    
/* EnduroX system ATMI flags */
#define SYS_FLAG_REPLY_ERROR    0x00000001
#define SYS_CONVERSATION        0x00000002 /* We have open conversation */
#define SYS_SRV_THREAD          0x00000004 /* This is new server thread */
/* buffer management flags: */
#define SYS_SRV_CVT_JSON2UBF    0x00000008 /* Message is converted from JSON to UBF */
#define SYS_SRV_CVT_UBF2JSON    0x00000010 /* Message is converted from UBF to JSON */

/* Test is any flag set */
#define SYS_SRV_CVT_ANY_SET(X) (X & SYS_SRV_CVT_JSON2UBF || X & SYS_SRV_CVT_UBF2JSON)
    
#define tpadvertise(_SVCNM, _FNADDR) tpadvertise_full(_SVCNM, _FNADDR, #_FNADDR)

#define NDRX_INTEGRA(X)		__##X##__ /* integration mode*/

/********************** Queue suport  *****************************************/
#define TMQNAMELEN	15
#define TMMSGIDLEN	32
#define TMMSGIDLEN_STR	45 /* TMMSGIDLEN * 1.4 (base64 overhead) */
#define TMCORRIDLEN	32
#define TMCORRIDLEN_STR	45 /* TMMSGIDLEN * 1.4 (base64 overhead) */
    
/* structure elements that are valid - set in flags */
#define TPNOFLAGS	0x00000		
#define	TPQCORRID	0x00001		/* set/get correlation id */		
#define	TPQFAILUREQ	0x00002		/* set/get failure queue */		
#define	TPQBEFOREMSGID	0x00004		/* RFU, enqueue before message id */		
#define	TPQGETBYMSGIDOLD	0x00008	/* RFU, deprecated */		
#define	TPQMSGID	0x00010		/* get msgid of enq/deq message */		
#define	TPQPRIORITY	0x00020		/* set/get message priority */		
#define	TPQTOP		0x00040		/* RFU, enqueue at queue top */		
#define	TPQWAIT		0x00080		/* RFU, wait for dequeuing */		
#define	TPQREPLYQ	0x00100		/* set/get reply queue */		
#define	TPQTIME_ABS	0x00200		/* RFU, set absolute time */		
#define	TPQTIME_REL	0x00400		/* RFU, set absolute time */		
#define	TPQGETBYCORRIDOLD	0x00800	/* deprecated */		
#define	TPQPEEK		0x01000		/* peek */		
#define TPQDELIVERYQOS   0x02000         /* RFU, delivery quality of service */		
#define TPQREPLYQOS     0x04000         /* RFU, reply message quality of service */		
#define TPQEXPTIME_ABS  0x08000         /* RFU, absolute expiration time */		
#define TPQEXPTIME_REL  0x10000         /* RFU, relative expiration time */		
#define TPQEXPTIME_NONE 0x20000        	/* RFU, never expire */		
#define	TPQGETBYMSGID	0x40008		/* dequeue by msgid */		
#define	TPQGETBYCORRID	0x80800		/* dequeue by corrid */		
#define TPQASYNC        0x100000    /* Async complete */
		
/* Valid flags for the quality of service fileds in the TPQCTLstructure */		
#define TPQQOSDEFAULTPERSIST  0x00001   /* queue's default persistence policy */		
#define TPQQOSPERSISTENT      0x00002   /* disk message */		
#define TPQQOSNONPERSISTENT   0x00004   /* memory message */		

#define QMEINVAL	-1
#define QMEBADRMID	-2
#define QMENOTOPEN	-3
#define QMETRAN		-4
#define QMEBADMSGID	-5
#define QMESYSTEM	-6
#define QMEOS		-7
#define QMEABORTED	-8
#define QMENOTA		QMEABORTED		
#define QMEPROTO	-9
#define QMEBADQUEUE	-10
#define QMENOMSG	-11
#define QMEINUSE	-12
#define QMENOSPACE	-13
#define QMERELEASE      -14
#define QMEINVHANDLE    -15
#define QMESHARE        -16

    
/*---------------------------Enums--------------------------------------*/
/*---------------------------Typedefs-----------------------------------*/

/* client/caller identifier */
struct clientid_t
{
        char    clientdata[NDRX_MAX_ID_SIZE];          
};
typedef struct clientid_t CLIENTID;

/* Transaction ID used by tpsuspend()/tpresume() */
struct tp_tranid_t
{
    /* NOTE ! Same fileds from ATMI_XA_TX_INFO_FIELDS some precompilers have issues with
    parsing macro */
    short tmtxflags;                   /* See TMTXFLAGS_* */
    char tmxid[NDRX_XID_SERIAL_BUFSIZE+1]; /* tmxid, serialized */
    short tmrmid; /* initial resource manager id */
    short tmnodeid; /* initial node id */
    short tmsrvid; /* initial TM server id */
    char tmknownrms[NDRX_MAX_RMS+1]; /* valid values 1..32-1, 0 - reserved + EOS */
    /* END OF ATMI_XA_TX_INFO_FIELDS */
    int is_tx_initiator;
};
typedef struct tp_tranid_t TPTRANID;


struct tpevctl_t
{
    long flags;
    char name1[XATMI_SERVICE_NAME_LENGTH];
    char name2[XATMI_SERVICE_NAME_LENGTH];
};
typedef struct tpevctl_t TPEVCTL;

/*
 * service information structure
 */
typedef struct
{
	char	name[XATMI_SERVICE_NAME_LENGTH];
	char	*data;
	long	len;
	long	flags;
	int 	cd;
	long    appkey;
        CLIENTID cltid; /* RFU */
        char	fname[XATMI_SERVICE_NAME_LENGTH+1]; /* function name */
} TPSVCINFO;

struct	tpinfo_t
{
	char usrname[MAXTIDENT+2];
	char cltname[MAXTIDENT+2];
	char passwd[MAXTIDENT+2];
	char grpname[MAXTIDENT+2];
	long flags;
	long datalen;
	long data;
};
typedef	struct	tpinfo_t TPINIT;

/* Queue support structure: */
struct tpqctl_t 
{
	long flags;		/* indicates which of the values are set */		
	long deq_time;		/* absolute/relative  time for dequeuing */		
	long priority;		/* enqueue priority */		
	long diagnostic;	/* indicates reason for failure */		
        char diagmsg[NDRX_QDIAG_MSG_SIZE]; /* diagnostic message */
	char msgid[TMMSGIDLEN];	/* id of message before which to queue */		
	char corrid[TMCORRIDLEN];/* correlation id used to identify message */		
	char replyqueue[TMQNAMELEN+1];	/* queue name for reply message */		
	char failurequeue[TMQNAMELEN+1];/* queue name for failure message */		
	CLIENTID cltid;		/* client identifier for originating client */		
	long urcode;		/* application user-return code */		
	long appkey;		/* application authentication client key */		
	long delivery_qos;      /* delivery quality of service  */		
	long reply_qos;         /* reply message quality of service  */		
	long exp_time;          /* expiration time  */		
};		
typedef struct tpqctl_t TPQCTL;		

/*---------------------------Globals------------------------------------*/
extern NDRX_API int (*G_tpsvrinit__)(int, char **);
extern NDRX_API void (*G_tpsvrdone__)(void);
/*---------------------------Prototypes---------------------------------*/

/*
 * xatmi communication api
 */
extern NDRX_API int tpacall(char *svc, char *data, long len, long flags);
extern NDRX_API int tpadvertise_full (char *, void (*)(TPSVCINFO *), char *);
extern NDRX_API char *tpalloc(char *type, char *subtype, long size);
extern NDRX_API int tpcall(char *svc, char *idata, long ilen, char **odata, long *olen, long flags);
extern NDRX_API int tpcancel(int cd);
extern NDRX_API int tpconnect(char *svc, char *data, long len, long flags);
extern NDRX_API int tpdiscon(int cd);
extern NDRX_API void tpfree(char *ptr);
extern NDRX_API int tpgetrply(int *cd, char **data, long *len, long flags);
extern NDRX_API char *tprealloc(char *ptr, long size);
extern NDRX_API int tprecv(int cd, char **data, long *len, long flags, long *revent);
extern NDRX_API void tpreturn(int rval, long rcode, char *data, long len, long flags);
extern NDRX_API int tpsend(int cd, char *data, long len, long flags, long *revent);
extern NDRX_API void tpservice(TPSVCINFO *svcinfo);
extern NDRX_API long tptypes(char *ptr, char *type, char *subtype);
extern NDRX_API int tpunadvertise(char *svcname);

/* 
 * Extension functions
 */
extern NDRX_API void tpforward (char *svc, char *data, long len, long flags);
extern NDRX_API int tpabort (long flags);
extern NDRX_API int tpbegin (unsigned long timeout, long flags);
extern NDRX_API int tpcommit (long flags);
extern NDRX_API int tpconvert (char *strrep, char *binrep, long flags);
extern NDRX_API int tpsuspend (TPTRANID *tranid, long flags);
extern NDRX_API int tpresume (TPTRANID *tranid, long flags);
extern NDRX_API int tpopen (void);
extern NDRX_API int tpclose (void);
extern NDRX_API int tpgetlev (void);
extern NDRX_API char * tpstrerror (int err);
extern NDRX_API char * tpsrvgetctxdata (void); 
extern NDRX_API int tpsrvsetctxdata (char *data, long flags);
extern NDRX_API void tpsrvcontinue(void);
extern NDRX_API void tpcontinue (void);
extern NDRX_API long tpgetnodeid(void);
extern NDRX_API long tpsubscribe (char *eventexpr, char *filter, TPEVCTL *ctl, long flags);
extern NDRX_API int tpunsubscribe (long subscription, long flags);
extern NDRX_API int tppost (char *eventname, char *data, long len, long flags);
extern NDRX_API int * _exget_tperrno_addr (void);
extern NDRX_API long * _exget_tpurcode_addr (void);
/*extern NDRX_API void tpsvrdone (void);*/
extern NDRX_API int tpinit(TPINIT *tpinfo);

/* in external application: */
extern NDRX_API void tpsvrdone(void);
extern NDRX_API int tpsvrinit (int argc, char **argv);

/* Poller & timer extension: */
extern NDRX_API int tpext_addpollerfd(int fd, uint32_t events, 
        void *ptr1, int (*p_pollevent)(int fd, uint32_t events, void *ptr1));
extern NDRX_API int tpext_delpollerfd(int fd);
extern NDRX_API int tpext_addperiodcb(int secs, int (*p_periodcb)(void));
extern NDRX_API int tpext_delperiodcb(void);
extern NDRX_API int tpext_addb4pollcb(int (*p_b4pollcb)(void));
extern NDRX_API int tpext_delb4pollcb(void);
extern NDRX_API int tpterm (void);
extern NDRX_API int tpgetsrvid (void);

/* JSON<->ubf buffer support */
extern NDRX_API int tpjsontoubf(UBFH *p_ub, char *buffer);
extern NDRX_API int tpubftojson(UBFH *p_ub, char *buffer, int bufsize);


/* Queue support: */
extern NDRX_API int tpenqueue (char *qspace, char *qname, TPQCTL *ctl, char *data, long len, long flags);
extern NDRX_API int tpdequeue (char *qspace, char *qname, TPQCTL *ctl, char **data, long *len, long flags);

extern NDRX_API int tpenqueueex (short nodeid, short srvid, char *qname, TPQCTL *ctl, char *data, long len, long flags);
extern NDRX_API int tpdequeueex (short nodeid, short srvid, char *qname, TPQCTL *ctl, char **data, long *len, long flags);

extern NDRX_API int ndrx_main(int argc, char **argv); /* exported by atmisrvnomain */
extern NDRX_API int ndrx_main_integra(int argc, char** argv, int in_tpsvrinit(int, char **), 
            void (*in_tpsvrdone)(void), long flags);

#if defined(__cplusplus)
}
#endif

#endif /* __XATMI_H__ */
