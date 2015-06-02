#ifndef MEGA_API_WRAPPER_H_
#define MEGA_API_WRAPPER_H_

#include <inttypes.h>

typedef void CMegaApi;
typedef void CMegaRequestListener;
typedef void CMegaNode;
typedef void CMegaNodeList;
typedef void CMegaTransferListener;

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LOG_LEVEL_FATAL = 0,
    LOG_LEVEL_ERROR = 1,
    LOG_LEVEL_WARNING = 2,
    LOG_LEVEL_INFO = 3,
    LOG_LEVEL_DEBUG = 4,
    LOG_LEVEL_MAX = 5,
} LogLevel;

enum {
    API_OK = 0,             ///< Everything OK
    API_EINTERNAL = -1,     ///< Internal error.
    API_EARGS = -2,         ///< Bad arguments.
    API_EAGAIN = -3,        ///< Request failed, retry with exponential back-off.
    API_ERATELIMIT = -4,    ///< Too many requests, slow down.
    API_EFAILED = -5,       ///< Request failed permanently.
    API_ETOOMANY = -6,      ///< Too many requests for this resource.
    API_ERANGE = -7,        ///< Resource access out of rage.
    API_EEXPIRED = -8,      ///< Resource expired.
    API_ENOENT = -9,        ///< Resource does not exist.
    API_ECIRCULAR = -10,    ///< Circular linkage.
    API_EACCESS = -11,      ///< Access denied.
    API_EEXIST = -12,       ///< Resource already exists.
    API_EINCOMPLETE = -13,  ///< Request incomplete.
    API_EKEY = -14,         ///< Cryptographic error.
    API_ESID = -15,         ///< Bad session ID.
    API_EBLOCKED = -16,     ///< Resource administratively blocked.
    API_EOVERQUOTA = -17,   ///< Quota exceeded.
    API_ETEMPUNAVAIL = -18, ///< Resource temporarily not available.
    API_ETOOMANYCONNECTIONS = -19, ///< Too many connections on this resource.
    API_EWRITE = -20,       ///< File could not be written to (or failed post-write integrity check).
    API_EREAD = -21,        ///< File could not be read from (or changed unexpectedly during reading).
    API_EAPPKEY = -22,       ///< Invalid or missing application key.

    PAYMENT_ECARD = -101,
    PAYMENT_EBILLING = -102,
    PAYMENT_EFRAUD = -103,
    PAYMENT_ETOOMANY = -104,
    PAYMENT_EBALANCE = -105,
    PAYMENT_EGENERIC = -106
};

enum {
    TYPE_UNKNOWN = -1,
    TYPE_FILE = 0,
    TYPE_FOLDER,
    TYPE_ROOT,
    TYPE_INCOMING,
    TYPE_RUBBISH
};


CMegaApi *mega_api_create(const char *appKey, const char *basePath, const char *userAgent);
void mega_api_set_loglevel(CMegaApi *mega_api, int logLevel);
void mega_api_login(CMegaApi *mega_api, const char* email, const char* password, CMegaRequestListener *clistener);
void mega_api_fetch_nodes(CMegaApi *mega_api, CMegaRequestListener *clistener);
CMegaNode *mega_api_get_node_by_path(CMegaApi *mega_api, const char *path);
CMegaNodeList *mega_api_node_get_children(CMegaApi *mega_api, CMegaNode *cnode);
CMegaNode *mega_api_node_get_child_node(CMegaApi *mega_api, CMegaNode *parent, const char* name);
int mega_api_node_list_get_n_children(CMegaApi *mega_api, CMegaNodeList *cnode_list);
CMegaNode *mega_api_node_list_get_child(CMegaNodeList *cnode_list, int i);
int mega_api_get_num_children(CMegaApi *mega_api, CMegaNode *parent);

void mega_api_get_preview(CMegaApi *mega_api, CMegaNode* cnode, const char *dstFilePath, CMegaRequestListener *clistener);
void mega_api_start_download(CMegaApi *mega_api, CMegaNode* node, const char* localPath, CMegaTransferListener *clistener);

const char *mega_api_node_get_name(CMegaNode *cnode);
int mega_api_node_is_file(CMegaNode *cnode);
int64_t mega_api_node_get_size(CMegaNode *cnode);
int64_t mega_api_node_get_creation_time(CMegaNode *cnode);
int64_t mega_api_node_get_modification_time(CMegaNode *cnode);
int mega_api_node_get_type(CMegaNode *cnode);
const char *mega_api_node_get_key(CMegaNode *cnode);
const char *mega_api_node_get_handle(CMegaNode *cnode);
int mega_api_node_is_removed(CMegaNode *cnode);
int mega_api_node_has_thumbnail(CMegaNode *cnode);
int mega_api_node_has_preview(CMegaNode *cnode);
int mega_api_node_is_public(CMegaNode *cnode);


typedef void (*CMegaRequestListener_on_wait_cb)(void *ctx);
typedef void (*CMegaRequestListener_on_start_cb)(void *ctx);
typedef void (*CMegaRequestListener_on_finish_cb)(void *ctx, int errorCode);
CMegaRequestListener *mega_request_listener_create(void *ctx,
    CMegaRequestListener_on_start_cb on_start_cb,
    CMegaRequestListener_on_finish_cb on_finish_cb,
    CMegaRequestListener_on_wait_cb on_wait_cb
);
void mega_request_listener_wait(CMegaRequestListener *clistener);
void mega_request_listener_destroy(CMegaRequestListener *clistener);



CMegaTransferListener *mega_transfer_listener_create(void *ctx);
void mega_transfer_listener_destroy(CMegaTransferListener *clistener);


#ifdef __cplusplus
}
#endif

#endif
