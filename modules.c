#include "jsirssi.h"

enum module_types {
	MODULE_NORMAL, MODULE_NATIVE
};

static struct JSClass modules_class_global = {
	"Global", JSCLASS_GLOBAL_FLAGS,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

static struct JSClass modules_class_exports = {
	"Exports", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

static JSBool modules_fun_require(JSContext *cx, uintN argc, jsval *vp){
	char *moduleid;
	JSObject *exports;

	moduleid = JS_EncodeString(cx, JS_ValueToString(cx, JS_ARGV(cx, vp)[0]));
	if(exports = modules_require(cx, moduleid))
		JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(exports));
	else /* todo: throw an error */
		JS_SET_RVAL(cx, vp, JSVAL_VOID);
	return JS_TRUE;
}

struct module *modules_create(JSContext *cx, const char *moduleid, JSObject **global){
	struct module *module;
	jsval tmp;

	if(modules_c == 1000)
		return NULL;
	module = JS_malloc(cx, sizeof *module);
	module->type = MODULE_NORMAL;
	module->name = JS_strdup(cx, moduleid);
	module->o.exports = JS_NewObject(cx, &modules_class_exports, NULL, NULL);
	modules_a[module->id = modules_c++] = module;

	JS_AddObjectRoot(cx, &module->o.exports);

	*global = JS_NewGlobalObject(cx, &modules_class_global);
	JS_InitStandardClasses(cx, *global);

	tmp = OBJECT_TO_JSVAL(module->o.exports);
	JS_SetProperty(cx, *global, "exports", &tmp);
	tmp = OBJECT_TO_JSVAL(*global);
	JS_SetProperty(cx, *global, "global", &tmp);
	tmp = OBJECT_TO_JSVAL(JS_GetFunctionObject(JS_NewFunction(
		cx, modules_fun_require, 1, 0, NULL, "require")));
	JS_SetProperty(cx, *global, "require", &tmp);

	return module;
}

struct module *modules_hook_native(JSContext *cx, module_hook *hook, const char *moduleid){
	struct module *module;

	if(modules_c == 1000)
		return NULL;
	module = JS_malloc(cx, sizeof *module);
	module->type = MODULE_NATIVE;
	module->name = JS_strdup(cx, moduleid);
	module->o.hook = hook;
	modules_a[modules_c++] = module;

	return module;
}

void modules_delete(JSContext *cx, struct module *module){
	(modules_a[module->id] = modules_a[--modules_c])->id = module->id;

	JS_free(cx, module->name);
	if(module->name == MODULE_NORMAL)
		JS_RemoveObjectRoot(cx, &module->o.exports);
	JS_free(cx, module);
}

void modules_shutdown(JSContext *cx){
	while(modules_c)
		modules_delete(cx, modules_a[0]);
}

JSObject *modules_require(JSContext *cx, const char *moduleid){
	static char path[NAME_MAX + 1];
	struct module *module;
	JSObject *global, *scriptobj;
	JSScript *script;
	jsval rval;
	int x;

	for(x = 0; x < modules_c; x++)
		if(!strcmp(modules_a[x]->name, moduleid))
			break;

	if(x == modules_c){
		snprintf(path, NAME_MAX + 1, "%s/js/%s", get_irssi_dir(), moduleid);
		module = modules_create(cx, moduleid, &global);
		if(script = JS_CompileFile(cx, global, path)){
			scriptobj = JS_NewScriptObject(cx, script);
			JS_AddObjectRoot(cx, &scriptobj);
			JS_ExecuteScript(cx, global, script, &rval);
			JS_RemoveObjectRoot(cx, &scriptobj);
		}else{
			modules_delete(cx, module);
			return NULL;
		}
	}else
		module = modules_a[x];

	if(module->type == MODULE_NATIVE)
		return module->o.hook(cx);
	return module->o.exports;
}
