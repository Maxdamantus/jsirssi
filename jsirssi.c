#include "jsirssi.h"

/* get_irssi_dir() */

JSObject *js_test_printer(JSContext *cx);

void js_core_init(){
	module_register("js", "core");

	command_bind("js", NULL, (SIGNAL_FUNC)cmd_js);
	command_bind("js exec", NULL, (SIGNAL_FUNC)cmd_js_exec);
	command_bind("js list", NULL, (SIGNAL_FUNC)cmd_js_list);
	command_bind("js open", NULL, (SIGNAL_FUNC)cmd_js_open);

	JS_SetCStringsAreUTF8();
	js_rt = JS_NewRuntime(160L * 1024L * 1024L);

	js_cx = JS_NewContext(js_rt, 8192);
	JS_SetOptions(js_cx, JS_GetOptions(js_cx) |
		JSOPTION_STRICT | JSOPTION_WERROR | JSOPTION_COMPILE_N_GO | JSOPTION_XML |
		JSOPTION_JIT);
	JS_SetVersion(js_cx, JSVERSION_LATEST);
	JS_SetErrorReporter(js_cx, js_errorhandler);

	js_cmdline = js_newenv("<cmdline>", 0, 1);
	JS_SetGlobalObject(js_cx, js_setupglobal(js_cmdline));

	modules_hook_native(js_cx, js_test_printer, "test");
	modules_hook_native(js_cx, mod_irssi_get, "irssi");
}

void js_core_deinit(){
	for(;js_envs;)
		js_unregenv(js_envs);
	JS_GC(js_cx);
	JS_DestroyContext(js_cx);
	JS_DestroyRuntime(js_rt);

	command_unbind("js", (SIGNAL_FUNC)cmd_js);
	command_unbind("js exec", (SIGNAL_FUNC)cmd_js_exec);
	command_unbind("js list", (SIGNAL_FUNC)cmd_js_list);
	command_unbind("js open", (SIGNAL_FUNC)cmd_js_open);
}

JSObject *js_test_printer(JSContext *cx){
	JSObject *ret;
	jsval tmp;

	ret = JS_NewObject(cx, NULL, NULL, NULL);
	JS_DefineFunctions(cx, ret, js_funs);
	tmp = OBJECT_TO_JSVAL(JS_NewArrayObject(cx, 0, NULL));
	JS_SetProperty(cx, ret, "a", &tmp);
	return ret;
}

