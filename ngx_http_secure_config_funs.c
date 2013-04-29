/*
 * Copyright (C) 
 * Copyright (C) 
 * author:      	wu yangping
 * create time:		20120600
 * update time: 	20120727
 */


#include <ngx_http_secure_config_funs.h>
#include <ngx_http_secure_config_module.h>

ngx_int_t		first = 1;

ngx_int_t ngx_shm_zone_isexist(ngx_conf_t *cf, ngx_str_t *name, void *tag)
{
	ngx_uint_t				  i;
    ngx_shm_zone_t            *shm_zone=NULL, *shm_zone_e=NULL;
    ngx_list_part_t  		  *part=NULL;	

    part = &cf->cycle->shared_memory.part;
    shm_zone_e = part->elts;

    for (i = 0; /* void */ ; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }
            part = part->next;
            shm_zone_e = part->elts;
            i = 0;
        }

		if (tag != shm_zone_e[i].tag) {
            continue;
        }
	
        if (name->len != shm_zone_e[i].shm.name.len) {
            continue;
        }

        if (ngx_strncmp(name->data, shm_zone_e[i].shm.name.data, name->len)
            != 0)
        {
            continue;
        }

        shm_zone = &shm_zone_e[i];
		return 1;
    }	

	return 0;
}

char *ngx_limit_req_zone_config_update_interface(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
	void					  *tag = &ngx_http_limit_req_module;
	u_char                    *p;
    size_t                     len;
    ssize_t                    size;
    ngx_str_t                 *value, name;
    ngx_int_t                  rate, scale;
    ngx_uint_t                 i;
    ngx_shm_zone_t            *shm_zone, *shm_zone_e;
    ngx_list_part_t  		  *part;
	
    ngx_http_limit_req_ctx_t_p  *ctx, *octx;

	if (cf->args==NULL || cf->args->elts == NULL)
		return NGX_CONF_ERROR;
	
    value = cf->args->elts;

    ctx = NULL;
    size = 0;
    rate = 1;
    scale = 1;
    name.len = 0;

	ctx = (ngx_http_limit_req_ctx_t_p *)malloc(sizeof(ngx_http_limit_req_ctx_t_p));
    if (ctx == NULL) {
        return NGX_CONF_ERROR;
    }
	
    for (i = 1; i < cf->args->nelts; i++) {

        if (ngx_strncmp(value[i].data, "zone=", 5) == 0) {

            name.data = value[i].data + 5;

            name.len = value[i].len - 5;

            continue;
        }

        if (ngx_strncmp(value[i].data, "rate=", 5) == 0) {

            len = value[i].len;
            p = value[i].data + len - 3;

            if (ngx_strncmp(p, "r/s", 3) == 0) {
                scale = 1;
                len -= 3;

            } else if (ngx_strncmp(p, "r/m", 3) == 0) {
                scale = 60;
                len -= 3;
            }

            rate = ngx_atoi(value[i].data + 5, len - 5);
            if (rate <= NGX_ERROR) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid rate \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }

            continue;
        }

        if (value[i].data[0] == '$') {
			
            continue;
        }

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid parameter \"%V\"", &value[i]);
		ngx_free(ctx);
        return NGX_CONF_ERROR;
    }

    if (name.len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "\"%V\" must have \"zone\" parameter",
                           &cmd->name);
		ngx_free(ctx);
        return NGX_CONF_ERROR;
    }

    if (ctx == NULL) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "no variable is defined for %V \"%V\"",
                           &cmd->name, &name);
		ngx_free(ctx);
        return NGX_CONF_ERROR;
    }

    ctx->rate = rate * 1000 / scale;

    shm_zone = NULL;

    part = &cf->cycle->shared_memory.part;
    shm_zone_e = part->elts;

    for (i = 0; /* void */ ; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }
            part = part->next;
            shm_zone_e = part->elts;
            i = 0;
        }

		if (tag != shm_zone_e[i].tag) {
            continue;
        }
	
        if (name.len != shm_zone_e[i].shm.name.len) {
            continue;
        }

        if (ngx_strncmp(name.data, shm_zone_e[i].shm.name.data, name.len)
            != 0)
        {
            continue;
        }

        shm_zone = &shm_zone_e[i];
		break;
    }

	if (shm_zone == NULL) {
		ngx_free(ctx);
        return NGX_CONF_ERROR;
    }

    if (shm_zone->data) {
        octx = shm_zone->data;

		/*update*/
		octx->rate = ctx->rate;

		ngx_free(ctx);
        return NGX_CONF_OK;
    }
	
	ngx_free(ctx);
    return NGX_CONF_ERROR;
}

char *ngx_limit_req_config_update(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
	ngx_http_limit_req_conf_t_p  *lrcf = conf;

    ngx_int_t                    burst;
    ngx_str_t                   *value, s;
    ngx_uint_t                   i, nodelay;
    ngx_shm_zone_t              *shm_zone;
    ngx_http_limit_req_limit_t_p  *limit, *limits;

	if (cf->args==NULL || cf->args->elts == NULL || lrcf == NULL)
		return NGX_CONF_ERROR;
	
    value = cf->args->elts;

    shm_zone = NULL;
    burst = 0;
    nodelay = 0;

    for (i = 1; i < cf->args->nelts; i++) {

        if (ngx_strncmp(value[i].data, "zone=", 5) == 0) {

            s.len = value[i].len - 5;
            s.data = value[i].data + 5;

			if (ngx_shm_zone_isexist(cf, &s, &ngx_http_limit_req_module) == 0){
				return NGX_CONF_ERROR;
			}

            shm_zone = ngx_shared_memory_add(cf, &s, 0,
                                             &ngx_http_limit_req_module);
            if (shm_zone == NULL) {
                return NGX_CONF_ERROR;
            }

            continue;
        }

        if (ngx_strncmp(value[i].data, "burst=", 6) == 0) {

            burst = ngx_atoi(value[i].data + 6, value[i].len - 6);
            if (burst <= 0) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid burst rate \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }

            continue;
        }

        if (ngx_strncmp(value[i].data, "nodelay", 7) == 0) {
            nodelay = 1;
            continue;
        }

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid parameter \"%V\"", &value[i]);
        return NGX_CONF_ERROR;
    }

    if (shm_zone == NULL) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "\"%V\" must have \"zone\" parameter",
                           &cmd->name);
        return NGX_CONF_ERROR;
    }

    if (shm_zone->data == NULL) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "unknown limit_req_zone \"%V\"",
                           &shm_zone->shm.name);
        return NGX_CONF_ERROR;
    }

    limits = lrcf->limits.elts;

    if (limits == NULL) {
        if (ngx_array_init(&lrcf->limits, cf->pool, 1,
                           sizeof(ngx_http_limit_req_limit_t_p))
            != NGX_OK)
        {
            return NGX_CONF_ERROR;
        }
    }

    for (i = 0; i < lrcf->limits.nelts; i++) {
        if (shm_zone == limits[i].shm_zone) {
			/*update*/
			limits[i].burst = burst * 1000;
			limits[i].nodelay = nodelay;
            return NGX_CONF_OK;
        }
    }

    limit = ngx_array_push(&lrcf->limits);

    limit->shm_zone = shm_zone;
    limit->burst = burst * 1000;
    limit->nodelay = nodelay;

    return NGX_CONF_OK;
}

