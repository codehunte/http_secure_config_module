
#ifndef _NGX_HTTP_SECURE_CONFIG_INCLUDED_
#define _NGX_HTTP_SECURE_CONFIG_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <nginx.h>


typedef struct {
	ngx_http_core_loc_conf_t	*t;
	ngx_http_core_srv_conf_t    *srv;
	ngx_http_core_main_conf_t	*main;
	ngx_str_t					addr_text;
	
	ngx_str_t					name;	/*server name*/
}ngx_locations;


struct ngx_host_list_s{
	ngx_str_t					host;
	ngx_uint_t					limit_rate;
	struct ngx_host_list_s		*next;	
	struct ngx_host_list_s		*prev;
};

typedef struct ngx_host_list_s ngx_host_list_t;


typedef struct {
	ngx_str_t					path_conf;
	ngx_host_list_t				*host_list;
}ngx_limit_rate_conf_t;

ngx_int_t ngx_create_locations();
ngx_int_t ngx_http_update_limit_rate(ngx_http_request_t *r);
ngx_rbtree_node_t *ngx_limit_host_list_lookup(ngx_rbtree_t *rbtree, ngx_str_t *vv, uint32_t hash);


#endif

