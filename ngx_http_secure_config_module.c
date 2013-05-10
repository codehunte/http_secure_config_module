
/*
 * Copyright (C) 
 * Copyright (C) 
 * author:      	wu yangping
 * create time:		20120600
 * update time: 	20120727
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <cjson.h>
#include <nginx.h>
#include <ngx_white_black_list.h>
//#include <ngx_config_update.h>
#include <ngx_white_black_list.h>
#include <ngx_http_secure_config_funs.h>
#include <ngx_http_secure_config_module.h>

#define ZONE_NAME 1
#define ADD_ITEM 2
#define DELETE_ITEM 3
#define CONFIG_ERROR 4
#define FOR_EACH	 5
#define LIMIT_HOST_LIST 6

#define ADD_ITEM_STR			"add_item"
#define DELETE_ITEM_STR			"delete_item"
#define ZONE_NAME_STR			"zone_name"
#define HTTP_SECURE_CONFIG_URI          "secure_config_update"
#define FOR_EACH_STR			"for_each"
#define LIMIT_HOST_LIST_STR		"limit_host"



#define SECURE_CONFIG_POOL_SIZE	1024*1024
#define ARGS_CONFIG_POOL_SIZE	1024

#define SECURE_CONFIG_USE_WHERE   0x00000001		/*need parse where*/
#define SECURE_CONFIG_USE_ARGV	  0x00000002		/*neeed parse arg*/

#define SECURE_CONFIG_USE_LOC	  0x00000004		/*use core_loc*/
#define SECURE_CONFIG_USE_LOC_M	  0x00000008		/*use module_loc*/
#define SECURE_CONFIG_USE_SRV	  0x00000010		/*use module_srv*/

u_char *zone_name_is_empty = (u_char *)"zone_name is empty!";

ngx_int_t					locations_need_update = 0;
ngx_conf_t           		ngx_conf_g;
ngx_array_t					*locations;
ngx_rbtree_t       			l_host_rbtree;
ngx_limit_rate_conf_t		host_limit_conf;