char *ngx_limit_conn_zone_config_update(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
	if (cf->args==NULL || cf->args->elts == NULL)
		return NGX_CONF_ERROR;
	
	return NGX_CONF_OK;
}

char *ngx_limit_conn_config_update(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
	ngx_shm_zone_t               	*shm_zone;
    ngx_http_limit_conn_conf_t_p 	*lccf = conf;
    ngx_http_limit_conn_limit_t_p	*limit, *limits;

    ngx_str_t  *value;
    ngx_int_t   n;
    ngx_uint_t  i;

	if (cf->args==NULL || cf->args->elts == NULL || lccf == NULL)
		return NGX_CONF_ERROR;

	if (cf->args->nelts < 3)
		return NGX_CONF_ERROR;
	
    value = cf->args->elts;

	if (ngx_shm_zone_isexist(cf, &value[1], &ngx_http_limit_conn_module) == 0) {
		return NGX_CONF_ERROR;
	}

    shm_zone = ngx_shared_memory_add(cf, &value[1], 0,
                                     &ngx_http_limit_conn_module);
    if (shm_zone == NULL) {
        return NGX_CONF_ERROR;
    }

    limits = lccf->limits.elts;

    if (limits == NULL) {
        if (ngx_array_init(&lccf->limits, cf->pool, 1,
                           sizeof(ngx_http_limit_conn_limit_t_p))
            != NGX_OK)
        {
            return NGX_CONF_ERROR;
        }
    }

	n = ngx_atoi(value[2].data, value[2].len);
    if (n <= 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid number of connections \"%V\"", &value[2]);
        return NGX_CONF_ERROR;
    }

    if (n > 65535) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "connection limit must be less 65536");
        return NGX_CONF_ERROR;
    }
	
    for (i = 0; i < lccf->limits.nelts; i++) {
        if (shm_zone == limits[i].shm_zone) {

			/*update*/

			limits[i].conn = n;
            return NGX_CONF_OK;
        }
    }

    limit = ngx_array_push(&lccf->limits);
    limit->conn = n;
    limit->shm_zone = shm_zone;

    return NGX_CONF_OK;
}

char *ngx_limit_rate_config_update(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
	ssize_t           			sp;
	ngx_str_t  					*value;
	ngx_http_core_loc_conf_t	*clcf = conf;

	if (cf->args==NULL || cf->args->elts == NULL)
		return NGX_CONF_ERROR;

	if (clcf == NULL)
		return NGX_CONF_ERROR;

	if (cf->args->nelts < 2)
		return NGX_CONF_ERROR;
	
	value = cf->args->elts;

	sp = ngx_parse_size(&value[1]);

	if (sp <= 0 || sp == NGX_ERROR) 
	{
		return NGX_CONF_ERROR;
	}

	clcf->limit_rate = sp;
	
	return NGX_CONF_OK;
}


char *ngx_limit_rate_config_del(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
	ngx_http_core_loc_conf_t	*clcf = conf;
	if (clcf == NULL)
		return NGX_CONF_ERROR;

	clcf->limit_rate = 0;

	return NGX_CONF_OK;
}

char *ngx_http_white_black_list_del(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
	ngx_uint_t						n, m;
	ngx_str_t						*value;
	ngx_white_black_list_conf_t  	*lccf = conf;
    ngx_white_black_list_isvalid_t 	*valids;

	if (cf->args==NULL || cf->args->nelts ==0 || lccf == NULL)
		return NGX_CONF_ERROR;
	
	value = cf->args->elts;
	if (cf->args->nelts < 2)
		return NGX_CONF_ERROR;
	
	valids = lccf->valids.elts;
	if (valids == NULL)
		return NGX_CONF_ERROR;

	for (n=0; n < lccf->valids.nelts; n++)
	{
		if (valids[n].shm_zone == NULL)
			continue;

		if (value[1].len != valids[n].shm_zone->shm.name.len)
			continue;

		if (valids[n].iswhite && 
			(ngx_memcmp(value[0].data, "white_list", value[0].len)!=0))
			continue;

		if (!valids[n].iswhite && 
			(ngx_memcmp(value[0].data, "black_list", value[0].len)!=0))
			continue;
		
		if (ngx_memcmp(valids[n].shm_zone->shm.name.data, value[1].data, valids[n].shm_zone->shm.name.len) == 0)
		{

			/*start delete*/
			if (n == lccf->valids.nelts-1)
			{
				valids[lccf->valids.nelts-1].delete =1;
				lccf->valids.nelts--;
				return NGX_CONF_OK;	
			}

			for (m=n; m<lccf->valids.nelts-1; m++)
			{
				valids[m].isvalid = valids[m+1].isvalid;
				valids[m].iswhite = valids[m+1].iswhite;
				valids[m].shm_zone = valids[m+1].shm_zone;
			}
			valids[lccf->valids.nelts-1].delete =1;

			lccf->valids.nelts--;
			return NGX_CONF_OK;
		}
	}
	return NGX_CONF_ERROR;
}

char *ngx_limit_req_config_del(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
	ngx_uint_t					 n, m;
	ngx_str_t					 *value, zone_name;
	ngx_http_limit_req_conf_t_p  *lrcf = conf;
	ngx_http_limit_req_limit_t_p *limits;

	zone_name.data =0;
	if (lrcf == NULL)
		return NGX_CONF_ERROR;

	if (cf->args == NULL || cf->args->elts == NULL)
		return NGX_CONF_ERROR;

	value = cf->args->elts;
	for (n=0 ; n<cf->args->nelts; n++)
	{
		if (ngx_memcmp(value[n].data, "zone=", 5) == 0)
		{
			zone_name.data = value[n].data+5;
			zone_name.len = value[n].len-5;
			break;
		}
	}

	if (zone_name.data == 0)
		return NGX_CONF_ERROR;
	
	limits = lrcf->limits.elts;

	for (n=0; n<lrcf->limits.nelts; n++)
	{
		/*zone=*/
		if (limits[n].shm_zone->shm.name.len != zone_name.len)
			continue;
		
		if (ngx_memcmp(limits[n].shm_zone->shm.name.data, zone_name.data, zone_name.len) == 0)
		{
			/*start delete*/
			if (n == lrcf->limits.nelts-1)
			{
				lrcf->limits.nelts--;
				break;
			}

			for (m=n; m<lrcf->limits.nelts-1; m++)
			{
				limits[m].burst = limits[m+1].burst;
				limits[m].nodelay = limits[m+1].nodelay;
				limits[m].shm_zone = limits[m+1].shm_zone;
			}

			lrcf->limits.nelts--;
			break;
		}
	}
	
	return NGX_CONF_OK;
}


