/*
 * (C) 2015 The University of Chicago
 * 
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <abt.h>
#include <abt-snoozer.h>
#include <margo.h>

#include "svc1-client.h"

static hg_id_t my_rpc_shutdown_id;

int main(int argc, char **argv) 
{
    int i;
    int ret;
    margo_instance_id mid;
    hg_context_t *hg_context;
    hg_class_t *hg_class;
    hg_addr_t svr_addr = HG_ADDR_NULL;
    hg_handle_t handle;
    char proto[12] = {0};
  
    if(argc != 2)
    {
        fprintf(stderr, "Usage: ./client <server_addr>\n");
        return(-1);
    }
       
    /* boilerplate HG initialization steps */
    /***************************************/

    /* initialize Mercury using the transport portion of the destination
     * address (i.e., the part before the first : character if present)
     */
    for(i=0; i<11 && argv[1][i] != '\0' && argv[1][i] != ':'; i++)
        proto[i] = argv[1][i];
    hg_class = HG_Init(proto, HG_FALSE);
    if(!hg_class)
    {
        fprintf(stderr, "Error: HG_Init()\n");
        return(-1);
    }
    hg_context = HG_Context_create(hg_class);
    if(!hg_context)
    {
        fprintf(stderr, "Error: HG_Context_create()\n");
        HG_Finalize(hg_class);
        return(-1);
    }

    /* set up argobots */
    /***************************************/
    ret = ABT_init(argc, argv);
    if(ret != 0)
    {
        fprintf(stderr, "Error: ABT_init()\n");
        return(-1);
    }

    /* set primary ES to idle without polling */
    ret = ABT_snoozer_xstream_self_set();
    if(ret != 0)
    {
        fprintf(stderr, "Error: ABT_snoozer_xstream_self_set()\n");
        return(-1);
    }

    /* actually start margo */
    /***************************************/
    mid = margo_init(0, 0, hg_context);

    /* register core RPC */
    my_rpc_shutdown_id = MERCURY_REGISTER(hg_class, "my_shutdown_rpc", void, void, 
        NULL);
    /* register service APIs */
    svc1_register_client(mid);

    /* find addr for server */
    ret = margo_addr_lookup(mid, argv[1], &svr_addr);
    assert(ret == 0);

    svc1_do_thing(mid, svr_addr, 1);

    /* send one rpc to server to shut it down */
    /* create handle */
    ret = HG_Create(hg_context, svr_addr, my_rpc_shutdown_id, &handle);
    assert(ret == 0);

    margo_forward(mid, handle, NULL);

    HG_Addr_free(hg_class, svr_addr);

    /* shut down everything */
    margo_finalize(mid);
    
    ABT_finalize();

    HG_Context_destroy(hg_context);
    HG_Finalize(hg_class);

    return(0);
}

