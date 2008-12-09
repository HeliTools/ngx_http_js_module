#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <nginx.h>

#include <js/jsapi.h>


ngx_buf_t *
js_str2ngx_buf(JSContext *cx, JSString *str, ngx_pool_t *pool, size_t len)
{
	ngx_buf_t           *b;
	const char          *p;
	
	if (len == 0)
		len = JS_GetStringLength(str);
	
	p = JS_GetStringBytes(str);
	if (p == NULL)
		return NULL;
	
	b = ngx_create_temp_buf(pool, len);
	if (b == NULL)
		return NULL;
	ngx_memcpy(b->last, p, len);
	b->last = b->last + len;
	
	return b;
}

JSBool
js_str2ngx_str(JSContext *cx, JSString *str, ngx_pool_t *pool, ngx_str_t *s, size_t len)
{
	const char          *p;
	
	s->len = 0;
	s->data = NULL;
	
	if (len == 0)
		if ((len = JS_GetStringLength(str)) == 0)
			return JS_TRUE;
	
	p = JS_GetStringBytes(str);
	if (p == NULL)
	{
		JS_ReportOutOfMemory(cx);
		return JS_FALSE;
	}
	
	s->data = ngx_palloc(pool, len);
	if (s->data == NULL)
	{
		JS_ReportOutOfMemory(cx);
		return JS_FALSE;
	}
	
	ngx_memcpy(s->data, p, len);
	s->len = len;
	
	return JS_TRUE;
}