char *ngx_limit_conn_config_del(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
	ngx_uint_t					 	n, m;
	ngx_str_t					 	*value;
    ngx_http_limit_conn_conf_t_p 	*lccf = conf;
    ngx_http_limit_conn_limit_t_p	*limits;

	if (lccf == NULL)
		return NGX_CONF_ERROR;

	if (cf->args == NULL || cf->args->elts == NULL)
		return NGX_CONF_ERROR;

	value = cf->args->elts;
	if(cf->args->nelts < 2)
		return NGX_CONF_ERROR;

	limits = lccf->limits.elts;

	for (n =0; n<lccf->limits.nelts; n++)
	{
		if (limits[n].shm_zone->shm.name.len != value[1].len)
			continue;
		
		if (ngx_memcmp(limits[n].shm_zone->shm.name.data, value[1].data, value[1].len) == 0)
		{
			/*start delete*/
			if (n == lccf->limits.nelts-1)
			{
				lccf->limits.nelts--;
				break;
			}

			for (m=n; m<lccf->limits.nelts-1; m++)
			{
				limits[m].conn = limits[m+1].conn;
				limits[m].shm_zone = limits[m+1].shm_zone;
			}

			lccf->limits.nelts--;
			break;
		}
	}
	
	return NGX_CONF_OK;
}

void *ngx_get_srv_ctx()
{
	ngx_http_core_srv_conf_t  *cscf;
	ngx_listening_t           *ls;
	ngx_http_in_addr_t		  *hia;
	ngx_http_port_t			  *pots;
	
	ls = ngx_cycle->listening.elts;

	if (ls == NULL || ls[0].servers == NULL)
		return NULL;
	
	pots = ls[0].servers;
	if (pots == NULL || pots[0].addrs == NULL)
		return NULL;

	hia = pots[0].addrs;
	if (hia == NULL)
		return NULL;
	
	cscf = hia[0].conf.default_server;
	if (cscf != NULL)
		return cscf->ctx;
	
	return NULL;
}

ngx_int_t ngx_location_exist(ngx_http_core_srv_conf_t *cscf, ngx_str_t *name)
{
	ngx_uint_t					number, can;
	ngx_locations			  	*value;

	can = 0;
	if (name == NULL || name->len == 0)
		return NGX_ERROR;

	if (ngx_create_locations() == NGX_ERROR)
		return NGX_ERROR;
	
	if (locations==NULL || locations->elts == NULL)
		return NGX_ERROR;
	
	value = (ngx_locations *)locations->elts;
	for (number=0; number < locations->nelts; number++)
	{
		if (value[number].srv!=NULL && value[number].srv == cscf)
			can = 1;
		else
		if (value[number].srv!=NULL && value[number].srv != cscf)
			can = 0;
		
		if (can != 1)
			continue;
		
		if (value[number].t == NULL)
			continue;
		
		if (value[number].t->name.len != name->len)
			continue;

		if (ngx_memcmp(value[number].t->name.data, name->data, name->len) == 0)
			return NGX_OK;
	}
	
	return NGX_ERROR;
}

ngx_int_t ngx_location_add_regex(ngx_http_core_loc_conf_t  *clcf, ngx_http_core_srv_conf_t *cscf)
{	
	ngx_int_t					number;
	ngx_http_core_loc_conf_t	*ptoinsert, **preglocation, **newclcfp;

	if (clcf == NULL)
		return NGX_ERROR;
	
	if (cscf == NULL || 
		cscf->ctx==NULL ||
		cscf->ctx->loc_conf == NULL)
		return NGX_ERROR;
	
	ptoinsert = cscf->ctx->loc_conf[ngx_http_core_module.ctx_index];
	if (ptoinsert == NULL)
		return NGX_ERROR;

	preglocation = ptoinsert->regex_locations;

	if (preglocation == NULL)
	{
		newclcfp = ngx_pcalloc(ngx_conf_g.pool,
                      2 * sizeof(ngx_http_core_loc_conf_t **));

		if (newclcfp == NULL)
			return NGX_ERROR;

		newclcfp[0] = clcf;
		ptoinsert->regex_locations = newclcfp;
		locations_need_update = 1;
		return NGX_OK;
	}

	number = 0;
	while(preglocation[number]!=NULL)
	{
		if (preglocation[number]->name.len == clcf->name.len
			&& ngx_memcmp(preglocation[number]->name.data, clcf->name.data, preglocation[number]->name.len) == 0)
			return NGX_OK;
		number++;
	}
	
	newclcfp = ngx_pcalloc(ngx_conf_g.pool,
                      (number + 2) * sizeof(ngx_http_core_loc_conf_t **));

	if (newclcfp == NULL)
		return NGX_ERROR;
	
	ngx_memcpy(newclcfp, preglocation, number * sizeof(ngx_http_core_loc_conf_t **));

	newclcfp[number] = clcf;
	
	ptoinsert->regex_locations = newclcfp;
	locations_need_update = 1;
	return NGX_OK;
}