typedef struct {
	ngx_str_t			cmd;
	ngx_uint_t			type;
	ngx_module_t		*tag;
	char				*(*update_config)(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
	char				*(*add_config)(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
	char				*(*del_config)(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
}ngx_conf_fun;

static char *ngx_http_config_module_set(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_http_secure_config_set(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_http_limit_rate_config_set(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static void* ngx_http_secure_config_cmd_parse( ngx_http_request_t *r );
static ngx_int_t ngx_http_secure_config_handle( void* data,ngx_int_t size );

static ngx_conf_fun ngx_conf_update_fun[] = {
		{ ngx_string("white_list"),
		  SECURE_CONFIG_USE_WHERE|SECURE_CONFIG_USE_ARGV|SECURE_CONFIG_USE_LOC_M,
		  &ngx_white_black_list_module,
		  ngx_http_white_black_list_set,			/*update*/
		  ngx_http_white_black_list_set,			/*add*/
		  ngx_http_white_black_list_del				/*del*/
		},

		{ ngx_string("black_list"),
		  SECURE_CONFIG_USE_WHERE|SECURE_CONFIG_USE_ARGV|SECURE_CONFIG_USE_LOC_M,
		  &ngx_white_black_list_module,
		  ngx_http_white_black_list_set,			/*update*/
		  ngx_http_white_black_list_set,			/*add*/
		  ngx_http_white_black_list_del				/*del*/
		},
		
		{ ngx_string("limit_req_zone"),
		  SECURE_CONFIG_USE_ARGV,
		  &ngx_http_limit_req_module,
		  ngx_limit_req_zone_config_update_interface,
		  NULL,
		  NULL
		},

		{ ngx_string("limit_req"),
		  SECURE_CONFIG_USE_WHERE|SECURE_CONFIG_USE_ARGV|SECURE_CONFIG_USE_LOC_M,
		  &ngx_http_limit_req_module,
		  ngx_limit_req_config_update,
		  ngx_limit_req_config_update,
		  ngx_limit_req_config_del
		},

		{ ngx_string("limit_conn_zone"),
		  SECURE_CONFIG_USE_ARGV,
		  &ngx_http_limit_conn_module,
		  ngx_limit_conn_zone_config_update,
		  NULL,
		  NULL
		},

		{ ngx_string("limit_conn"),
		  SECURE_CONFIG_USE_WHERE|SECURE_CONFIG_USE_ARGV|SECURE_CONFIG_USE_LOC_M,
		  &ngx_http_limit_conn_module,
		  ngx_limit_conn_config_update,
		  ngx_limit_conn_config_update,
		  ngx_limit_conn_config_del
		},

		{ ngx_string("limit_rate"),
		  SECURE_CONFIG_USE_WHERE|SECURE_CONFIG_USE_ARGV|SECURE_CONFIG_USE_LOC,
		  NULL,
		  ngx_limit_rate_config_update,
		  ngx_limit_rate_config_update,
		  ngx_limit_rate_config_del
		},

		{ ngx_string("location"),
		  SECURE_CONFIG_USE_WHERE|SECURE_CONFIG_USE_ARGV|SECURE_CONFIG_USE_SRV,
		  NULL,
		  ngx_location_update,
		  ngx_location_add,
		  ngx_location_del
		},

		{ ngx_string("alias"),
		  SECURE_CONFIG_USE_WHERE|SECURE_CONFIG_USE_ARGV|SECURE_CONFIG_USE_LOC,
		  NULL,
		  ngx_http_private_root,
		  ngx_http_private_root,
		  NULL
		},
		
		{ ngx_string("root"),
		  SECURE_CONFIG_USE_WHERE|SECURE_CONFIG_USE_ARGV|SECURE_CONFIG_USE_LOC,
		  NULL,
		  ngx_http_private_root,
		  ngx_http_private_root,
		  NULL
		},

		{ ngx_string("limit_rate_conf"),
		  SECURE_CONFIG_USE_ARGV,
		  NULL,
		  ngx_http_lrlc_add,
		  ngx_http_lrlc_add,
		  ngx_http_lrlc_del
		},
		
		{
		  ngx_null_string,
		  0,
		  NULL,
		  NULL,
		  NULL,
		  NULL
		}
};

static ngx_command_t  ngx_http_config_module_commands[] = {

    { ngx_string("sec_config"),
      NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_http_config_module_set,
      0,
      0,
      NULL },

	{ ngx_string("limit_rate_conf"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_http_limit_rate_config_set,
      0,
      0,
      NULL },

	{ ngx_string(HTTP_SECURE_CONFIG_URI),
      NGX_HTTP_LOC_CONF | NGX_CONF_NOARGS,
      ngx_http_secure_config_set,
      0,
      0,
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_config_module_ctx = {
    NULL,                                  /* preconfiguration */
    NULL,                                  /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    NULL,                                  /* create location configuration */
    NULL                                   /* merge location configuration */
};


ngx_module_t  ngx_http_secure_config_module = {
    NGX_MODULE_V1,
    &ngx_http_config_module_ctx,      	   /* module context */
    ngx_http_config_module_commands,       /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};

static void* ngx_http_secure_config_cmd_parse( ngx_http_request_t *r )
{
/*
	ngx_config_update_ctx_t *cuctx;

	cuctx = ngx_http_get_module_ctx(r, ngx_config_update_framework_module);
	cuctx->data = cuctx->body_buf;
	cuctx->data_size = cuctx->body_size;

	return cuctx->data;
*/
	return NULL;
}

static char *
ngx_http_secure_config_set( ngx_conf_t *cf, ngx_command_t *cmd, void *conf ) {

/*
	ngx_config_update_loc_conf_t  *culcf;
	ngx_config_update_cmd_conf_t *cucf;

	culcf = ngx_http_conf_get_module_loc_conf( cf, ngx_config_update_framework_module );

	if( culcf == NULL ) {
		ngx_conf_log_error( NGX_LOG_ERR, cf, 0,"can not find update module loc_conf ");
		return NGX_CONF_ERROR;
	}

	if( culcf->nelt >= culcf->size ) {
		ngx_conf_log_error( NGX_LOG_ERR, cf, 0," had reached max config command count ");
		return NGX_CONF_ERROR;
	}

	cucf = ngx_pcalloc(	cf->pool,sizeof(ngx_config_update_cmd_conf_t) );
	if( cucf == NULL ) {
		ngx_conf_log_error( NGX_LOG_ERR, cf, 0," pcalloc  update command error");
		return NGX_CONF_ERROR;
	}

	cucf->path.data = (u_char*)HTTP_SECURE_CONFIG_URI;
	cucf->path.len = sizeof(HTTP_SECURE_CONFIG_URI) - 1;
	cucf->free = NULL;
	cucf->parse = ngx_http_secure_config_cmd_parse;
	cucf->update_handler = ngx_http_secure_config_handle;
	cucf->version = 10;
	cucf->cmd = culcf->nelt;

	culcf->cmds[ culcf->nelt++ ] = cucf;
*/
	return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_for_each_location(ngx_log_t *log, 
    ngx_http_location_tree_node_t *node, ngx_array_t *location)
{
    size_t      len, n;
    ngx_int_t   rv;
	ngx_locations  *value;
	
    rv = NGX_DECLINED;

    if (node == NULL) {
        return rv;
    }

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, log, 0,
                   "test location: \"%*s\"", node->len, node->name);

    n = (len <= (size_t) node->len) ? len : node->len;


	value = ngx_array_push(location);
	if (value == NULL)
	{
		return rv;
	}

	value->name.len=0;
	
	if (node->exact!= NULL)
		value->t = node->exact;
	else
		value->t = node->inclusive;

	if (node->left != NULL)
		ngx_http_for_each_location(log, node->left, location);

	if (node->right != NULL)
		ngx_http_for_each_location(log, node->right, location);

	if (node->inclusive) {
        rv = NGX_AGAIN;
    }

	if (node->tree)
		ngx_http_for_each_location(log,node->tree, location);
	
	return rv;
}

ngx_int_t ngx_create_locations()
{
	ngx_uint_t				  n,m,z,y;
	ngx_http_core_srv_conf_t  *cscf;
	ngx_http_core_loc_conf_t  *clcf, **addr;
	ngx_locations			  *value;
	ngx_listening_t           *ls;
	ngx_http_in_addr_t		  *hia;
	ngx_http_port_t			  *pots;

	if (locations && locations->nelts > 0 && locations_need_update == 0)
		return NGX_OK;

	locations_need_update = 0;
	
	ls = ngx_cycle->listening.elts;

	if (ls == NULL || ls[0].servers == NULL)
		return NGX_ERROR;

	if (ngx_conf_g.pool== NULL)
	{
		ngx_conf_g.pool = ngx_create_pool(SECURE_CONFIG_POOL_SIZE, ngx_conf_g.log);
		if (ngx_conf_g.pool == NULL)
			return NGX_ERROR;
	}
	
	locations = ngx_array_create(ngx_conf_g.pool, 8, sizeof(ngx_locations));
	if (locations == NULL)
		return NGX_ERROR;

	
	for (m=0; m< ngx_cycle->listening.nelts; m++)
	{
		pots = ls[m].servers;
		if (pots == NULL || pots[0].addrs == NULL)
			continue;
		y=0;
		n=0;
		for (y =0; pots[y].addrs != NULL; y++)
		{
			hia = pots[y].addrs;
			if (hia == NULL)
				continue;
			
			for (n = 0; pots[y].addrs!=NULL && n < pots[y].naddrs; n++)
			{
				cscf = hia[n].conf.default_server;
				if (cscf == NULL 
					|| cscf->ctx == NULL 
					|| cscf->ctx->loc_conf == NULL
					)
				{
					continue;
				}

				if (ngx_conf_g.ctx == NULL)
					ngx_conf_g.ctx = cscf->ctx;
				
				clcf = cscf->ctx->loc_conf[ngx_http_core_module.ctx_index];
				if (cscf->server_name.len != 0)
				{
					value = ngx_array_push(locations);
					if (value != NULL)
					{
						value->name.data= cscf->server_name.data;
						value->name.len= cscf->server_name.len;
						value->srv = cscf;
						
						if (cscf->ctx && cscf->ctx->main_conf)
							value->main = cscf->ctx->main_conf[0];
						
						value->addr_text.data = ls[m].addr_text.data;
						value->addr_text.len = ls[m].addr_text.len;
					}
					else
					{
						continue;
					}
				}

				/*for_each_location*/
				if (clcf && clcf->static_locations)
				{
					ngx_http_for_each_location(ngx_conf_g.log, clcf->static_locations, locations);
				}

				if (clcf && clcf->regex_locations)
				{
					addr = clcf->regex_locations;
					for (z=0; addr[z]!=NULL; z++)
					{
						value = ngx_array_push(locations);
						if (value != NULL)
						{
							value->name.len = 0;
							value->t = addr[z];
						}
						else
						{
							continue;
						}
					}
				}		
			}
		}
		
	}
	
	return NGX_OK;
}

static ngx_int_t ngx_for_each_share_memory(cJSON *root, ngx_http_request_t *r)
{
	u_char							buf_temp[260], buf_temp1[64], buf_temp2[32];
	cJSON					  		*fmt;
	ngx_uint_t						i;
	ngx_list_t						share_list;
	ngx_shm_zone_t      			*shm_zone;
	ngx_list_part_t     			*part;

	ngx_http_limit_req_ctx_t_p  	*lr_ctx;
	ngx_http_limit_conn_ctx_t_p	 	*lc_ctx;
	ngx_white_black_list_ctx_t 		*wb_ctx;

	share_list = ngx_cycle->shared_memory;
	
	part = &share_list.part;
    shm_zone = part->elts;

	cJSON_AddItemToObject(root, "zone", fmt=cJSON_CreateObject());
	for (i = 0; /* void */ ; i++) {
	
		if (i >= part->nelts) {
			if (part->next == NULL) {
				break;
			}
			part = part->next;
			shm_zone = part->elts;
			i = 0;
		}

		/*parse data*/
		if (shm_zone[i].tag == &ngx_white_black_list_module)
		{
			wb_ctx = shm_zone[i].data;
			ngx_memzero(buf_temp, 260);
			ngx_memzero(buf_temp1, 64);
			ngx_memzero(buf_temp2, 32);

			ngx_memcpy(buf_temp1, wb_ctx->var.data, wb_ctx->var.len);
			ngx_memcpy(buf_temp2, shm_zone[i].shm.name.data, shm_zone[i].shm.name.len);
			
			ngx_snprintf(buf_temp, 259, "%s zone=%s:%d" , buf_temp1, buf_temp2, shm_zone[i].shm.size);
			cJSON_AddStringToObject(fmt, "white_black_list_conf", buf_temp);
		}

		if (shm_zone[i].tag == &ngx_http_limit_req_module)
		{
			lr_ctx = shm_zone[i].data;
			ngx_memzero(buf_temp, 260);
			ngx_memzero(buf_temp1, 64);
			ngx_memzero(buf_temp2, 32);

			ngx_memcpy(buf_temp1, lr_ctx->var.data, lr_ctx->var.len);
			ngx_memcpy(buf_temp2, shm_zone[i].shm.name.data, shm_zone[i].shm.name.len);

			ngx_snprintf(buf_temp, 259, "$%s zone=%s:%d rate=%dr/s", buf_temp1, buf_temp2, shm_zone[i].shm.size, lr_ctx->rate/1000);
			cJSON_AddStringToObject(fmt, "limit_req_zone", buf_temp);
		}

		if (shm_zone[i].tag == &ngx_http_limit_conn_module)
		{
			lc_ctx = shm_zone[i].data;
			ngx_memzero(buf_temp, 260);
			ngx_memzero(buf_temp1, 64);
			ngx_memzero(buf_temp2, 32);

			ngx_memcpy(buf_temp1, lc_ctx->var.data, lc_ctx->var.len);
			ngx_memcpy(buf_temp2, shm_zone[i].shm.name.data, shm_zone[i].shm.name.len);

			ngx_snprintf(buf_temp, 259, "$%s zone=%s:%d", buf_temp1, buf_temp2, shm_zone[i].shm.size);
			cJSON_AddStringToObject(fmt, "limit_conn_zone", buf_temp);
		}
		
	}
			
	return NGX_OK;
}

static ngx_int_t ngx_for_each_location(cJSON	*root, ngx_http_request_t *r)
{
	cJSON					  		*fmt = NULL, *fmt_sub=NULL;
	u_char					  		name[260], flag[8], *pos;
	ngx_uint_t				 		n,m;
	ngx_locations			  		*value;
	ngx_white_black_list_conf_t 	*wb_lc;
	ngx_white_black_list_isvalid_t 	*value_wb;
	ngx_http_limit_req_conf_t_p     *lrcf;
    ngx_http_limit_req_limit_t_p    *limits;
	ngx_http_limit_conn_conf_t_p	*lccf;
    ngx_http_limit_conn_limit_t_p   *limits_conn;
	
	
	if (ngx_create_locations() == NGX_ERROR) 
		return NGX_ERROR;
	
	cJSON_AddStringToObject(root, "version", NGINX_VER);
	cJSON_AddStringToObject(root, "code", "0");
	
	/*add view shm_zone*/
	ngx_for_each_share_memory(root, r);
	
	value = (ngx_locations *)locations->elts;
	for (n=0; n < locations->nelts; n++)
	{
		if (value[n].name.len != 0)
		{
			ngx_memzero(name, 260);
			ngx_memcpy(name, value[n].name.data, value[n].name.len);

			cJSON_AddItemToObject(root, "server", fmt=cJSON_CreateObject());
			
			cJSON_AddNumberToObject(fmt, "index", n);
			cJSON_AddStringToObject(fmt, "server_name", name);
			/*add port*/
			ngx_memzero(name, 260);
			ngx_memcpy(name, value[n].addr_text.data, value[n].addr_text.len);
			cJSON_AddStringToObject(fmt, "listen", name);
			continue;
		}

		if (value[n].t == NULL)
			continue;
		
		ngx_memzero(name,260);
		ngx_memcpy(name,value[n].t->name.data, value[n].t->name.len);

		if (fmt == NULL)
		{
			cJSON_AddItemToObject(root, "server", fmt=cJSON_CreateObject());
			cJSON_AddNumberToObject(fmt, "index", n);
		}
		cJSON_AddItemToObject(fmt, name, fmt_sub=cJSON_CreateObject());
		cJSON_AddNumberToObject(fmt_sub, "index", n);

		/*white_black_list*/
		wb_lc = (ngx_white_black_list_conf_t *)value[n].t->loc_conf[ngx_white_black_list_module.ctx_index];
		if (wb_lc && wb_lc->valids.nelts !=0)
		{
			value_wb = wb_lc->valids.elts;
			for (m = 0; m < wb_lc->valids.nelts; m++)
			{
				if (value_wb[m].delete ==1 )
					continue;
				ngx_memzero(flag, 8);
				if (value_wb[m].isvalid)
				{
					ngx_snprintf(flag, 8," on");
				}
				else
				{
					ngx_snprintf(flag, 8," off");
				}
				
				ngx_memzero(name, 260);
				ngx_memcpy(name, value_wb[m].shm_zone->shm.name.data, value_wb[m].shm_zone->shm.name.len);

				ngx_memcpy(name+value_wb[m].shm_zone->shm.name.len, flag, ngx_strlen(flag));
				
				if (value_wb[m].iswhite)
				{	
					cJSON_AddStringToObject(fmt_sub, "white_list", name);
				}
				else
				{
					cJSON_AddStringToObject(fmt_sub, "black_list", name);
				}
			}
		}
		
		/*limit_req*/
		lrcf = value[n].t->loc_conf[ngx_http_limit_req_module.ctx_index];
		if (lrcf && lrcf->limits.nelts != 0)
		{
			limits = lrcf->limits.elts;
			for (m = 0; m <lrcf->limits.nelts; m++)
			{
				ngx_memzero(name, 260);

				pos = name;
				ngx_memcpy(pos, "zone=", ngx_strlen("zone="));
				pos += ngx_strlen("zone=");
				ngx_memcpy(pos, limits[m].shm_zone->shm.name.data, limits[m].shm_zone->shm.name.len);
				pos += limits[m].shm_zone->shm.name.len;
				pos = ngx_snprintf(pos, 260 - ((ngx_int_t)pos-(ngx_int_t)name)," burst=%d", limits[m].burst/1000);

				if (limits[m].nodelay == 1)
				{
					ngx_memcpy(pos, " nodelay", ngx_strlen(" nodelay"));
				}
				
				cJSON_AddStringToObject(fmt_sub, "limit_req", name);
			}
		}

		/*limit_conn*/
		lccf = value[n].t->loc_conf[ngx_http_limit_conn_module.ctx_index];
		if (lccf && lccf->limits.nelts != 0)
		{
			limits_conn = lccf->limits.elts;
			for (m =0; m <lccf->limits.nelts; m++)
			{
				ngx_memzero(name, 260);
				pos = name;
				ngx_memcpy(pos, limits_conn[m].shm_zone->shm.name.data, limits_conn[m].shm_zone->shm.name.len);
				pos += limits_conn[m].shm_zone->shm.name.len;
				pos = ngx_snprintf(pos, 260 - ((ngx_int_t)pos-(ngx_int_t)name)," %d", limits_conn[m].conn);

				cJSON_AddStringToObject(fmt_sub, "limit_conn", name);
			}
		}

		/*limit_rate*/
		if (value[n].t && value[n].t->limit_rate>0)
		{
			cJSON_AddNumberToObject(fmt_sub, "limit_rate", value[n].t->limit_rate);
		}

		/*root or alias*/
		if (value[n].t && value[n].t->root.data && value[n].t->handler == NULL)
		{
			ngx_memzero(name, 260);
			ngx_memcpy(name, value[n].t->root.data, value[n].t->root.len);
			if (value[n].t->alias != 0)
			{
				cJSON_AddStringToObject(fmt_sub, "alias", name);
			}
			else
			{
				cJSON_AddStringToObject(fmt_sub, "root", name);
			}
		}
	}
	
	return NGX_OK;
}

static ngx_int_t ngx_for_each_limit_host_rate(cJSON	*root, ngx_http_request_t *r)
{
	char			buf[512];
	cJSON			*fmt;
	ngx_host_list_t	*pos;
	
	if (root == NULL || r==NULL)
		return NGX_ERROR;

	cJSON_AddStringToObject(root, "version", NGINX_VER);
	cJSON_AddStringToObject(root, "code", "0");
	
	cJSON_AddItemToObject(root, "host_limit", fmt=cJSON_CreateObject());
	
	for (pos = host_limit_conf.host_list;
		 pos != NULL; pos=pos->next)
	{
		if (pos->host.data == NULL || pos->host.len == 0)
			continue;
		
		if (pos->host.len>511)
			continue;

		ngx_memzero(buf, 512);
		ngx_memcpy(buf, pos->host.data, pos->host.len);

		cJSON_AddNumberToObject(fmt, buf, pos->limit_rate);
	}
	
	return NGX_OK;
}


static ngx_int_t ngx_http_get_value_ex(u_char *buf, ngx_int_t buf_len, u_char *pos, u_char *name, ngx_str_t *value, u_char skip)
{
	u_char *end;
	if (pos == NULL)
	{
		pos = ngx_strstr(buf, name);
		if (pos == NULL)
			return NGX_ERROR;
	}

	value->data = pos+ngx_strlen(name)+1;
	end = value->data;

	if (skip == 0)
	{
		while (*end!='&' && *end != ' ' && end != (buf + buf_len))
			end++;
	}
	else if (skip == ' ')
	{
		while (*end!='&' && end != (buf + buf_len))
			end++;
	}
	
	value->len = end - value->data;

	return NGX_OK;
}

static ngx_int_t ngx_http_get_value(ngx_http_request_t *r, u_char *pos, u_char *name, ngx_str_t *value)
{
	return ngx_http_get_value_ex(r->args.data, r->args.len, pos, name, value, 0);
}

static ngx_int_t ngx_http_get_url_arg(ngx_http_request_t *r, ngx_array_t **values)
{
	u_char		*pos;
	ngx_str_t	*value;
	
	if (r==NULL || r->args.data==NULL)
		return NGX_ERROR;

	*values = ngx_array_create(r->pool, 16, sizeof(ngx_str_t));
	if (*values == NULL)
		return NGX_ERROR;

	if ((pos=ngx_strstr(r->args.data, ADD_ITEM_STR))!=NULL)
	{
		value = ngx_array_push(*values);
		if (value == NULL)
			return NGX_ERROR;
		
		ngx_http_get_value(r, pos, ADD_ITEM_STR, value);

		value = ngx_array_push(*values);
		if (value == NULL)
			return NGX_ERROR;
		
		pos = ngx_strstr(r->args.data, ZONE_NAME_STR);
		if (pos == NULL)
		{
			value->data = zone_name_is_empty;
			value->len = ngx_strlen(zone_name_is_empty);
			return CONFIG_ERROR;
		}
		ngx_http_get_value(r, pos, ZONE_NAME_STR, value);
		
		return ADD_ITEM;
	}

	if ((pos = ngx_strstr(r->args.data, DELETE_ITEM_STR))!=NULL)
	{
		value = ngx_array_push(*values);
		if (value == NULL)
			return NGX_ERROR;
		
		ngx_http_get_value(r, pos, DELETE_ITEM_STR, value);

		value = ngx_array_push(*values);
		if (value == NULL)
			return NGX_ERROR;
		
		pos = ngx_strstr(r->args.data, ZONE_NAME_STR);
		if (pos == NULL)
		{
			value->data = zone_name_is_empty;
			value->len = ngx_strlen(zone_name_is_empty);
			return CONFIG_ERROR;
		}
		ngx_http_get_value(r, pos, ZONE_NAME_STR, value);
		return DELETE_ITEM;
	}

	if ((pos = ngx_strstr(r->args.data, FOR_EACH_STR)) != NULL)
	{
		value = ngx_array_push(*values);
		if (value == NULL)
			return NGX_ERROR;

		ngx_http_get_value(r, pos, FOR_EACH_STR, value);
		
		return FOR_EACH;
	}

	if ((pos = ngx_strstr(r->args.data, LIMIT_HOST_LIST_STR)) != NULL)
	{
		value = ngx_array_push(*values);
		if (value == NULL)
			return NGX_ERROR;

		ngx_http_get_value(r, pos, LIMIT_HOST_LIST_STR, value);
		
		return LIMIT_HOST_LIST;
	}

	if ((pos = ngx_strstr(r->args.data, ZONE_NAME_STR))!=NULL)
	{
		value = ngx_array_push(*values);
		if (value == NULL)
			return NGX_ERROR;
		
		ngx_http_get_value(r, pos, ZONE_NAME_STR, value);
		return ZONE_NAME;
	}
	
	return NGX_OK;
}

ngx_shm_zone_t *ngx_http_get_shm_zone_by_name(ngx_str_t *zone_name)
{
	ngx_uint_t						n;
	ngx_white_black_array_node_t 	*wb_array_node;

	if (zone_name == NULL)
		return NULL;
	
	if (array_white_black_list)
	{
		wb_array_node = array_white_black_list->elts;
		for (n=0; n<array_white_black_list->nelts; n++)
		{
			if (ngx_memcmp(wb_array_node[n].zone_name->data, zone_name->data, wb_array_node[n].zone_name->len) == 0)
			{
				return wb_array_node[n].shm_zone;
			}
		}
	}
	return NULL;
}

static ngx_int_t ngx_http_config_error(cJSON *root, ngx_array_t *values)
{
	ngx_str_t *value;
	cJSON_AddStringToObject(root, "version", NGINX_VER);
	cJSON_AddStringToObject(root, "code", "-1");
	if (values)
	{
		value = values->elts;	
		cJSON_AddStringToObject(root, "reason", value[values->nelts-1].data);
	}
	return NGX_OK;
}

static ngx_int_t ngx_for_each_rbtree(ngx_rbtree_node_t *rbtree, ngx_rbtree_node_t *sentinel, ngx_array_t *array)
{
	void						**value;

	if (rbtree == sentinel)
		return NGX_OK;
	
	if (rbtree->left != sentinel)
	{
		ngx_for_each_rbtree(rbtree->left, sentinel, array);
	}

	if (rbtree->right != sentinel)
	{
		ngx_for_each_rbtree(rbtree->right, sentinel, array);
	}

	value = ngx_array_push(array);
	if (value == NULL)
		return NGX_ERROR;

	*value = &rbtree->color;
	
	return NGX_OK;
}

#define POW(c) (1<<(c))
#define MASK(c) (((unsigned long)-1) / (POW(POW(c)) + 1))
#define ROUND(n, c) (((n) & MASK(c)) + ((n) >> POW(c) & MASK(c)))

int bit_count(unsigned int n)
{
    n = ROUND(n, 0);
    n = ROUND(n, 1);
    n = ROUND(n, 2);
    n = ROUND(n, 3);
    n = ROUND(n, 4);
    return n;
}


static ngx_int_t ngx_http_get_info_by_zone_name(ngx_http_request_t *r, cJSON *root, ngx_array_t *values)
{
	u_char							out_buf[32];
	u_char							buf_temp[256];
	cJSON							*array;
	ngx_uint_t						n_i, n;
	ngx_str_t						*value;
	ngx_cidr_t 						*cdir;
	ngx_array_t						value_array;
	ngx_shm_zone_t					*shm_zone;
	ngx_slab_pool_t     			*shpool;
	ngx_network_addr_list_t			*p_network_list;
	ngx_network_addr_node_t			*p_network_data;
	ngx_white_black_list_ctx_t      *ctx;
    ngx_white_black_list_node_t     **lc;
	
	value = values->elts;
	shm_zone = ngx_http_get_shm_zone_by_name(value);
	cJSON_AddStringToObject(root, "version", NGINX_VER);
	
	if (shm_zone == NULL)
	{
		cJSON_AddStringToObject(root, "code", "-1");
		cJSON_AddStringToObject(root, "reason", "the zone name is invalid!");
		return NGX_ERROR;
	}
	
	ctx = shm_zone->data;
	
	shpool = (ngx_slab_pool_t *)shm_zone->shm.addr;
	
	if (ngx_array_init(&value_array, r->pool, 16, sizeof(void *)) == NGX_ERROR)
		return NGX_ERROR;

	ngx_memzero(buf_temp, 256);
	
	if (ctx->var.data != NULL)
		ngx_memcpy(buf_temp, ctx->var.data, ctx->var.len);
	
	cJSON_AddStringToObject(root, "file_path", buf_temp);
	
	if (ctx->rbtree != NULL)
		ngx_for_each_rbtree(ctx->rbtree->root, ctx->rbtree->sentinel, &value_array);

	lc = (ngx_white_black_list_node_t **)value_array.elts;
	
	cJSON_AddItemToObject(root, "ip_list", array = cJSON_CreateArray());
	
	for (n_i = 0; n_i < value_array.nelts; n_i++)
	{
		if (lc[n_i]->data == NULL)
			continue;
		
		cdir = (ngx_cidr_t *) &lc[n_i]->data;
		ngx_memzero(out_buf,32);

		if (ngx_inet_ntop(cdir->family, &cdir->u.in.addr, out_buf, 32) != 0)
			cJSON_AddItemToArray(array,cJSON_CreateString(out_buf));
	}

	cJSON_AddItemToObject(root, "net_list", array = cJSON_CreateArray());
	for (p_network_list = ctx->network_list;
		 p_network_list != NULL; 
		 p_network_list = p_network_list->next)
	{
		if (p_network_list->data == NULL)
			continue;

		p_network_data = p_network_list->data;
		ngx_memzero(out_buf, 32);

		if (ngx_inet_ntop(AF_INET, &p_network_data->addr, out_buf, 32) != 0)
		{
			n = bit_count(p_network_data->mask);
			ngx_snprintf(out_buf, 32, "%s/%d", out_buf, n);
			cJSON_AddItemToArray(array,cJSON_CreateString(out_buf));
		}
	}
	
    return NGX_OK;
}

static ngx_int_t ngx_http_get_white_black_zone_list(cJSON *root)
{
	size_t							n;
	char							str_temp[260];
	cJSON							*fmt;
	ngx_white_black_array_node_t 	*wb_array_node;
	
	cJSON_AddStringToObject(root, "version", NGINX_VER);
	cJSON_AddStringToObject(root, "code", "0");
	if (array_white_black_list)
	{
		wb_array_node = array_white_black_list->elts;
		for (n=0; n<array_white_black_list->nelts; n++)
		{
			cJSON_AddItemToObject(root, "item", fmt=cJSON_CreateObject());

			ngx_memzero(str_temp, 260);
			ngx_memcpy(str_temp, wb_array_node[n].conf_type->data, wb_array_node[n].conf_type->len);
			cJSON_AddStringToObject(fmt, "conf_type", str_temp);

			ngx_memzero(str_temp, 260);
			ngx_memcpy(str_temp, wb_array_node[n].zone_name->data, wb_array_node[n].zone_name->len);
			cJSON_AddStringToObject(fmt, "zone_name", str_temp);

			ngx_memzero(str_temp, 260);
			ngx_memcpy(str_temp, wb_array_node[n].conf_path->data, wb_array_node[n].conf_path->len);
			cJSON_AddStringToObject(fmt, "list_path", str_temp);
		}
	}
	return 0;
}

static ngx_int_t ngx_http_white_black_add_item(ngx_http_request_t *r, cJSON *root, ngx_array_t *values)
{
	ngx_str_t						*value, reason;

	value = values->elts;

	cJSON_AddStringToObject(root, "version", NGINX_VER);
	if (ngx_white_black_add_item(r, &value[0], &value[1], &reason) == NGX_OK)
	{
		cJSON_AddStringToObject(root, "code", "0");
	}
	else
	{
		cJSON_AddStringToObject(root, "code", "-1");
		cJSON_AddStringToObject(root, "reason", reason.data);
	}

	return NGX_OK;
}

static ngx_int_t ngx_http_white_black_delete_item(ngx_http_request_t *r, cJSON *root, ngx_array_t *values)
{
	ngx_str_t						*value, reason;

	value = values->elts;

	cJSON_AddStringToObject(root, "version", NGINX_VER);
	if (ngx_white_black_delete_item(r, &value[0], &value[1], &reason) == NGX_OK)
	{
		cJSON_AddStringToObject(root, "code", "0");
	}
	else
	{
		cJSON_AddStringToObject(root, "code", "-1");
		cJSON_AddStringToObject(root, "reason", reason.data);
	}

	return NGX_OK;
}

static ngx_int_t ngx_http_config_module_handler(ngx_http_request_t *r)
{
	size_t             	size;
    ngx_int_t          	rc;
    ngx_buf_t         	*b;
    ngx_chain_t        	out;
	cJSON				*root;
	char				*out_cjson;
	ngx_array_t			*values;

    if (r->method != NGX_HTTP_GET && r->method != NGX_HTTP_HEAD) {
        return NGX_HTTP_NOT_ALLOWED;
    }
	
	root=cJSON_CreateObject();
	
	switch (ngx_http_get_url_arg(r, &values))
	{
		case ZONE_NAME:
			ngx_http_get_info_by_zone_name(r, root, values);
			break;
			
		case ADD_ITEM:
			ngx_http_white_black_add_item(r, root, values);
			break;
			
		case DELETE_ITEM:
			ngx_http_white_black_delete_item(r, root, values);
			break;
			
		case CONFIG_ERROR:
			ngx_http_config_error(root, values);
			break;

		case FOR_EACH:
			ngx_for_each_location(root, r);
			break;

		case LIMIT_HOST_LIST:
			ngx_for_each_limit_host_rate(root, r);
			break;
			
		default:
			ngx_http_get_white_black_zone_list(root);
			break;
	}
	
	rc = ngx_http_discard_request_body(r);

    if (rc != NGX_OK) {
		cJSON_Delete(root);
        return rc;
    }
	
    ngx_str_set(&r->headers_out.content_type, "text/plain");

    if (r->method == NGX_HTTP_HEAD) {
        r->headers_out.status = NGX_HTTP_OK;

        rc = ngx_http_send_header(r);

        if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
			cJSON_Delete(root);
            return rc;
        }
    }

	out_cjson=cJSON_Print(root);  
	cJSON_Delete(root);
	
	size = ngx_strlen(out_cjson);
	b = ngx_create_temp_buf(r->pool, size);
	
    if (b == NULL) {
      if (out_cjson)
		    ngx_free(out_cjson); 
      return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
	out.buf = b;
    out.next = NULL;
	b->last = ngx_cpymem(b->last, out_cjson,
                         size);
	if (out_cjson)
		ngx_free(out_cjson); 

    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = b->last - b->pos;

    b->last_buf = 1;

    rc = ngx_http_send_header(r);

    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }

    return ngx_http_output_filter(r, &out);
}


static char *ngx_http_config_module_set(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
	ngx_conf_file_t   		  *conf_file;
    ngx_http_core_loc_conf_t  *clcf;
	
    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_http_config_module_handler;

	conf_file = ngx_alloc(sizeof(ngx_conf_file_t), cf->log);
	if (conf_file == NULL)
		return NGX_CONF_ERROR;
	
	ngx_memzero(conf_file, sizeof(ngx_conf_file_t));

	conf_file->file.name.data = ngx_cycle->conf_file.data;
	conf_file->file.name.len = ngx_cycle->conf_file.len;
	
	ngx_conf_g.cmd_type = cf->cmd_type;
	ngx_conf_g.conf_file = conf_file;
	ngx_conf_g.ctx = NULL;
	ngx_conf_g.cycle = cf->cycle;
	ngx_conf_g.handler = cf->handler;
	ngx_conf_g.handler_conf = NULL;
	ngx_conf_g.log = &ngx_conf_g.cycle->new_log;
	ngx_conf_g.module_type = cf->module_type;
	ngx_conf_g.name = NULL;
	ngx_conf_g.pool = NULL;
	ngx_conf_g.temp_pool = NULL;
	
    return NGX_CONF_OK;
}

ngx_http_core_loc_conf_t *ngx_get_core_loc_conf_by_index(ngx_uint_t index)
{
	if (locations == NULL)
		return NULL;
	
	ngx_locations *clcf = (ngx_locations *)locations->elts;

	if (clcf == NULL)
		return NULL;

	if (index >= locations->nelts)
		return NULL;
	
	return clcf[index].t;
}

ngx_http_core_srv_conf_t *ngx_get_core_srv_conf_by_index(ngx_uint_t index)
{
	if (locations == NULL)
		return NULL;
	
	ngx_locations *clcf = (ngx_locations *)locations->elts;

	if (clcf == NULL)
		return NULL;

	if (index >= locations->nelts)
		return NULL;
	
	return clcf[index].srv;
}


static ngx_array_t *ngx_http_create_args(ngx_str_t *cmd_name, ngx_str_t *str)
{
	ngx_str_t				*str_value;
	ngx_array_t 			*args_g;
	u_char					*pos, *start;

	if (str==NULL || str->data== NULL)
		return NULL;

	if (ngx_conf_g.temp_pool== NULL)
	{
		ngx_conf_g.temp_pool = ngx_create_pool(ARGS_CONFIG_POOL_SIZE, ngx_conf_g.log);
		if (ngx_conf_g.temp_pool == NULL)
			return NULL;
	}

	ngx_reset_pool(ngx_conf_g.temp_pool);
	
	args_g = ngx_array_create(ngx_conf_g.temp_pool,8, sizeof(ngx_str_t));
	if (args_g == NULL)
	{
		return NULL;
	}

	str_value = ngx_array_push(args_g);
	if (str_value == NULL)
	{
		return NULL;
	}

	str_value->data = cmd_name->data;
	str_value->len = cmd_name->len;
	
	pos = str->data;
	start = str->data;
	for (;;pos++)
	{
		if (*pos == ' ' && pos-start!=0)
		{
			str_value = ngx_array_push(args_g);
			if (str_value)
			{
				str_value->data = start;
				str_value->len = pos - start;
				start = pos;
				
				while(*start == ' ')
				{
					start++;
					pos++;
				}
			}
		}

		if  (pos >= str->data+str->len && pos-start!=0)
		{
			str_value = ngx_array_push(args_g);
			if (str_value)
			{
				str_value->data = start;
				str_value->len = pos - start;
				start = pos+1;
			}
			break;
		}

		if (pos >= str->data+str->len)
		{
			break;
		}
	}
	
	return args_g;
}

static ngx_int_t ngx_http_get_action(ngx_str_t *str)
{
	ngx_int_t	ngx_action = -1;
	if (ngx_memcmp(str->data, "add", str->len) == 0)
	{
		ngx_action = 0;
	}

	if (ngx_memcmp(str->data, "del", str->len) == 0)
	{
		ngx_action = 1;
	}

	if (ngx_memcmp(str->data, "update", str->len) == 0)
	{
		ngx_action = 2;
	}

	return ngx_action;
}

static ngx_int_t ngx_http_secure_config_handle( void* data, ngx_int_t size )
{
	ngx_http_core_loc_conf_t	*clcf = NULL;
	ngx_http_core_srv_conf_t	*cscf = NULL;
	ngx_str_t	 				s_method, s_cmd_name, s_arg, s_where;
	ngx_int_t 					ngx_action = -1, n = 0;
	ngx_array_t 				*args_g;
	u_char						buf[16];
	ngx_int_t					rv = NGX_ERROR;

	s_method.data = 0;
	s_cmd_name.data = 0;
	s_arg.data = 0;
	s_where.data = 0;

	if ( data == NULL)
	{
		return NGX_ERROR; 
	}
	
	if (ngx_conf_g.pool == NULL)
	{
		ngx_conf_g.pool = ngx_create_pool(SECURE_CONFIG_POOL_SIZE, ngx_conf_g.log);
		if (ngx_conf_g.pool == NULL)
			return NGX_ERROR;
	}

	if (ngx_create_locations() == NGX_ERROR)
		return NGX_ERROR;
	
	if (   ngx_http_get_value_ex(data, size, NULL, "method", &s_method, 0) == NGX_OK
		&& ngx_http_get_value_ex(data, size, NULL, "cmd_name", &s_cmd_name, 0) == NGX_OK
		)
	{
		ngx_action = ngx_http_get_action(&s_method);
		
		for (n=0; ngx_conf_update_fun[n].cmd.data!= NULL; n++)
		{	
			if (ngx_conf_update_fun[n].cmd.len != s_cmd_name.len)
				continue;
			
			if (ngx_memcmp(ngx_conf_update_fun[n].cmd.data, s_cmd_name.data, ngx_conf_update_fun[n].cmd.len) == 0)
			{

				if ((ngx_conf_update_fun[n].type & SECURE_CONFIG_USE_ARGV) == SECURE_CONFIG_USE_ARGV)
				{
					/*if (ngx_http_get_value_ex(data, size, NULL, "arg", &s_arg, ' ') != NGX_OK)
						return NGX_ERROR;
					
					args_g = ngx_http_create_args(&s_cmd_name, &s_arg);
					if (args_g == NULL)
						return NGX_ERROR;
					*/

					ngx_http_get_value_ex(data, size, NULL, "arg", &s_arg, ' ');
					args_g = ngx_http_create_args(&s_cmd_name, &s_arg);
					
					ngx_conf_g.args = args_g;
				}

				if ((ngx_conf_update_fun[n].type & SECURE_CONFIG_USE_WHERE) == SECURE_CONFIG_USE_WHERE)
				{
					if (ngx_http_get_value_ex(data, size, NULL, "where", &s_where, 0) == NGX_OK 
						&&s_where.len<15)
					{
						ngx_memzero(buf,16);
						ngx_memcpy(buf,s_where.data, s_where.len);
						if (((ngx_conf_update_fun[n].type & SECURE_CONFIG_USE_LOC) == SECURE_CONFIG_USE_LOC)
							|| ((ngx_conf_update_fun[n].type & SECURE_CONFIG_USE_LOC_M) == SECURE_CONFIG_USE_LOC_M)
							)
						{	
							clcf = ngx_get_core_loc_conf_by_index(ngx_atoi(buf, ngx_strlen(buf)));
							if (clcf == NULL || clcf->loc_conf == NULL)
								return NGX_ERROR;
						}

						if ((ngx_conf_update_fun[n].type & SECURE_CONFIG_USE_SRV) == SECURE_CONFIG_USE_SRV)
						{	
							cscf = ngx_get_core_srv_conf_by_index(ngx_atoi(buf, ngx_strlen(buf)));
							if (cscf == NULL)
								return NGX_ERROR;
						}
					}
					else
					{
						return NGX_ERROR;
					}
				}
				
				switch(ngx_action)
				{
					case 0:
						if (ngx_conf_update_fun[n].add_config
							&& ((ngx_conf_update_fun[n].type & SECURE_CONFIG_USE_LOC_M) == SECURE_CONFIG_USE_LOC_M)
							)
						{
							rv = (ngx_int_t)ngx_conf_update_fun[n].add_config(&ngx_conf_g, NULL, clcf->loc_conf[ngx_conf_update_fun[n].tag->ctx_index]);
						}
						else
						if (ngx_conf_update_fun[n].add_config
							&& ((ngx_conf_update_fun[n].type & SECURE_CONFIG_USE_LOC) == SECURE_CONFIG_USE_LOC)
							)
						{
							rv = (ngx_int_t)ngx_conf_update_fun[n].add_config(&ngx_conf_g, NULL, clcf);
						}
						else
						if (ngx_conf_update_fun[n].add_config
							&& ((ngx_conf_update_fun[n].type & SECURE_CONFIG_USE_SRV) == SECURE_CONFIG_USE_SRV)
							)
						{
							rv = (ngx_int_t)ngx_conf_update_fun[n].add_config(&ngx_conf_g, NULL, cscf);
						}
						else if (ngx_conf_update_fun[n].add_config)
						{
							rv = (ngx_int_t)ngx_conf_update_fun[n].add_config(&ngx_conf_g, NULL, NULL);
						}
						break;
						
					case 1:
						if (ngx_conf_update_fun[n].del_config
							&& ((ngx_conf_update_fun[n].type & SECURE_CONFIG_USE_LOC_M) == SECURE_CONFIG_USE_LOC_M)
							)
						{
							rv = (ngx_int_t)ngx_conf_update_fun[n].del_config(&ngx_conf_g, NULL, clcf->loc_conf[ngx_conf_update_fun[n].tag->ctx_index]);
						}
						else
						if (ngx_conf_update_fun[n].del_config
							&& ((ngx_conf_update_fun[n].type & SECURE_CONFIG_USE_LOC) == SECURE_CONFIG_USE_LOC)
							)
						{
							rv = (ngx_int_t)ngx_conf_update_fun[n].del_config(&ngx_conf_g, NULL, clcf);
						}
						else
						if (ngx_conf_update_fun[n].del_config
							&& ((ngx_conf_update_fun[n].type & SECURE_CONFIG_USE_SRV) == SECURE_CONFIG_USE_SRV)
							)
						{
							rv = (ngx_int_t)ngx_conf_update_fun[n].del_config(&ngx_conf_g, NULL, cscf);
						}
						else if (ngx_conf_update_fun[n].del_config)
						{
							rv = (ngx_int_t)ngx_conf_update_fun[n].del_config(&ngx_conf_g, NULL, NULL);
						}
						break;
						
					case 2:
						if (ngx_conf_update_fun[n].update_config
							&& ((ngx_conf_update_fun[n].type & SECURE_CONFIG_USE_LOC_M) == SECURE_CONFIG_USE_LOC_M)
							)
						{
							rv = (ngx_int_t)ngx_conf_update_fun[n].update_config(&ngx_conf_g, NULL, clcf->loc_conf[ngx_conf_update_fun[n].tag->ctx_index]);
						}
						else
						if (ngx_conf_update_fun[n].update_config
							&& ((ngx_conf_update_fun[n].type & SECURE_CONFIG_USE_LOC) == SECURE_CONFIG_USE_LOC)
							)
						{
							rv = (ngx_int_t)ngx_conf_update_fun[n].update_config(&ngx_conf_g, NULL, clcf);
						}
						else
						if (ngx_conf_update_fun[n].update_config
							&& ((ngx_conf_update_fun[n].type & SECURE_CONFIG_USE_SRV) == SECURE_CONFIG_USE_SRV)
							)
						{
							rv = (ngx_int_t)ngx_conf_update_fun[n].update_config(&ngx_conf_g, NULL, cscf);
						}
						else if (ngx_conf_update_fun[n].update_config)
						{
							rv = (ngx_int_t)ngx_conf_update_fun[n].update_config(&ngx_conf_g, NULL, NULL);
						}
						
						break;
					default:
						break;
				}

				break;
			}
		}
	}
	
	return rv;
}

ngx_rbtree_node_t *
ngx_limit_host_list_lookup(ngx_rbtree_t *rbtree, ngx_str_t *vv,
    uint32_t hash)
{
    ngx_int_t                    rc;
    ngx_rbtree_node_t           *node, *sentinel;
    ngx_host_list_t				**lcn, *plh;

    node = rbtree->root;
    sentinel = rbtree->sentinel;

    while (node != sentinel) {
		
        if (hash < node->key) {
            node = node->left;
            continue;
        }

        if (hash > node->key) {
            node = node->right;
            continue;
        }

        /* hash == node->key */

        lcn = (ngx_host_list_t **) &node->data;
		plh = *lcn;
		rc = ngx_memn2cmp(plh->host.data, vv->data, plh->host.len, vv->len);
        if (rc == 0) {
            return node;
        }

        node = (rc < 0) ? node->left : node->right;
    }

    return NULL;
}

ngx_int_t ngx_http_update_limit_rate(ngx_http_request_t *r)
{
	uint32_t			hash;
	ngx_host_list_t		**temp, *pos;
	ngx_rbtree_node_t	*node;
	
	if (r==NULL || r->headers_in.host == NULL)
		return NGX_ERROR;

	/*find from rbtree*/
	hash = ngx_crc32_short(r->headers_in.host->value.data, r->headers_in.host->value.len);
	node = ngx_limit_host_list_lookup(&l_host_rbtree, &r->headers_in.host->value, hash);
	if (node != NULL)
	{
		temp = (ngx_host_list_t **)&node->data;
		pos = (*temp);
		if (pos != NULL)
		{
			if (r->limit_rate > pos->limit_rate)
				r->limit_rate = pos->limit_rate;
		}
	}
	/*
	for (pos = host_limit_conf.host_list; pos != NULL; pos = pos->next)
	{
		if (r->headers_in.host->value.len != pos->host.len)
			continue;

		if (pos->host.data == NULL)
			continue;
		
		if (ngx_memcmp(r->headers_in.host->value.data, pos->host.data, pos->host.len) == 0)
		{
			r->limit_rate = pos->limit_rate;
			break;
		}
	}
	*/
	return NGX_OK;
}

static char *ngx_http_read_lrc_config(ngx_conf_t *cf, ngx_limit_rate_conf_t *h_limit_conf)
{
	FILE				*fd;
	u_char				path[1024] = {0};
	cJSON 				*json, *json_c;
	ngx_host_list_t		*host_list, *p_data;
	ngx_rbtree_node_t	*new_node;
	
	if (h_limit_conf == NULL
		|| h_limit_conf->path_conf.data == NULL)
	{
		fprintf(stderr, "limit_host_conf path is NULL\n");
		return NGX_CONF_ERROR;
	}

	if (h_limit_conf->path_conf.len > 1023)
	{
		fprintf(stderr, "limit_host_conf, the len of path > 1024\n");
		return NGX_CONF_ERROR;
	}
	
	ngx_memcpy(path, h_limit_conf->path_conf.data, h_limit_conf->path_conf.len);
	fd = fopen(path,"rb");
	if (fd == NULL)
		return NGX_CONF_OK;
	
	fseek(fd,0,SEEK_END);long len=ftell(fd);
	if (len < 2)
	{
		ngx_log_error(NGX_LOG_WARN, cf->log, 0, "the limit_host_conf if empty!");
		return NGX_CONF_OK;
	}
	
	char *data=malloc(len+1);
	if (data == NULL )
	{
		fclose(fd);
		fprintf(stderr, "ngx_http_read_lrc_config->malloc failed!\n");
		return NGX_CONF_ERROR;
	}
	
	fseek(fd,0,SEEK_SET);
	len = fread(data,1,len,fd);fclose(fd);

	json=cJSON_Parse(data);
	if (!json) {
		ngx_free(data);
		fprintf(stderr, "ngx_http_read_lrc_config->cJSON_Parse failed!\n");
		return NGX_CONF_ERROR;
	}
	else
	{
		json_c = cJSON_GetObjectItem(json, "host_limit");
		if (json_c != NULL && json_c->child != NULL)
		{
			json_c = json_c->child;
			for (;json_c !=NULL; json_c = json_c->next)
			{
				/*printf("%s----%d\n", json_c->string, json_c->valueint);*/
				host_list = ngx_pcalloc(cf->pool, sizeof(ngx_host_list_t));
				if (host_list == NULL)
				{
					fprintf(stderr, "ngx_http_read_lrc_config-->ngx_pcalloc failed!\n");
					return NGX_CONF_ERROR;
				}
				host_list->limit_rate = json_c->valueint;
				host_list->next = NULL;
				host_list->host.len = ngx_strlen(json_c->string);
				host_list->host.data = ngx_pcalloc(cf->pool, host_list->host.len);
				ngx_memcpy(host_list->host.data, json_c->string, host_list->host.len);

				/*put to the limit_host_list*/
				ngx_put_limit_host_list(host_list);

				/*insert to rbtree*/
				new_node = ngx_pcalloc(cf->pool, sizeof(ngx_rbtree_node_t)+sizeof(void *)-1);
				if (new_node == NULL)
				{
					fprintf(stderr, "ngx_http_read_lrc_config-->ngx_pcalloc failed!\n");
					return NGX_CONF_ERROR;
				}
				
				p_data = (ngx_host_list_t *)&new_node->data;
				ngx_memcpy(p_data, &host_list, sizeof(void *));
				new_node->key = ngx_crc32_short(host_list->host.data, host_list->host.len);
				ngx_rbtree_insert(&l_host_rbtree, new_node);
			}
		}
		cJSON_Delete(json);
	}

	ngx_free(data);
	
	return NGX_CONF_OK;
}

static void
ngx_l_host_rbtree_insert_value(ngx_rbtree_node_t *temp,
    ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel)
{
	ngx_host_list_t					  **lcn, **lcnt, *lcn_t, *lcnt_t;
	ngx_rbtree_node_t           	  **p;

    for ( ;; ) {

        if (node->key < temp->key) {

            p = &temp->left;

        } else if (node->key > temp->key) {

            p = &temp->right;

        } else { /* node->key == temp->key */

            lcn = (ngx_host_list_t **) &node->data;
            lcnt = (ngx_host_list_t **) &temp->data;

			lcn_t = *lcn;
			lcnt_t = *lcnt;
            p = (ngx_memn2cmp(lcn_t->host.data, lcnt_t->host.data, lcn_t->host.len, lcnt_t->host.len) < 0)
                ? &temp->left : &temp->right;
        }

        if (*p == sentinel) {
            break;
        }

        temp = *p;
    }

    *p = node;
    node->parent = temp;
    node->left = sentinel;
    node->right = sentinel;
    ngx_rbt_red(node);
}

static char *ngx_http_limit_rate_config_set(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
	ngx_str_t				*value;
	ngx_rbtree_node_t       *sentinel;
	
	if (cf->args == NULL
		|| cf->args->nelts < 2)
		return NGX_CONF_ERROR;

	value = cf->args->elts;

	if (value == NULL
		|| value->data == NULL)
		return NGX_CONF_ERROR;
	
	if (ngx_conf_full_name(cf->cycle, &value[1], 0) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

	host_limit_conf.path_conf.len= value[1].len;
	host_limit_conf.path_conf.data = ngx_pcalloc(cf->pool, value[1].len);
	if (host_limit_conf.path_conf.data == NULL)
		return NGX_CONF_ERROR;
	
	ngx_memcpy(host_limit_conf.path_conf.data, value[1].data, value[1].len);

	/*init rbtree*/
	sentinel = ngx_pcalloc(cf->pool, sizeof(ngx_rbtree_node_t));
	if (sentinel == NULL)
	{
		fprintf(stderr, "ngx_http_limit_rate_config_set->ngx_pcalloc failed!\n");
		return NGX_CONF_ERROR;
	}
	ngx_rbtree_init(&l_host_rbtree, sentinel,
                    ngx_l_host_rbtree_insert_value);
	/*
	parese limit_rate_conf
	*/
	host_limit_conf.host_list = NULL;
	return ngx_http_read_lrc_config(cf, &host_limit_conf);
	
	return NGX_CONF_OK;
}