struct JSClass js_class_global = {
	"Global", JSCLASS_GLOBAL_FLAGS,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

struct JSClass js_class_envfreeer = JSCLASS_ROOBJ(
	"", JSCLASS_MARK_IS_TRACE | JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(1),
	JS_PropertyStub, JS_PropertyStub, js_envfreeer, JS_CLASS_TRACE(js_envtracer));

/*{
	"InvisibleObjectWhyDoesItMatter", JSCLASS_MARK_IS_TRACE | JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(1),
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, js_envfreeer,
	0, 0, 0, 0, 0, 0, JS_CLASS_TRACE(js_envtracer)
};*/

void js_envfreeer(JSContext *cx, JSObject *obj){
	struct js_env *env = JS_GetPrivate(js_cx, obj);

	fprintf(stderr, "Some object is being freed now\n");
	fflush(stderr);
	if(!env->live)
		js_freeenv(env);
}

void js_envtracer(JSTracer *trc, JSObject *obj){
	fprintf(stderr, "A trace\n");
	fflush(stderr);

	struct js_env *env = JS_GetPrivate(js_cx, obj);

	if(JSVAL_IS_TRACEABLE(env->exportval))
		JS_CallTracer(trc, JSVAL_TO_TRACEABLE(env->exportval), JSVAL_TRACE_KIND(env->exportval));
}

JSBool modules_fun_require(JSContext *cx, uintN argc, jsval *vp);

struct JSFunctionSpec js_funs[] = {
	{"print", js_fun_print, 1},
	{"require", modules_fun_require, 1},
	{NULL}
};

struct JSFunctionSpec js_funs_withenv[] = {
	{"JsExport", js_fun_JsExport, 1},
	{"JsLoad", js_fun_JsLoad, 1},
	{"JsExit", js_fun_JsExit, 0},
	{NULL}
};

void js_errorhandler(JSContext *cx, const char *msg, JSErrorReport *report){
	printtext_window(NULL, 0, "Error [%s:%u] %s", report->filename, report->lineno, msg);
}

JSObject *js_setupglobal(struct js_env *env){
	JSObject *oval, *uval, *fval;
	jsval t;
	struct JSFunctionSpec *funspec;

	oval = JS_NewGlobalObject(js_cx, &js_class_global);
	t = OBJECT_TO_JSVAL(oval);
	JS_SetProperty(js_cx, oval, "global", &t);
	JS_InitStandardClasses(js_cx, oval);
	JS_DefineFunctions(js_cx, oval, js_funs);
	uval = JS_NewObject(js_cx, &js_class_envfreeer, NULL, NULL);
	JS_SetPrivate(js_cx, uval, env);
	for(funspec = js_funs_withenv; funspec->name; funspec++){
		t = OBJECT_TO_JSVAL(fval = JS_GetFunctionObject(JS_NewFunction(js_cx,
			funspec->call, funspec->nargs, funspec->flags, NULL, funspec->name)));
		JS_SetReservedSlot(js_cx, fval, 0, PRIVATE_TO_JSVAL(env));
		JS_SetReservedSlot(js_cx, fval, 1, OBJECT_TO_JSVAL(uval));
		JS_SetProperty(js_cx, oval, funspec->name, &t);
	}
	env->refd = 1;
	return oval;
}

struct js_env *js_newenv(char *name, int dup, int hide){
	struct js_env *env;

	env = JS_malloc(js_cx, sizeof *env);
	env->name = dup? JS_strdup(js_cx, name) : name;
	env->ndup = dup;
	env->hide = hide;
	env->live = 1;
	env->refd = 0;
	env->exportval = JSVAL_VOID;
	JS_AddValueRoot(js_cx, &env->exportval);
	env->next = NULL;
	if(js_envs_last)
		js_envs_last->next = env;
	js_envs_last = env;
	if(!js_envs)
		js_envs = env;
	return env;
}

void js_unregenv(struct js_env *env){
	struct js_env *cur;

	fprintf(stderr, "unreg!\n");
	fflush(stderr);
	if(!env->live)
		return;
	env->live = 0;
	for(cur = js_envs; cur; cur = cur->next)
		if(cur->next == env)
			break;
	if(cur)
		cur->next = env->next;
	else if(js_envs == env)
		js_envs = env->next;
	else
		js_envs = js_envs_last = NULL;
	if(js_envs_last == env)
		js_envs_last = cur;
	JS_RemoveValueRoot(js_cx, &env->exportval);
	if(!env->refd)
		js_freeenv(env);
}

void js_freeenv(struct js_env *env){
	fprintf(stderr, "free(%s)\n", env->name);
	fflush(stderr);
	if(env->ndup)
		JS_free(js_cx, env->name);
	JS_free(js_cx, env);
}

struct js_env *js_loadscript(char *fname, int hide, int force){
	static char path[NAME_MAX + 1];
	struct js_env *env = NULL;
	JSScript *script;
	JSObject *glob, *scriptobj;
	jsval rval;