char *
ngx_http_private_root(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *clcf = conf;

    ngx_str_t                  *value;
    ngx_int_t                   alias;
    ngx_uint_t                  n;
    ngx_http_script_compile_t   sc;

	if (cf->args==NULL || cf->args->elts==NULL)
		return NGX_CONF_ERROR;

	/*is't have handler function?*/
	if (cf->handler != NULL)
		return NGX_CONF_ERROR;

	value = cf->args->elts;
    alias = (value[0].len == sizeof("alias") - 1) ? 1 : 0;
	
    if (clcf->named && alias) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "the \"alias\" directive cannot be used "
                           "inside the named location");

        return NGX_CONF_ERROR;
    }

    if (ngx_strstr(value[1].data, "$document_root")
        || ngx_strstr(value[1].data, "${document_root}"))
    {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "the $document_root variable cannot be used "
                           "in the \"%V\" directive",
                           &value[0]);

        return NGX_CONF_ERROR;
    }

    if (ngx_strstr(value[1].data, "$realpath_root")
        || ngx_strstr(value[1].data, "${realpath_root}"))
    {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "the $realpath_root variable cannot be used "
                           "in the \"%V\" directive",
                           &value[0]);

        return NGX_CONF_ERROR;
    }

    clcf->alias = alias ? clcf->name.len : 0;
	
	if (clcf->root.len >= value[1].len)
	{
		ngx_memzero(clcf->root.data, clcf->root.len);
		ngx_memcpy(clcf->root.data, value[1].data, value[1].len);
		clcf->root.len = value[1].len;
	}
	else
	{
		clcf->root.data = ngx_pcalloc(cf->pool, value[1].len);
		ngx_memcpy(clcf->root.data, value[1].data, value[1].len);
		clcf->root.len = value[1].len;
	}

    if (!alias && clcf->root.data[clcf->root.len - 1] == '/') {
        clcf->root.len--;
    }

    if (clcf->root.data[0] != '$') {
        if (ngx_conf_full_name(cf->cycle, &clcf->root, 0) != NGX_OK) {
            return NGX_CONF_ERROR;
        }
    }

    n = ngx_http_script_variables_count(&clcf->root);

    ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));
    sc.variables = n;

#if (NGX_PCRE)
    if (alias && clcf->regex) {
        n = 1;
    }
#endif

    if (n) {
        sc.cf = cf;
        sc.source = &clcf->root;
        sc.lengths = &clcf->root_lengths;
        sc.values = &clcf->root_values;
        sc.complete_lengths = 1;
        sc.complete_values = 1;

        if (ngx_http_script_compile(&sc) != NGX_OK) {
            return NGX_CONF_ERROR;
        }
    }

    return NGX_CONF_OK;
}

ngx_http_location_tree_node_t *
ngx_http_private_find_neast_location(ngx_http_location_tree_node_t *node, ngx_str_t *loc_name, ngx_int_t *new_node_len, ngx_int_t *new_node_len_1)
{
    u_char     						*uri;
    size_t      					len, n;
    ngx_int_t   					rc;
	ngx_http_location_tree_node_t	*node_ret = NULL;

    len = loc_name->len;
    uri = loc_name->data;
	*new_node_len = len;		/*(new node)->len*/
	*new_node_len_1 = len;		/**/

    for ( ;; ) {

        if (node == NULL) {
            return node_ret;
        }

        n = (len <= (size_t) node->len) ? len : node->len;

        rc = ngx_filename_cmp(uri, node->name, n);

        if (rc != 0) {
			node_ret = node;
            node = (rc < 0) ? node->left : node->right;
            continue;
        }

        if (len > (size_t) node->len) {

			node_ret = node;
			
            if (node->inclusive) {
				
                node = node->tree;
                uri += n;
                len -= n;
				*new_node_len = len;
				if (node != NULL)
					*new_node_len_1 = len;
				
                continue;
            }

            /* exact only */

            node = node->right;
			
            continue;
        }

        if (len == (size_t) node->len) {

            if (node->exact) {
                return NULL;
            } else {
                node_ret = node;
                return node_ret;
            }
        }

		if (len < node->len)
		{
			/*I want add the new node, include the node?*/
			if (ngx_filename_cmp(uri, node->name, len) == 0)
			{
				return node_ret;
			}
			
			node_ret = node;
		}

        node = node->left;
    }

	return node_ret;
}


