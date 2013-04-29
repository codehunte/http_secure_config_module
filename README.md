http_secure_config_module
=========================
--------------------------------------------------------------
sec_config

sec_config功能介绍

sec_config 提供了nginx几乎所有安全模块以及黑白名单模块的 http配置接口，
可通过http接口查询它们的详细配置情况，并可动态修改它们的配置。

-----------------------------------------------------------------

需要知识
1. 了解ngixn的配置
2. 熟悉 nginx
limit_req_zone, limit_req
limit_conn_zone, limit_conn
limit_rate 的配置方法
3. 了解curl的使用

-----------------------------------------------------------------------------------

sec_config 目前主要支持下面模块的  http配置接口
1. limit_req_zone, limit_req
2. limit_conn
3. limit_rate
4. white_black_list_conf 
5. white_list 
6. black_list

-----------------------------------------------------------------------------------

sec_config 配置
nginx配置文件中，在http块下，增加如下配置信息即可。
  location /manager {
             config_update;
	     ...
	     ...
             secure_config_update;
        }

        location /sec_config {
             sec_config on;
        }
}

-------------------------------------------------------------------------------------

sec_config 的使用
经过以上配置后即可通过 http://your host/sec_config 
和 http://your host/manager/secure_config_update 查询和配置。

--------------------------------------------------------------------------------------

http://your host/sec_config说明
该接口是通过get方式访问的，主要是查询配置与修改  white_black_list 中的ip列表信息的。

访问 http://your host/sec_config  将返回 white_black_list 容器的配置情况。
如下：
{
	"version":	"nginx/1.2.0",
	"code":	"0",
	"item":	{
		"conf_type":	"white_black_list_conf",
		"zone_name":	"white",
		"list_path":	"/opt/nginx/conf/white.list"
	},
	"item":	{
		"conf_type":	"white_black_list_conf",
		"zone_name":	"black",
		"list_path":	"/opt/nginx/conf/black.list"
	}
}

对于返回的信息，基本上只需关注zone_name即可，因为只有这个后面会用到。

访问 http://you host/sec_config?for_each  将返回 目前sec_config 所支持模块的所有配置详细信息。
类似：
{
  "version": "dragon/1.4.2_beta1", "code": "0",
  "zone": 
  {
  },

  "server": 
  { 
    "index": 0,
    "server_name":
    "listen": "0.0.0.0:80", locations
    ......
  },

  "server": 
  {
  } 
}


查看某个黑白名单容器中数据
示例：
http://you host/sec_config?zone_name=white
secure_config_update
向某个黑白名单容器中增加IP信息
示例：
http://you host/sec_config?zone_name=white&add_item=182.157.3.2

向某个黑白名单容器中删除IP信息
示例：
http://you host/sec_config?zone_name=white&delete_item=182.157.3.2

-----------------------------------------------------------------------------------

查看限制host的配置信息
示例：

http://127.0.0.1/sec_config?limit_host
{
	"version":	"nginx/1.2.0",
	"code":	"0",
	"host_limit":	{
		"test.baidu.com":	10485760,
		"localhost":	131072
	}
}
其中host_limit 字段 即是限制host的配置信息
限制test.baidu.com  每个TCP连接的带宽为 10485760byte
限制localhost 每个TCP连接的带宽为131072byte

-------------------------------------------------------------------------------------

http://your host/manager/secure_config_update 说明
该接口需要使用post的方式提交配置数据
以下示例都将使用 curl 提交post数据

post 数据的格式：
method=xxx&cmd_name=xxx&arg=xxx&where=xxx
其中方法有: add，del, update
cmd_name有:
limit_req_zone(目前只支持update), limit_req, limit_conn, limit_rate, white_list, black_list
arg 即是cmd_name中相关命令所需参数
where是指配置作用位置。。 
该值是个数字，取值来源于http://you host/sec_config?for_each 返回结果中的index字段的数据。

具体如下：
￼￼￼method 参数,必须
  ￼￼￼可取值,del、add、update

￼￼￼cmd_name 参 数 , 必 须
  ￼￼可 取 值 
    limit_req_zone( 仅 支 持 method=update) , 
    limit_req , 
    limit_conn , 
    limit_rate,
    white_list,
    black_list, 
    limit_rate_conf

￼￼￼arg 参数,必须
  ￼￼取值由 cmd_name 的取值决定,具体如下(各举一例):
    limit_req_zone: arg=zone=one rate=10r/s
    limit_req: arg==zone=one burst=12 nodelay
    limit_conn: arg=perip 8
    limit_rate: arg=128k
    white_list: arg=black on
    black_list: arg=white on
    limit_rate_conf: arg=www.xxx.com 1M

￼￼￼￼where 参数
  ￼￼取值为 location 的编号,可通过/sec_config?for_each 获 得。是否必须决定于 cmd_name 的取值。
    limit_req_zone 不需要
    limit_req 必须
    limit_conn 必须 
    limit_rate 必须 
    white_list 必须 
    black_list 必须 
    limit_rate_conf 不需要
￼￼
返回结果
成功:
   {
      "code": 1000,
      "text": "config update succed" 
   }  

失败:
   {
     "code": 非 1000,
     "text": "config update succed" 
   }

------------------------------------------------------------

示例1：
curl -d "method=update&cmd_name=limit_req_zone&arg=zone=one rate=10r/s" http://your host/manager/secure_config_update
更新limit_req_zone 容器为one  中的rate数据为10r/s

示例2：
curl -d "method=add&cmd_name=limit_rate&arg=128K&where=8" http://your host/manager/secure_config_update
在where=8下面增加 limit_rate 128K; 的配置

示例3：
curl -d "method=del&cmd_name=limit_rate&arg=128K&where=8" http://your host/manager/secure_config_update
在where=8下面删除 limit_rate 128K; 的配置

示例4：
curl -d "method=add&cmd_name=limit_conn&arg=perip 8&where=8" http://your host/manager/secure_config_update
在where=8下面增加limit_conn perip 8的配置，按照zonename=perip, 限制客户端容许最大TCP连接数为8个。（具体需要了解 nginx，limit_conn_zone, limit_conn 的使用）

示例5：
curl -d "method=add&cmd_name=black_list&arg=black on&where=8" http://your host/manager/secure_config_update
在where=8下面增加 black_list black on; 的配置

示例6：
curl -d "method=add&cmd_name=limit_req&arg=zone=one burst=12 nodelay&where=8" http://your host/manager/secure_config_update
在where=8下面增加limit_req zone=one burst=12; 的配置

示例7：
curl -d "method=add&cmd_name=limit_rate_conf&arg=www.xxxx.com 1M" http://your host/manager/secure_config_update
增加 限制host信息，限制www.xxxx.com 的带宽为1M byte

示例8：
curl -d "method=del&cmd_name=limit_rate_conf&arg=www.xxx.com" http://your host/manager/secure_config_update
删除 限制host信息，删除对www.xxxx.com 的带宽限制


