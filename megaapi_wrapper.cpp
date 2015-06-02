#include "megaapi_wrapper.h"

#include "megaapi.h"

using namespace mega;
using namespace std;

class SynchronousRequestListener : public MegaRequestListener
{
	public:
        SynchronousRequestListener(void *ctx,
            CMegaRequestListener_on_start_cb on_start_cb,
            CMegaRequestListener_on_finish_cb on_finish_cb,
            CMegaRequestListener_on_wait_cb on_wait_cb):
            ctx(ctx), on_start_cb(on_start_cb), on_finish_cb(on_finish_cb), on_wait_cb(on_wait_cb) {
        }

        void onRequestStart(MegaApi* api, MegaRequest *request) {
            on_start_cb(ctx);
        }

        void onRequestFinish(MegaApi* api, MegaRequest *request, MegaError* e) {
            on_finish_cb(ctx, e->getErrorCode());
        }

        void wait() {
            on_wait_cb(ctx);
        }

	private:
        void *ctx;
        CMegaRequestListener_on_start_cb on_start_cb;
        CMegaRequestListener_on_finish_cb on_finish_cb;
        CMegaRequestListener_on_wait_cb on_wait_cb;
};

class SynchronousTransferListener : public MegaTransferListener
{
};

extern "C" {

CMegaApi *mega_api_create(const char *appKey, const char *basePath, const char *userAgent)
{
    return new MegaApi(appKey, basePath, userAgent);
}

void mega_api_set_loglevel(CMegaApi *mega_api, int logLevel)
{
    MegaApi *api = (MegaApi *)mega_api;
    api->setLogLevel(logLevel);
}

void mega_api_login(CMegaApi *mega_api, const char* email, const char* password, CMegaRequestListener *clistener)
{
    MegaApi *api = (MegaApi *)mega_api;
    MegaRequestListener *listener = (MegaRequestListener *)clistener;

    api->login(email, password, listener);
}

void mega_api_fetch_nodes(CMegaApi *mega_api, CMegaRequestListener *clistener)
{
    MegaApi *api = (MegaApi *)mega_api;
    MegaRequestListener *listener = (MegaRequestListener *)clistener;

    api->fetchNodes(listener);
}

CMegaNode *mega_api_get_node_by_path(CMegaApi *mega_api, const char *path)
{
    MegaApi *api = (MegaApi *)mega_api;
    return api->getNodeByPath(path);
}

CMegaNodeList *mega_api_node_get_children(CMegaApi *mega_api, CMegaNode *cnode)
{
    MegaApi *api = (MegaApi *)mega_api;
    MegaNode *node = (MegaNode *)cnode;

    return api->getChildren(node);
}

CMegaNode *mega_api_node_get_child_node(CMegaApi *mega_api, CMegaNode *parent, const char* name)
{
    MegaApi *api = (MegaApi *)mega_api;
    MegaNode *node = (MegaNode *)parent;

    return api->getChildNode(node, name);
}

int mega_api_get_num_children(CMegaApi *mega_api, CMegaNode *parent)
{
    MegaApi *api = (MegaApi *)mega_api;
    MegaNode *node = (MegaNode *)parent;

    return api->getNumChildren(node);
}

int mega_api_node_list_get_n_children(CMegaApi *mega_api, CMegaNodeList *cnode_list)
{
    MegaApi *api = (MegaApi *)mega_api;
    MegaNodeList *node_list = (MegaNodeList *)cnode_list;

    return node_list->size();
}

CMegaNode *mega_api_node_list_get_child(CMegaNodeList *cnode_list, int i)
{
    MegaNodeList *node_list = (MegaNodeList *)cnode_list;

    return node_list->get(i);
}

const char *mega_api_node_get_name(CMegaNode *cnode)
{
    MegaNode *node = (MegaNode *)cnode;
    return node->getName();
}

int mega_api_node_is_file(CMegaNode *cnode)
{
    MegaNode *node = (MegaNode *)cnode;
    return node->isFile();
}

int64_t mega_api_node_get_size(CMegaNode *cnode)
{
    MegaNode *node = (MegaNode *)cnode;
    return node->getSize();
}

int64_t mega_api_node_get_creation_time(CMegaNode *cnode)
{
    MegaNode *node = (MegaNode *)cnode;
    return node->getCreationTime();
}

int64_t mega_api_node_get_modification_time(CMegaNode *cnode)
{
    MegaNode *node = (MegaNode *)cnode;
    return node->getModificationTime();
}

int mega_api_node_get_type(CMegaNode *cnode)
{
    MegaNode *node = (MegaNode *)cnode;
    return node->getType();
}

const char *mega_api_node_get_key(CMegaNode *cnode)
{
    MegaNode *node = (MegaNode *)cnode;
    return node->getBase64Key();
}

const char *mega_api_node_get_handle(CMegaNode *cnode)
{
    MegaNode *node = (MegaNode *)cnode;
    return node->getBase64Handle();
}

int mega_api_node_is_removed(CMegaNode *cnode)
{
    MegaNode *node = (MegaNode *)cnode;
    return node->isRemoved();
}

int mega_api_node_has_thumbnail(CMegaNode *cnode)
{
    MegaNode *node = (MegaNode *)cnode;
    return node->hasThumbnail();
}

int mega_api_node_has_preview(CMegaNode *cnode)
{
    MegaNode *node = (MegaNode *)cnode;
    return node->hasPreview();
}

int mega_api_node_is_public(CMegaNode *cnode)
{
    MegaNode *node = (MegaNode *)cnode;
    return node->isPublic();
}

void mega_api_get_preview(CMegaApi *mega_api, CMegaNode* cnode, const char *dstFilePath, CMegaRequestListener *clistener)
{
    MegaApi *api = (MegaApi *)mega_api;
    MegaRequestListener *listener = (MegaRequestListener *)clistener;
    MegaNode *node = (MegaNode *)cnode;

    api->getPreview(node, dstFilePath, listener);
}

void mega_api_start_download(CMegaApi *mega_api, CMegaNode* cnode, const char* localPath, CMegaTransferListener *clistener)
{
    MegaApi *api = (MegaApi *)mega_api;
    MegaTransferListener *listener = (MegaTransferListener *)clistener;
    MegaNode *node = (MegaNode *)cnode;

    api->startDownload(node, localPath, listener);
}


CMegaRequestListener *mega_request_listener_create(void *ctx, CMegaRequestListener_on_start_cb on_start_cb,
    CMegaRequestListener_on_finish_cb on_finish_cb, CMegaRequestListener_on_wait_cb on_wait_cb)
{
    return new SynchronousRequestListener(ctx, on_start_cb, on_finish_cb, on_wait_cb);
}

void mega_request_listener_wait(CMegaRequestListener *clistener)
{
    SynchronousRequestListener *listener = (SynchronousRequestListener *)clistener;
    listener->wait();
}

void mega_request_listener_destroy(CMegaRequestListener *clistener)
{
    SynchronousRequestListener *listener = (SynchronousRequestListener *)clistener;
    delete listener;
}

CMegaTransferListener *mega_transfer_listener_create(void *ctx)
{
    return new SynchronousTransferListener();
}

void mega_transfer_listener_destroy(CMegaTransferListener *clistener)
{
    SynchronousTransferListener *listener = (SynchronousTransferListener *)clistener;
    delete listener;
}

}