ngx_int_t ngx_location_add_static(ngx_http_core_loc_conf_t  *clcf, ngx_http_core_srv_conf_t *cscf)
{
	ngx_int_t								node_name_len, node_name_len_a, len_temp, rv;
	ngx_http_core_loc_conf_t				*clcf_temp;
	ngx_http_location_tree_node_t			*neaset_node, *new_node;

	rv = NGX_ERROR;

	if (cscf == NULL 
		|| cscf->ctx == NULL
		|| cscf->ctx->loc_conf == NULL)
		return NGX_ERROR;
	
	clcf_temp = cscf->ctx->loc_conf[ngx_http_core_module.ctx_index];
	
	if (clcf_temp == NULL || clcf_temp->static_locations == NULL)
		return NGX_ERROR;

	if (clcf->name.len == 0)
		return NGX_ERROR;

	neaset_node = ngx_http_private_find_neast_location(clcf_temp->static_locations, &clcf->name, &node_name_len, &node_name_len_a);

	if (neaset_node == NULL)
	{
		if (clcf->name.len > clcf_temp->static_locations->len)
			return NGX_ERROR;
		
		if (ngx_filename_cmp(clcf->name.data, clcf_temp->static_locations->name, clcf->name.len) == 0)
		{
			/*i'm root node*/
			/*too complex, do't deal with*/
			return NGX_ERROR;
		}
	}

	/*you can insert now!*/

	new_node = ngx_pcalloc(ngx_conf_g.pool, sizeof(ngx_http_location_tree_node_t)+node_name_len-1);
	if (new_node == NULL)
		return NGX_ERROR;

	new_node->left = NULL;
	new_node->right= NULL;
	new_node->tree = NULL;
	new_node->exact = NULL;
	new_node->inclusive = NULL;
	new_node->auto_redirect = 1;
	new_node->len = node_name_len;
	ngx_memcpy(&new_node->name, clcf->name.data+clcf->name.len-node_name_len, node_name_len);
	
	if ( neaset_node->len < node_name_len_a
	  && ngx_filename_cmp(neaset_node->name, clcf->name.data+clcf->name.len-node_name_len_a, neaset_node->len) == 0)
	{	
		/*neaset_node->tree == NULL*/
		if (neaset_node->tree == NULL)
		{
			if (clcf->exact_match)
				new_node->exact = clcf;
			else
				new_node->inclusive = clcf;
			
			neaset_node->tree = new_node;
			
			rv = NGX_OK;
		}
		else
		{
			/*the neaset_node->tree include new node*/
			if (neaset_node->tree->len > node_name_len
			 && ngx_filename_cmp(neaset_node->tree->name, clcf->name.data+clcf->name.len-node_name_len, node_name_len) == 0)
			{
				if (clcf->exact_match)
					new_node->exact = clcf;
				else
					new_node->inclusive = clcf;

				new_node->tree = neaset_node->tree;
				neaset_node->tree = new_node;
			}
			else
			{
				if (clcf->exact_match)
					new_node->exact = clcf;
				else
					new_node->inclusive = clcf;
				new_node->tree = NULL;

				len_temp = (node_name_len>neaset_node->tree->len)? node_name_len:neaset_node->tree->len;
				if (ngx_filename_cmp(neaset_node->tree->name, clcf->name.data+clcf->name.len-node_name_len, len_temp) < 0)
				{
					new_node->left = neaset_node->tree;
				}else
				{
					new_node->right = neaset_node->tree;
				}
				
				neaset_node->tree = new_node;
			}
			
			rv = NGX_OK;
		}
	}
	else
	/*not include */
	{
		len_temp = (node_name_len>neaset_node->len)? node_name_len:neaset_node->len;
		/*left*/
		if (ngx_filename_cmp(clcf->name.data+clcf->name.len-node_name_len, neaset_node->name, len_temp) < 0)
		{
			/*left == null*/
			if (neaset_node->left == NULL)
			{
				if (clcf->exact_match)
					new_node->exact = clcf;
				else
					new_node->inclusive = clcf;
				new_node->tree = NULL;
				neaset_node->left = new_node;
				
				rv = NGX_OK;
			}
			else/*left != null*/
			{	
				/*the neaset_node->left include new node*/
				if (neaset_node->left->len > node_name_len
				 && ngx_filename_cmp(neaset_node->left->name, clcf->name.data+clcf->name.len-node_name_len, node_name_len) == 0)
				{
					if (clcf->exact_match)
						new_node->exact = clcf;
					else
						new_node->inclusive = clcf;

					new_node->tree = neaset_node->left;
					neaset_node->left = new_node;
					
				}
				else
				{
					if (clcf->exact_match)
						new_node->exact = clcf;
					else
						new_node->inclusive = clcf;
					new_node->tree = NULL;
					len_temp = (node_name_len>neaset_node->left->len)? node_name_len:neaset_node->left->len;
					if (ngx_filename_cmp(neaset_node->left->name, clcf->name.data+clcf->name.len-node_name_len, len_temp) < 0)
					{
						new_node->left = neaset_node->tree;
					}
					else
					{
						new_node->right = neaset_node->tree;
					}

					neaset_node->left = new_node;
				}

				rv = NGX_OK;
			}
			
		}
		else/*right*/
		{
			/*right == null*/
			if (neaset_node->right == NULL)
			{
				if (clcf->exact_match)
					new_node->exact = clcf;
				else
					new_node->inclusive = clcf;
				new_node->tree = NULL;
				neaset_node->right = new_node;
				
				rv = NGX_OK;
			}
			else/*right != null*/
			{
				/*the neaset_node->right include new node*/
				if (neaset_node->right->len > node_name_len
				 && ngx_filename_cmp(neaset_node->right->name, clcf->name.data+clcf->name.len-node_name_len, node_name_len) == 0)
				{
					if (clcf->exact_match)
						new_node->exact = clcf;
					else
						new_node->inclusive = clcf;

					new_node->tree = neaset_node->right;
					neaset_node->right = new_node;
				}
				else
				{
					if (clcf->exact_match)
						new_node->exact = clcf;
					else
						new_node->inclusive = clcf;
					new_node->tree = NULL;
					len_temp = (node_name_len>neaset_node->left->len)? node_name_len:neaset_node->left->len;
					if (ngx_filename_cmp(neaset_node->left->name, clcf->name.data+clcf->name.len-node_name_len, len_temp) < 0)
					{
						new_node->left = neaset_node->tree;
					}
					else
					{
						new_node->right = neaset_node->tree;
					}

					neaset_node->right = new_node;
				}
				rv = NGX_OK;
			}
		}
	}

	if (rv == NGX_OK)
		locations_need_update = 1;
	
	return rv;
}

ngx_int_t
ngx_http_private_regex_location(ngx_conf_t *cf, ngx_http_core_loc_conf_t *clcf,
    ngx_str_t *regex, ngx_uint_t caseless)
{
#if (NGX_PCRE)
    ngx_regex_compile_t  rc;
    u_char               errstr[NGX_MAX_CONF_ERRSTR];

    ngx_memzero(&rc, sizeof(ngx_regex_compile_t));

    rc.pattern = *regex;
    rc.err.len = NGX_MAX_CONF_ERRSTR;
    rc.err.data = errstr;

#if (NGX_HAVE_CASELESS_FILESYSTEM)
    rc.options = NGX_REGEX_CASELESS;
#else
    rc.options = caseless;
#endif

    clcf->regex = ngx_http_regex_compile(cf, &rc);
    if (clcf->regex == NULL) {
        return NGX_ERROR;
    }

    clcf->name = *regex;

    return NGX_OK;

#else

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "using regex \"%V\" requires PCRE library",
                       regex);
    return NGX_ERROR;

#endif
}

void **ngx_get_null_location_loc(ngx_http_core_srv_conf_t  *cscf)
{
	ngx_uint_t			n, can;
	ngx_locations 		*clcf;
	if (locations == NULL)
		return NULL;

	can = 0;
	clcf = (ngx_locations *)locations->elts;

	if (clcf == NULL)
		return NULL;

	for (n=0; n<locations->nelts; n++)
	{
		if (clcf[n].srv!=NULL && clcf[n].srv == cscf)
			can = 1;
		else
		if (clcf[n].srv!=NULL && clcf[n].srv != cscf)
			can = 0;
		
		if (can != 1)
			continue;
		
		if (clcf[n].t == NULL)
			continue;

		if (clcf[n].t->name.len != 5)
			continue;
		
		if (ngx_memcmp(clcf[n].t->name.data, "/null", 5) == 0)
		{
			return clcf[n].t->loc_conf;
		}
	}
	
	return NULL;
}

