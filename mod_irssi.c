#include "jsirssi.h"

static JSBool irssi_getprop(JSContext*, JSObject*, jsid, jsval*),
              bind_getprop(JSContext*, JSObject*, jsid, jsval*);
static void bind_freeer(JSContext*, JSObject*);

struct callback_data {
	jsval fun;
	JSObject *obj;
	char *name;
};

static struct JSClass irssi_class = {
	"Irssi", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, irssi_getprop, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

static struct JSClass bind_class = {
	"Bind", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, bind_getprop, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, bind_freeer
};
/*
static struct JSClass bind_class = JSCLASS_ROOBJ(
	"Bind", JSCLASS_HAS_PRIVATE,
	bind_getprop, JS_PropertyStub, bind_freeer, NULL);
*/

enum {
	BIND_COMMAND, UNBIND_COMMAND
};

static struct JSPropertySpec irssi_props[] = {
	{"bindCommand", BIND_COMMAND, JSPROP_ENUMERATE | JSPROP_READONLY},
	{NULL}
};

static struct JSPropertySpec bind_props[] = {
	{"unbind", UNBIND_COMMAND, JSPROP_ENUMERATE | JSPROP_READONLY},
	{NULL}
};

jsval mod_irssi_get(void *unused){
	JSObject *ret;
	
	ret = JS_NewObject(js_cx, &irssi_class, NULL, NULL);
	JS_DefineProperties(js_cx, ret, irssi_props);
	return OBJECT_TO_JSVAL(ret);
}

static void command_callback(char *args){
	jsval rval, aval[1];

	aval[0] = STRING_TO_JSVAL(JS_NewStringCopyZ(js_cx, args));
	/* `this` will be the <commandline>'s global object */
	/* no it won't; passing NULL here causes it to be the object of the global scope it's defined in */
	JS_CallFunctionValue(js_cx, NULL /*JS_GetGlobalObject(js_cx)*/, *(jsval*)signal_user_data, 1, aval, &rval);
}

static void bind_freeer(JSContext *cx, JSObject *obj){
	struct callback_data *data;

	if(data = JS_GetPrivate(cx, obj)){
		/* todo: don't do these silly casts */
		command_unbind_full(data->name, (SIGNAL_FUNC)command_callback, data);
		JS_RemoveObjectRoot(cx, &data->obj);
		JS_RemoveValueRoot(cx, &data->fun);
/*		if(data->name) */
		JS_free(cx, data->name);
		JS_free(cx, data);
		JS_SetPrivate(cx, obj, NULL);
	}
}

static JSBool bind_fun_unbind(JSContext *cx, uintN argc, jsval *vp){
	jsval bindobj;

	JS_GetReservedSlot(cx, JSVAL_TO_OBJECT(JS_CALLEE(cx, vp)), 0, &bindobj);
	bind_freeer(cx, JSVAL_TO_OBJECT(bindobj));
	JS_SET_RVAL(js_cx, vp, JSVAL_VOID);
	return JS_TRUE;
}

static JSBool bind_getprop(JSContext *cx, JSObject *obj, jsid id, jsval *vp){
	JSObject *fun;

	JS_IdToValue(cx, id, vp);

	if(JSVAL_IS_INT(*vp))
		switch(JSVAL_TO_INT(*vp)){
			case UNBIND_COMMAND:
				*vp = OBJECT_TO_JSVAL(fun = JS_GetFunctionObject(JS_NewFunction(cx,
					bind_fun_unbind, 0, 0, NULL, "unbind")));
				JS_SetReservedSlot(cx, fun, 0, OBJECT_TO_JSVAL(obj));
				return JS_TRUE;
		}
	*vp = JSVAL_VOID;
	return JS_TRUE;
}

static JSBool irssi_fun_bindCommand(JSContext *cx, uintN argc, jsval *vp){
	struct callback_data *data;
	JSObject *ret;

	data = JS_malloc(cx, sizeof *data);
	data->fun = JS_ARGV(cx, vp)[1];
	JS_AddValueRoot(cx, &data->fun);
	data->name = JS_EncodeString(cx, JS_ValueToString(cx, JS_ARGV(cx, vp)[0]));
	command_bind_data(data->name, NULL, (SIGNAL_FUNC)command_callback, data);
	/* todo: return an object with .unbind */
	ret = JS_NewObject(cx, &bind_class, NULL, NULL);
	JS_SetPrivate(cx, ret, data);
	JS_DefineProperties(js_cx, ret, bind_props);
	JS_SET_RVAL(js_cx, vp, OBJECT_TO_JSVAL(ret));
	return JS_TRUE;
}

static JSBool irssi_getprop(JSContext *cx, JSObject *obj, jsid id, jsval *vp){
	JS_IdToValue(cx, id, vp);

	if(JSVAL_IS_INT(*vp))
		switch(JSVAL_TO_INT(*vp)){
			case BIND_COMMAND:
				*vp = OBJECT_TO_JSVAL(JS_GetFunctionObject(JS_NewFunction(cx,
					irssi_fun_bindCommand, 2, 0, NULL, "bindCommand")));
				return JS_TRUE;
		}
	*vp = JSVAL_VOID;
	return JS_TRUE;
}
