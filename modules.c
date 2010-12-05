#include "jsirssi.h"

enum module_types {
	MODULE_NORMAL = 1, MODULE_NATIVE = 2, MODULE_EXPORTERFUNC = 4
};

static struct JSClass modules_class_global = {
	"Global", JSCLASS_GLOBAL_FLAGS,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

static struct JSClass modules_class_exports = {
	"Exports", 0,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

static struct JSClass modules_class_module = {
	"Module", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

static JSBool modules_fun_require(JSContext *cx, uintN argc, jsval *vp){
	char *moduleid;
	jsval exports;

	moduleid = JS_EncodeString(cx, JS_ValueToString(cx, JS_ARGV(cx, vp)[0]));
	if(modules_require(cx, moduleid, &exports))
		JS_SET_RVAL(cx, vp, exports);
	else /* todo: throw an error */
		JS_SET_RVAL(cx, vp, JSVAL_VOID);
	return JS_TRUE;
}

/* module.remove() */
static JSBool modules_module_fun_remove(JSContext *cx, uintN argc, jsval *vp){
	struct module *module;
	jsval tmp;

	JS_GetReservedSlot(js_cx, JSVAL_TO_OBJECT(JS_CALLEE(cx, vp)), 0, &tmp);
	if(module = JS_GetPrivate(cx, JSVAL_TO_OBJECT(tmp)))
		modules_delete(cx, module);
	JS_SET_RVAL(cx, vp, JSVAL_VOID);
	return JS_TRUE;
}

/* x = module.exporter */
static JSBool modules_module_get_exporter(JSContext *cx, JSObject *obj, jsid id, jsval *vp){
	struct module *module;

	*vp = ((module = JS_GetPrivate(cx, obj)) && module->type & MODULE_EXPORTERFUNC)?
		module->exports : JSVAL_VOID;
	return JS_TRUE;
}

/* module.exporter = x */
static JSBool modules_module_set_exporter(JSContext *cx, JSObject *obj, jsid id, jsval *vp){
	struct module *module;

	if(module = JS_GetPrivate(cx, obj))
		if(JSVAL_IS_VOID(*vp)){
			module->type &= ~MODULE_EXPORTERFUNC;
			module->exports = JSVAL_VOID;
		}else{
			module->type |= MODULE_EXPORTERFUNC;
			module->exports = *vp;
		}
	return JS_TRUE;
}

struct module *modules_create(JSContext *cx, const char *moduleid, JSObject **global){
	struct module *module;
	JSObject *funtmp;

	if(modules_c == 1000)
		return NULL;
	module = JS_malloc(cx, sizeof *module);
	module->type = MODULE_NORMAL;
	module->name = JS_strdup(cx, moduleid);
	modules_a[module->id = modules_c++] = module;

	*global = JS_NewGlobalObject(cx, &modules_class_global);
	JS_InitStandardClasses(cx, *global);

	/* exports */
	module->exports = OBJECT_TO_JSVAL(JS_DefineObject(cx, *global, "exports", &modules_class_exports, NULL, 0));
	JS_AddValueRoot(cx, &module->exports);

	/* global */
	JS_DefineProperty(cx, *global, "global", OBJECT_TO_JSVAL(*global), NULL, NULL, 0);

	/* require */
	JS_DefineFunction(cx, *global, "require", modules_fun_require, 1, 0);

	/* module */
	module->moduleobj = JS_DefineObject(cx, *global, "module", &modules_class_module, NULL, 0);
	JS_AddObjectRoot(cx, &module->moduleobj);
	JS_SetPrivate(cx, module->moduleobj, module);

	/* module.id */
	JS_DefineProperty(cx, module->moduleobj, "id",
		STRING_TO_JSVAL(JS_NewStringCopyZ(cx, moduleid)), NULL, NULL, JSPROP_READONLY);

	/* module.remove */
	JS_DefineProperty(cx, module->moduleobj, "remove",
		OBJECT_TO_JSVAL(funtmp = JS_GetFunctionObject(JS_NewFunction(cx,
		modules_module_fun_remove, 0, 0, NULL, "remove"))), NULL, NULL, 0);
	JS_SetReservedSlot(cx, funtmp, 0, OBJECT_TO_JSVAL(module->moduleobj));

	/* module.exporter (getter/setter) */
	JS_DefineProperty(cx, module->moduleobj, "exporter", JSVAL_VOID,
		modules_module_get_exporter, modules_module_set_exporter, 0);

	return module;
}

struct module *modules_hook_native(JSContext *cx, module_hook *hook, const char *moduleid){
	struct module *module;

	if(modules_c == 1000)
		return NULL;
	module = JS_malloc(cx, sizeof *module);
	module->type = MODULE_NATIVE;
	module->name = JS_strdup(cx, moduleid);
	module->hook = hook;
	modules_a[modules_c++] = module;

	module->moduleobj = JS_NewObject(cx, &modules_class_module, NULL, NULL);
	JS_AddObjectRoot(cx, &module->moduleobj);
	JS_SetPrivate(cx, module->moduleobj, module);

	return module;
}

void modules_delete(JSContext *cx, struct module *module){
	jsval tmp, fval;

	(modules_a[module->id] = modules_a[--modules_c])->id = module->id;

	JS_free(cx, module->name);
	if(module->type & MODULE_NORMAL)
		JS_RemoveValueRoot(cx, &module->exports);
	JS_SetPrivate(cx, module->moduleobj, NULL);
	JS_GetProperty(cx, module->moduleobj, "onremove", &fval);
	if(!JSVAL_IS_VOID(fval))
		JS_CallFunctionValue(cx, NULL, fval, 0, NULL, &tmp);
	JS_RemoveObjectRoot(cx, &module->moduleobj);

	JS_free(cx, module);
}

void modules_shutdown(JSContext *cx){
	while(modules_c)
		modules_delete(cx, modules_a[0]);
}

JSBool modules_runscript(JSContext *cx, JSObject *global, const char *moduleid){
	static char path[NAME_MAX + 1];
	JSObject *scriptobj;
	JSScript *script;
	jsval rval;

	snprintf(path, NAME_MAX + 1, "%s/js/%s.js", get_irssi_dir(), moduleid);
	if(script = JS_CompileFile(cx, global, path)){
		scriptobj = JS_NewScriptObject(cx, script);
		JS_AddObjectRoot(cx, &scriptobj);
		JS_ExecuteScript(cx, global, script, &rval);
		JS_RemoveObjectRoot(cx, &scriptobj);
		return JS_TRUE;
	}
	return JS_FALSE;
}

JSBool modules_require(JSContext *cx, const char *moduleid, jsval *rval){
	struct module *module;
	JSObject *global;
	int x;

	for(x = 0; x < modules_c; x++)
		if(!strcmp(modules_a[x]->name, moduleid))
			break;

	if(x == modules_c){
		module = modules_create(cx, moduleid, &global);
		if(!modules_runscript(cx, global, moduleid)){
			modules_delete(cx, module);
			return JS_FALSE;
		}
	}else
		module = modules_a[x];

	if(module->type & MODULE_NATIVE)
		*rval = module->hook(cx);
	else if(module->type & MODULE_EXPORTERFUNC)
		JS_CallFunctionValue(cx, NULL, module->exports, 0, NULL, rval);
	else
		*rval = module->exports;

	return JS_TRUE;
}