char *ngx_location_add(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
	char                      *rv=NULL;
	void					  **pcloc;
    u_char                    *mod;
    size_t                     len;
    ngx_str_t                 *value, *name;
    ngx_uint_t                 i;
    ngx_http_module_t         *module;
    ngx_http_conf_ctx_t       *ctx;
    ngx_http_core_loc_conf_t  *clcf, *pclcf, *pcc;
	ngx_http_core_srv_conf_t  *cscf = conf;

	if (first == 0)
	{
		first = 1;
		return NGX_CONF_OK;
	}

	first = 0;
	if (cscf == NULL || cscf->ctx == NULL)
	{
		first = 1;
		return NGX_CONF_ERROR;
	}

	if (cf==NULL
		|| cf->args == NULL
		|| cf->args->nelts < 2)
	{
		first = 1;
		return NGX_CONF_ERROR;
	}
	
	value = cf->args->elts;

	
		
	if (value == NULL)
	{
		first = 1;
		return NGX_CONF_ERROR;
	}
	
	if (ngx_location_exist(cscf, &value[1]) == NGX_OK)
	{
		first = 1;
		return NGX_CONF_ERROR;
	}
	
    ctx = ngx_pcalloc(cf->pool, sizeof(ngx_http_conf_ctx_t));
    if (ctx == NULL) {
		first = 1;
        return NGX_CONF_ERROR;
    }
	
    ctx->main_conf = cscf->ctx->main_conf;
    ctx->srv_conf = cscf->ctx->srv_conf;

    ctx->loc_conf = ngx_pcalloc(cf->pool, sizeof(void *) * ngx_http_max_module);
    if (ctx->loc_conf == NULL) {
		first = 1;
        return NGX_CONF_ERROR;
    }

    for (i = 0; ngx_modules[i]; i++) {
        if (ngx_modules[i]->type != NGX_HTTP_MODULE) {
            continue;
        }

        module = ngx_modules[i]->ctx;

        if (module->create_loc_conf) {
            ctx->loc_conf[ngx_modules[i]->ctx_index] =
                                                   module->create_loc_conf(cf);
            if (ctx->loc_conf[ngx_modules[i]->ctx_index] == NULL) {
				first = 1;
                return NGX_CONF_ERROR;
            }
        }
    }

    clcf = ctx->loc_conf[ngx_http_core_module.ctx_index];

	pcloc = ngx_get_null_location_loc(cscf);
	if (pcloc == NULL)
	{
		first = 1;
		return NGX_CONF_ERROR;
	}

	pcc = pcloc[ngx_http_core_module.ctx_index];

	if (pcc == NULL)
	{
		first = 1;
		return NGX_CONF_ERROR;
	}
	
	/*use other location*/
	/*becouse our clcf is not init completely!!!*/
	ngx_memcpy(clcf, pcc, sizeof(ngx_http_core_loc_conf_t));
	clcf->loc_conf = ctx->loc_conf;
	if (clcf->loc_conf == NULL)
	{
		first = 1;
		return NGX_CONF_ERROR;
	}

	clcf->root.data = NULL;
	clcf->root.len  = 0;
	
	/*use other location->loc_conf*/
	ngx_memcpy(clcf->loc_conf, pcc->loc_conf, sizeof(void *) * ngx_http_max_module);
	
	clcf->loc_conf[ngx_http_core_module.ctx_index] = clcf;

    if (cf->args->nelts == 3) {

        len = value[1].len;
        mod = value[1].data;
		if (ngx_location_exist(cscf, &value[2]) == NGX_OK)
		{
			first = 1;
			return NGX_CONF_ERROR;
		}
		
		name = ngx_pcalloc(cf->pool, sizeof(ngx_str_t));
		if (name == NULL)
		{
			first = 1;
			return NGX_CONF_ERROR;
		}
		name->data = ngx_pcalloc(cf->pool, value[2].len);
		ngx_memcpy(name->data, value[2].data, value[2].len);
		name->len = value[2].len;
		
        if (len == 1 && mod[0] == '=') {

            clcf->name = *name;
            clcf->exact_match = 1;

        } else if (len == 2 && mod[0] == '^' && mod[1] == '~') {

            clcf->name = *name;
            clcf->noregex = 1;

        } else if (len == 1 && mod[0] == '~') {

			if (ngx_http_private_regex_location(cf, clcf, name, 0) != NGX_OK) {
				first = 1;
                return NGX_CONF_ERROR;
            }

        } else if (len == 2 && mod[0] == '~' && mod[1] == '*') {
			
            if (ngx_http_private_regex_location(cf, clcf, name, 1) != NGX_OK) {
				first = 1;
                return NGX_CONF_ERROR;
            }

        } else {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "invalid location modifier \"%V\"", &value[1]);
			first = 1;
            return NGX_CONF_ERROR;
        }

    } else {
		
		name = ngx_pcalloc(cf->pool, sizeof(ngx_str_t));
		if (name == NULL)
		{
			first = 1;
			return NGX_CONF_ERROR;
		}
		name->data = ngx_pcalloc(cf->pool, value[1].len);
		ngx_memcpy(name->data, value[1].data, value[1].len);
		name->len = value[1].len;
		
        if (name->data[0] == '=') {

            clcf->name.len = name->len - 1;
            clcf->name.data = name->data + 1;
            clcf->exact_match = 1;

        } else if (name->data[0] == '^' && name->data[1] == '~') {

            clcf->name.len = name->len - 2;
            clcf->name.data = name->data + 2;
            clcf->noregex = 1;

        } else if (name->data[0] == '~') {

            name->len--;
            name->data++;

            if (name->data[0] == '*') {

                name->len--;
                name->data++;

				if (ngx_http_private_regex_location(cf, clcf, name, 1) != NGX_OK) {
					first = 1;
                    return NGX_CONF_ERROR;
                }

            } else {
	               if (ngx_http_private_regex_location(cf, clcf, name, 0) != NGX_OK) {
				   	first = 1;
	                return NGX_CONF_ERROR;
	            }
            }

        } else {

            clcf->name = *name;

            if (name->data[0] == '@') {
                clcf->named = 1;
            }
        }
    }

    pclcf = clcf->loc_conf[ngx_http_core_module.ctx_index];

    if (pclcf->name.len) {


#if 0
        clcf->prev_location = pclcf;
#endif

        if (pclcf->exact_match) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "location \"%V\" cannot be inside "
                               "the exact location \"%V\"",
                               &clcf->name, &pclcf->name);
			first = 1;
            return NGX_CONF_ERROR;
        }

        if (pclcf->named) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "location \"%V\" cannot be inside "
                               "the named location \"%V\"",
                               &clcf->name, &pclcf->name);
			first = 1;
            return NGX_CONF_ERROR;
        }

        if (clcf->named) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "named location \"%V\" can be "
                               "on the server level only",
                               &clcf->name);
			first = 1;
            return NGX_CONF_ERROR;
        }

        len = pclcf->name.len;

#if (NGX_PCRE)
        if (clcf->regex == NULL
            && ngx_strncmp(clcf->name.data, pclcf->name.data, len) != 0)
#else
        if (ngx_strncmp(clcf->name.data, pclcf->name.data, len) != 0)
#endif
        {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "location \"%V\" is outside location \"%V\"",
                               &clcf->name, &pclcf->name);
            first = 1;
            return NGX_CONF_ERROR;
        }
    }

	clcf->limit_rate = 0;

	/*regex?*/
	if (clcf->regex)
	{
		rv = (char *)ngx_location_add_regex(clcf, cscf);
	}
	else
	if (clcf->named) /*named*/
	{
		/*rv = */
	}
	else /*static*/
	{
		rv = (char *)ngx_location_add_static(clcf, cscf);
	}

	if (rv == NGX_CONF_ERROR)
		first = 1;
	return rv;
	
}

