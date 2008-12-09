#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <nginx.h>

#include <js/jsapi.h>

#include "ngx_http_js_module.h"
#include "classes/Nginx.h"
#include "classes/Request.h"


void
reportError(JSContext *cx, const char *message, JSErrorReport *report)
{
	ngx_http_js_context_private_t  *private;
	
	private = JS_GetContextPrivate(cx);
	
	if (!private)
		return;
	
	ngx_log_error
	(
		NGX_LOG_ERR, private->log, 0,
		"%s:%i: %s",
		report->filename ? report->filename : "<no filename>",
		(unsigned int) report->lineno,
		message
	);
}


static JSBool
ngx_http_js_load(JSContext *cx, JSObject *global, char *filename)
{
	jsval           fval, rval, strval;
	JSString       *fnstring;
	// JSObject       *require;
	
	if (!JS_GetProperty(cx, global, "load", &fval))
	{
		JS_ReportError(cx, "global.load is undefined");
		return JS_FALSE;
	}
	if (!JSVAL_IS_OBJECT(fval) || !JS_ValueToFunction(cx, fval))
	{
		JS_ReportError(cx, "global.load is not a function");
		return JS_FALSE;
	}
	
	fnstring = JS_NewStringCopyZ(cx, filename);
	if (!fnstring)
		return JS_FALSE;
		
	strval = STRING_TO_JSVAL(fnstring);
	
	if (!JS_CallFunctionValue(cx, global, fval, 1, &strval, &rval))
	{
		JS_ReportError(cx, "error calling global.load from nginx");
		return JS_FALSE;
	}
	
	return JS_TRUE;
}

static ngx_int_t
ngx_http_js_run_requires(JSContext *cx, JSObject *global, ngx_array_t *requires, ngx_log_t *log)
{
	// ngx_str_t     **script;
	char          **script;
	ngx_uint_t      i;
	jsval           rval, strval, fval;
	// ngx_str_t      *value;
	char           *value;
	// JSObject       *require;
	
	if (!ngx_http_js_load(cx, global, NGX_HTTP_JS_CONF_PATH))
		return NGX_ERROR;
	
	if (!JS_GetProperty(cx, global, "require", &fval))
	{
		JS_ReportError(cx, "global.require is undefined");
		return NGX_ERROR;
	}
	if (!JSVAL_IS_OBJECT(fval) || !JS_ValueToFunction(cx, fval))
	{
		JS_ReportError(cx, "global.require is not a Function object");
		return NGX_ERROR;
	}
	
	script = requires->elts;
	for (i = 0; i < requires->nelts; i++)
	{
		value = script[i];
		// fprintf(stderr, "load %s\n", value);
		
		strval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, (char*)value));
		if (!JS_CallFunctionValue(cx, global, fval, 1, &strval, &rval))
		{
			JS_ReportError(cx, "error calling global.require from nginx");
			return NGX_ERROR;
		}
	}
	
	return NGX_OK;
}



char *
ngx_http_js__glue__init_interpreter(ngx_conf_t *cf, ngx_http_js_main_conf_t *jsmcf)
{
	static JSRuntime *rt;
	static JSContext *static_cx = NULL;
	static JSObject  *global;
	static ngx_http_js_context_private_t   private;
	JSContext        *cx;
	
	if (jsmcf->js_cx != NULL)
		return NGX_CONF_OK;
	
	if (static_cx)
	{
		if (ngx_set_environment(cf->cycle, NULL) == NULL)
			return NGX_CONF_ERROR;
		
		jsmcf->js_cx = static_cx;
		jsmcf->js_global = global;
		
		return NGX_CONF_OK;
	}
	
	private.cf = cf;
	private.jsmcf = jsmcf;
	private.log = cf->log;
	
	
	rt = JS_NewRuntime(32L * 1024L * 1024L);
	if (rt == NULL)
		return NGX_CONF_ERROR;
	
	cx = JS_NewContext(rt, 8192);
	if (cx == NULL)
		return NGX_CONF_ERROR;
	
	JS_SetOptions(cx, JSOPTION_VAROBJFIX);
	JS_SetVersion(cx, 170);
	JS_SetContextPrivate(cx, &private);
	JS_SetErrorReporter(cx, reportError);
	
	// global
	global = NULL;
	
	if (!JS_InitStandardClasses(cx, global))
		return NGX_CONF_ERROR;
	
	
	// call some external func
	
	
	jsmcf->js_cx = static_cx = cx;
	jsmcf->js_global = global;
	
	if (ngx_http_js_run_requires(static_cx, global, &jsmcf->requires, cf->log) != NGX_OK)
		return NGX_CONF_ERROR;
	
	return NGX_CONF_OK;
}