	if(!force)
		for(env = js_envs; env; env = env->next)
			if(!strcmp(fname, env->name))
				break;
	if(!env){
		env = js_newenv(fname, 1, hide);
		snprintf(path, NAME_MAX + 1, "%s/js/%s", get_irssi_dir(), fname);
		if(script = JS_CompileFile(js_cx, glob = js_setupglobal(env), path)){
			scriptobj = JS_NewScriptObject(js_cx, script);
			JS_AddObjectRoot(js_cx, &scriptobj);
			JS_ExecuteScript(js_cx, glob, script, &rval);
			JS_RemoveObjectRoot(js_cx, &scriptobj);
		}else{
			js_unregenv(env);
			return NULL;
		}
	}
	return env;
}

void cmd_js(char *data, void *server, WI_ITEM_REC *item){
	if(!*data)
		data = "list";
	command_runsub("js", data, server, item);
}

void cmd_js_exec(char *data, void *server, WI_ITEM_REC *item){
	jsval rval;
/*	
	if(!glob)
		glob = js_setupglobal(js_cmdline); */
/*	printtext_window(NULL, 0, "/js %s", data); */
	JS_EvaluateScript(js_cx, JS_GetGlobalObject(js_cx), data, strlen(data), "<cmdline>", 0, &rval);
	JS_GC(js_cx);
}

void cmd_js_list(char *data, void *server, WI_ITEM_REC *item){
	struct js_env *cur;

	printtext_window(NULL, 0, "-- JS contexts");
	for(cur = js_envs; cur; cur = cur->next)
		if(*data == 'a' || !cur->hide)
			printtext_window(NULL, 0, "- %s", cur->name);
	printtext_window(NULL, 0, "--");
}

void cmd_js_open(char *data, void *server, WI_ITEM_REC *item){
	js_loadscript(data, 0, 0);
}

JSBool js_fun_print(JSContext *cx, uintN argc, jsval *vp){
	int x;
	char *st;

	for(x = 0; x < argc; x++){
		printtext_window(NULL, 0, "%s", st = JS_EncodeString(cx, JS_ValueToString(cx, JS_ARGV(js_cx, vp)[x])));
		JS_free(cx, st);
	}
	JS_SET_RVAL(js_cx, vp, JSVAL_VOID);
	return JS_TRUE;
}

JSBool js_fun_JsExport(JSContext *cx, uintN argc, jsval *vp){
	jsval envv;

	JS_GetReservedSlot(js_cx, JSVAL_TO_OBJECT(JS_CALLEE(cx, vp)), 0, &envv);
	((struct js_env*)JSVAL_TO_PRIVATE(envv))->exportval = JS_ARGV(js_cx, vp)[0];
	JS_SET_RVAL(js_cx, vp, JSVAL_VOID);
	return JS_TRUE;
}

JSBool js_fun_JsLoad(JSContext *cx, uintN argc, jsval *vp){
	struct js_env *env;
	int x;
	char *lstr;

	lstr = JS_EncodeString(cx, JS_ValueToString(cx, JS_ARGV(js_cx, vp)[0]));
	fprintf(stderr, "BLARG!!!!\n");
	fflush(stderr);
	for(x = 0; js_modlist[x].name; x++){
		fprintf(stderr, "Testing for %s == %s\n", lstr, js_modlist[x].name);
		fflush(stderr);
		if(!strcmp(lstr, js_modlist[x].name)){
			fprintf(stderr, "Loading %s\n", lstr);
			fflush(stderr);
			JS_SET_RVAL(js_cx, vp, js_modlist[x].getter(js_modlist[x].data));
			JS_free(js_cx, lstr);
			return JS_TRUE;
		}
	}
	if(env = js_loadscript(lstr, 0, 0))
		JS_SET_RVAL(js_cx, vp, env->exportval);
	else
		JS_SET_RVAL(js_cx, vp, JSVAL_VOID);
	JS_free(js_cx, lstr);
	return JS_TRUE;
}

JSBool js_fun_JsExit(JSContext *cx, uintN argc, jsval *vp){
	jsval envv;

	JS_GetReservedSlot(js_cx, JSVAL_TO_OBJECT(JS_CALLEE(cx, vp)), 0, &envv);
	js_unregenv(JSVAL_TO_PRIVATE(envv));
	JS_SET_RVAL(js_cx, vp, JSVAL_VOID);
	return JS_TRUE;
}