char *ngx_location_update(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
	return NGX_CONF_ERROR;
}

ngx_int_t delete_fun(ngx_http_location_tree_node_t *parent, ngx_http_location_tree_node_t *delete, ngx_http_location_tree_node_t *replace)
{
	if (parent == NULL)
		return NGX_ERROR;
	
	if (parent->left == delete)
	{
		parent->left = replace;
	}
	if (parent->right== delete)
	{
		parent->right= replace;
	}
	if (parent->tree== delete)
	{
		parent->tree= replace;
	}
	
	return NGX_OK;
}

ngx_int_t insert_follow(ngx_http_location_tree_node_t *node, ngx_http_core_srv_conf_t *cscf)
{
	ngx_http_core_loc_conf_t		*clcf;

	if (node == NULL)
		return NGX_DONE;
	
	if (node->left)
	{
		clcf = node->left->exact;
		if (clcf == NULL)
			clcf = node->left->inclusive;
		ngx_location_add_static(clcf, cscf);
		
		insert_follow(node->left, cscf);
	}

	if (node->right)
	{
		clcf = node->right->exact;
		if (clcf == NULL)
			clcf = node->right->inclusive;
		ngx_location_add_static(clcf, cscf);
		
		insert_follow(node->right, cscf);
	}

	if (node->tree)
	{
		clcf = node->tree->exact;
		if (clcf == NULL)
			clcf = node->tree->inclusive;
		ngx_location_add_static(clcf, cscf);

		insert_follow(node->tree, cscf);
	}
	
	return NGX_ERROR;
}

char *ngx_location_del_static_loc(ngx_http_location_tree_node_t *node, ngx_str_t *loc_name, ngx_http_core_srv_conf_t *cscf)
{
	u_char     						*uri;
    size_t      					len, n;
    ngx_int_t   					rc;
	ngx_http_location_tree_node_t	*node_ret, *parent, *left, *right, *tree;

	node_ret = NULL;
	parent = NULL;
	left = NULL;
	right = NULL;
	tree = NULL;
	
    len = loc_name->len;
    uri = loc_name->data;

    for ( ;; ) {

        if (node == NULL) {
            return NGX_CONF_ERROR;
        }

        n = (len <= (size_t) node->len) ? len : node->len;

        rc = ngx_filename_cmp(uri, node->name, n);

        if (rc != 0) {
			node_ret = node;
			parent = node;
            node = (rc < 0) ? node->left : node->right;
            continue;
        }

        if (len > (size_t) node->len) {

			node_ret = node;
			
            if (node->inclusive) {
				parent = node;
                node = node->tree;
                uri += n;
                len -= n;
				
                continue;
            }

            /* exact only */
			
			parent = node;
            node = node->right;
			
            continue;
        }

        if (len == (size_t) node->len) {
			/*found!*/

			if (node->left == NULL
				&& node->right == NULL
				&& node->tree == NULL)
			{
				/*delete*/
				return (char *)delete_fun(parent, node, NULL);
			}

			if (node->left == NULL 
				&& node->right == NULL 
				&& node->tree!=NULL)
			{
				return (char *)delete_fun(parent, node, node->tree);
			}

			if (node->left == NULL 
				&& node->tree == NULL 
				&& node->right!=NULL)
			{
				return (char *)delete_fun(parent, node, node->right);
			}

			if (node->right == NULL 
				&& node->tree == NULL 
				&& node->left!=NULL)
			{
				return (char *)delete_fun(parent, node, node->left);
			}

			/************************************************/
			/*
			node->right != null
			node->left ! = null
			node->tree != null
			1. delete node.
			2. the child_node of node follow insert into cscf->stati_location.
			*/
			if (delete_fun(parent, node, NULL) != NGX_OK)
				return NGX_CONF_ERROR;
			
			insert_follow(node, cscf);
			return NGX_CONF_OK;
        }
		
		parent = node;
        node = node->left;
    }

	return NGX_CONF_ERROR;
}

char *ngx_location_del(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
	ngx_int_t						n, m, len;
	ngx_str_t						*value;
	ngx_http_core_srv_conf_t		*cscf = conf;
	ngx_http_core_loc_conf_t		*clcf, **preglocation;

	if (cscf == NULL
		|| cscf->ctx == NULL
		|| cscf->ctx->loc_conf == NULL)
	{
		return NGX_CONF_ERROR;
	}

	if (cf == NULL
		|| cf->args == NULL
		|| cf->args->nelts < 2)
	{
		return NGX_CONF_ERROR;
	}
	
	value = cf->args->elts;
	if (value == NULL)
		return NGX_CONF_ERROR;
	
	clcf = cscf->ctx->loc_conf[ngx_http_core_module.ctx_index];
	if (clcf == NULL)
		return NGX_CONF_ERROR;
	
	preglocation = clcf->regex_locations;
	if (preglocation != NULL)
	{
		/*is regex_location?*/
		for (n=0; preglocation[n] != NULL; n++)
		{
			len  = (preglocation[n]->name.len > value[1].len)? preglocation[n]->name.len:value[1].len;
			/*find*/
			if (ngx_memcmp(preglocation[n]->name.data, value[1].data, len) == 0)
			{
				for (m=n; preglocation[m] != NULL; m++)
				{
					preglocation[m] = preglocation[m+1];
				}

				locations_need_update = 1;
				return NGX_CONF_OK;
			}
		}
	}

	/*is static_location?*/
	if (clcf->static_locations == NULL)
		return NGX_CONF_ERROR;
	
	if (ngx_location_del_static_loc(clcf->static_locations, &value[1], cscf) == NGX_CONF_OK)
	{
		locations_need_update = 1;
		return NGX_CONF_OK;
	}

	return NGX_CONF_ERROR;
}

ngx_int_t ngx_limit_host_write_to_file()
{
	char			buf[512];
	char			*out_cjson;
	FILE			*fd;
	cJSON			*root,*fmt;
	ngx_int_t		rv;
	ngx_host_list_t	*pos;
	
	if (host_limit_conf.path_conf.data == NULL
		|| host_limit_conf.path_conf.len > 512)
		return NGX_ERROR;

	root=cJSON_CreateObject();
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
	
	out_cjson=cJSON_Print(root);
	cJSON_Delete(root);
	
	if (out_cjson == NULL)
		return NGX_ERROR;
	
	ngx_memzero(buf ,512);
	ngx_memcpy(buf, host_limit_conf.path_conf.data, host_limit_conf.path_conf.len);
	
	fd = fopen(buf, "w+b");
	if (fd == NULL)
	{
		goto failed;
	}

	rv = ftruncate((long)fd, 0);
	fseek(fd, 0, SEEK_SET);

	if (fwrite(out_cjson, ngx_strlen(out_cjson), 1, fd)>0)
	{
		fclose(fd);
		
		if (out_cjson)
			ngx_free(out_cjson);
		return NGX_OK;
	}
	

close_fd:
	fclose(fd);

failed:
	if (out_cjson)
		ngx_free(out_cjson);
	return NGX_ERROR;
}