char *
ngx_http_js__glue__set_callback(ngx_conf_t *cf, ngx_command_t *cmd, ngx_http_js_loc_conf_t *jslcf)
{
	ngx_str_t                  *value;
	ngx_http_js_main_conf_t    *jsmcf;
	JSContext                  *cx;
	jsval                       sub;
	static char                *JS_CALLBACK_ROOT_NAME = "js callback instance";
	
	jsmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_js_module);
	cx = jsmcf->js_cx;
	
	if (jsmcf->js_cx == NULL)
		if (ngx_http_js__glue__init_interpreter(cf, jsmcf) != NGX_CONF_OK)
			return NGX_CONF_ERROR;
	
	value = cf->args->elts;
	if (!JS_EvaluateScript(jsmcf->js_cx, jsmcf->js_global, (char*)value[1].data, value[1].len, (char*)cf->conf_file->file.name.data, cf->conf_file->line, &sub))
		return NGX_CONF_ERROR;
	
	if (!JSVAL_IS_OBJECT(sub) || !JS_ValueToFunction(jsmcf->js_cx, sub))
	{
		ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "processRequest is not a function");
		return NGX_CONF_ERROR;
	}
	
	jslcf->sub = JSVAL_TO_OBJECT(sub);
	if (!JS_AddNamedRoot(cx, &jslcf->sub, JS_CALLBACK_ROOT_NAME))
	{
		JS_ReportError(cx, "Can`t add new root %s", JS_CALLBACK_ROOT_NAME);
		return NGX_CONF_ERROR;
	}
	
	
	return NGX_CONF_OK;
}




//#define unless(a) if(!(a))

ngx_int_t
ngx_http_js__glue__call_handler(JSContext *cx, JSObject *global, ngx_http_request_t *r, JSObject *sub, ngx_str_t *handler)
{
	int                status;
	ngx_connection_t  *c;
	JSObject          *request;
	jsval              req;
	// fprintf(stderr, "%s", (char *) r->uri.data);
	status = NGX_HTTP_OK;
	jsval              rval;
	static char        *JS_REQUEST_ROOT_NAME = "Nginx.Request instance";
	
	// ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "ngx_http_js_call_handler(%p)", r);
	
	request = JS_NewObject(cx, &ngx_http_js__nginx_request_class, ngx_http_js__nginx_request_prototype, NULL);
	if (!request)
	{
		JS_ReportOutOfMemory(cx);
		return NGX_ERROR;
	}
	
	if (!JS_AddNamedRoot(cx, &request, JS_REQUEST_ROOT_NAME))
	{
		JS_ReportError(cx, "Can`t add new root %s", JS_REQUEST_ROOT_NAME);
		return NGX_ERROR;
	}
	JS_SetPrivate(cx, request, r);
	req = OBJECT_TO_JSVAL(request);
	
	c = r->connection;
	
	if (JS_CallFunctionValue(cx, global, OBJECT_TO_JSVAL(sub), 1, &req, &rval))
	{
		if (!JSVAL_IS_INT(rval))
		{
			status = NGX_ERROR;
			JS_ReportError(cx, "Request processor must return an Integer");
		}
		else
			status = JSVAL_TO_INT(rval);
	}
	else
		status = NGX_ERROR;
	
	// if (r->headers_out.status == 0)
	// 	r->headers_out.status = NGX_HTTP_OK;
	// ngx_http_send_header(r);
	
	JS_SetPrivate(cx, request, NULL);
	
	JS_MaybeGC(cx);
	
	if (c->destroyed)
		return NGX_DONE;
	// fprintf(stderr, "%d", status);
	return (ngx_int_t) status;
}
