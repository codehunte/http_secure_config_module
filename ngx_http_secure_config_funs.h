
#ifndef _NGX_HTTP_SECURE_CONFIG_FUNS_INCLUDED_
#define _NGX_HTTP_SECURE_CONFIG_FUNS_INCLUDED_

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <cjson.h>
#include <ngx_white_black_list.h>
#include <ngx_http_secure_config_module.h>

extern ngx_array_t	*locations;
extern ngx_conf_t    ngx_conf_g;
extern ngx_int_t	 locations_need_update;
extern ngx_module_t  ngx_http_limit_req_module;
extern ngx_module_t  ngx_http_limit_conn_module;
extern ngx_module_t	 ngx_white_black_list_module;
extern ngx_module_t	 ngx_http_limit_conn_module;
extern ngx_module_t  ngx_http_log_module;
extern ngx_rbtree_t  l_host_rbtree;

/*limit_host_rate   host list*/
extern ngx_limit_rate_conf_t	host_limit_conf;

/*ngx_http_limit_req_ctx_t_p is 
  ngx_http_limit_req_ctx_t
*/
typedef struct {
    void					    *sh;
    ngx_slab_pool_t             *shpool;
    /* integer value, 1 corresponds to 0.001 r/s */
    ngx_uint_t                   rate;
    ngx_int_t                    index;
    ngx_str_t                    var;
    void					    *node;
} ngx_http_limit_req_ctx_t_p;

/*ngx_http_limit_req_limit_t_p is 
  ngx_http_limit_req_limit_t
*/
typedef struct {
    ngx_shm_zone_t              *shm_zone;
    /* integer value, 1 corresponds to 0.001 r/s */
    ngx_uint_t                   burst;
    ngx_uint_t                   nodelay; /* unsigned  nodelay:1 */
} ngx_http_limit_req_limit_t_p;

/*ngx_http_limit_req_conf_t_p is 
  ngx_http_limit_req_conf_t
*/
typedef struct {
    ngx_array_t                  limits;
    ngx_uint_t                   limit_log_level;
    ngx_uint_t                   delay_log_level;
} ngx_http_limit_req_conf_t_p;


/*ngx_http_limit_conn_limit_t_p is 
  ngx_http_limit_conn_limit_t
*/
typedef struct {
    ngx_shm_zone_t     *shm_zone;
    ngx_uint_t          conn;
} ngx_http_limit_conn_limit_t_p;

/*ngx_http_limit_conn_conf_t_p is 
  ngx_http_limit_conn_conf_t
*/
typedef struct {
    ngx_array_t         limits;
    ngx_uint_t          log_level;
} ngx_http_limit_conn_conf_t_p;


/*ngx_http_limit_conn_ctx_t_p is 
  ngx_http_limit_conn_ctx_t
*/
typedef struct {
    ngx_rbtree_t       *rbtree;
    ngx_int_t           index;
    ngx_str_t           var;
} ngx_http_limit_conn_ctx_t_p;



char *ngx_limit_req_zone_config_update_interface(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
char *ngx_limit_req_config_update(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
char *ngx_limit_req_config_del(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

char *ngx_limit_conn_zone_config_update(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
char *ngx_limit_conn_config_update(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
char *ngx_limit_conn_config_del(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

char *ngx_limit_rate_config_update(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
char *ngx_limit_rate_config_del(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

char *ngx_http_white_black_list_del(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

char *ngx_location_update(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
char *ngx_location_add(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
char *ngx_location_del(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

char *ngx_http_private_root(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

char *ngx_http_lrlc_add(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
char *ngx_http_lrlc_del(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

#endif