ngx_int_t ngx_put_limit_host_list(ngx_host_list_t *new_node)
{
	ngx_host_list_t		*pos = NULL;
	
	pos = host_limit_conf.host_list;
	if (pos == NULL)
	{
		host_limit_conf.host_list = new_node;
		new_node->prev = NULL;
		new_node->next = NULL;
	}
	else
	{
		while (pos->next != NULL)
			pos = pos->next;
		
		pos->next = new_node;
		new_node->prev = pos;
		new_node->next = NULL;
	}
	
	return NGX_OK;
}

ngx_int_t ngx_del_limit_host_list(ngx_str_t *host_name)
{
	ngx_host_list_t		*pos = NULL;
	
	pos = host_limit_conf.host_list;
	if (pos == NULL || host_name == NULL)
	{
		return NGX_ERROR;
	}
	else
	{
		for (;pos != NULL; pos = pos->next)
		{
			if (pos->host.len != host_name->len)
				continue;

			if (pos->host.data == NULL)
				continue;

			if (ngx_memcmp(pos->host.data, host_name->data, host_name->len) == 0)
			{
				if (pos->prev == NULL)
				{
					host_limit_conf.host_list = pos->next;
					if (pos->next != NULL)
						pos->next->prev = NULL;
					return NGX_OK;
				}

				if (pos->next == NULL)
				{
					pos->prev->next = NULL;
					return NGX_OK;
				}
				
				pos->prev->next = pos->next;
				pos->next->prev = pos->prev;
				
				return NGX_OK;
			}
		}
	}
	
	return NGX_ERROR;
}

ngx_int_t ngx_http_lrlc_isexist_and_update(ngx_str_t *value)
{
	ssize_t				limit_rate;
	ngx_host_list_t		*pos = NULL;
	
	pos = host_limit_conf.host_list;
	if (pos == NULL)
	{
		return NGX_DONE;
	}
	else
	{
		for (;pos != NULL; pos = pos->next)
		{
			if (value[1].len != pos->host.len)
				continue;

			if (value[1].data == NULL)
				continue;

			if (ngx_memcmp(value[1].data, pos->host.data, pos->host.len) == 0)
			{
				/*update*/
				limit_rate = ngx_parse_size(&value[2]);
				if (limit_rate != 0 && limit_rate != NGX_ERROR)
				{
					pos->limit_rate = limit_rate;
					ngx_limit_host_write_to_file();
					return NGX_OK;
				}

				return NGX_ERROR;
			}
		}
	}
	
	return NGX_DONE;
}

char *ngx_http_lrlc_add(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
	ssize_t				limit_rate;
	ngx_int_t			rv;
	ngx_str_t			*value;
	ngx_host_list_t		*new_node, *p_rbnode;
	ngx_rbtree_node_t	*new_rbnode;

	if (   cf->args == NULL 
		|| cf->args->elts == NULL
		|| cf->args->nelts < 3)
		return NGX_CONF_ERROR;

	value = cf->args->elts;

	limit_rate = ngx_parse_size(&value[2]);
	if (limit_rate == 0 || limit_rate == NGX_ERROR)
		return NGX_CONF_ERROR;
	
	if (value[1].data == NULL || value[1].len == 0)
		return NGX_CONF_ERROR;

	rv = ngx_http_lrlc_isexist_and_update(value);
	if (rv == NGX_OK)
		return NGX_CONF_OK;

	if (rv == NGX_ERROR)
		return NGX_CONF_ERROR;
	
	new_node = ngx_pcalloc(cf->pool, sizeof(ngx_host_list_t));
	if (new_node == NULL)
		return NGX_CONF_ERROR;

	new_node->host.len = value[1].len;
	new_node->host.data = ngx_pcalloc(cf->pool, value[1].len);
	if (new_node->host.data == NULL)
		return NGX_CONF_ERROR;

	ngx_memcpy(new_node->host.data, value[1].data, value[1].len);
	new_node->limit_rate = limit_rate;
	new_node->next = NULL;
	
	if (ngx_put_limit_host_list(new_node) != NGX_OK)
		return NGX_CONF_ERROR;

	/*add to rbtree*/
	new_rbnode = ngx_pcalloc(cf->pool, sizeof(ngx_rbtree_node_t) + sizeof(void *) -1);
	if (new_rbnode == NULL)
		return NGX_CONF_ERROR;
	new_rbnode->key = ngx_crc32_short(new_node->host.data, new_node->host.len);
	p_rbnode = (ngx_host_list_t *)&new_rbnode->data;
	ngx_memcpy(p_rbnode, &new_node, sizeof(void *));
	ngx_rbtree_insert(&l_host_rbtree, new_rbnode);
	
	/*write to file*/
	if (ngx_limit_host_write_to_file() != NGX_OK)
	{
		/*log*/
		ngx_log_error(NGX_LOG_ERR, cf->log, 0,
                          "limit_host, update the conf file failed! funciton=ngx_http_lrlc_add");
	}
	
	return NGX_CONF_OK;	
}

char *ngx_http_lrlc_del(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
	ngx_str_t			*value;
	ngx_rbtree_node_t	*node;
	
	if (   cf->args == NULL 
		|| cf->args->elts == NULL
		|| cf->args->nelts < 2)
		return NGX_CONF_ERROR;

	value = cf->args->elts;
	if (ngx_del_limit_host_list(&value[1]) != NGX_OK)
		return NGX_CONF_ERROR;

	/*del from rbtree*/
	node = ngx_limit_host_list_lookup(&l_host_rbtree, &value[1], ngx_crc32_short(value[1].data, value[1].len));
	if (node != NULL)
		ngx_rbtree_delete(&l_host_rbtree, node);	
	
	/*write to file*/
	if (ngx_limit_host_write_to_file() != NGX_OK)
	{
		/*log*/
		ngx_log_error(NGX_LOG_ERR, cf->log, 0,
                          "limit_host, update the conf file failed! funciton=ngx_http_lrlc_del");
	}
	
	return NGX_CONF_OK;
}

