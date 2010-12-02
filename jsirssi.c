#include "jsirssi.h"

void js_core_init(){
	JSObject *global;

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

	js_cmdline = modules_create(js_cx, "<cmdline>", &global);
	JS_SetGlobalObject(js_cx, global);

	modules_hook_native(js_cx, js_mod_test, "test");
	modules_hook_native(js_cx, mod_irssi_get, "irssi");

	modules_runscript(js_cx, global, "rc");
}

void js_core_deinit(){
	modules_shutdown(js_cx);
	JS_GC(js_cx);
	JS_DestroyContext(js_cx);
	JS_DestroyRuntime(js_rt);

	command_unbind("js", (SIGNAL_FUNC)cmd_js);
	command_unbind("js exec", (SIGNAL_FUNC)cmd_js_exec);
	command_unbind("js list", (SIGNAL_FUNC)cmd_js_list);
	command_unbind("js open", (SIGNAL_FUNC)cmd_js_open);
}

struct JSFunctionSpec js_mod_test_funs[] = {
	{"print", js_fun_print, 1},
	{NULL}
};

jsval js_mod_test(JSContext *cx){
	JSObject *ret;
	jsval tmp;

	ret = JS_NewObject(cx, NULL, NULL, NULL);
	JS_DefineFunctions(cx, ret, js_mod_test_funs);
	tmp = OBJECT_TO_JSVAL(JS_NewArrayObject(cx, 0, NULL));
	JS_SetProperty(cx, ret, "a", &tmp);
	return OBJECT_TO_JSVAL(ret);
}

void js_errorhandler(JSContext *cx, const char *msg, JSErrorReport *report){
	printtext_window(NULL, 0, "Error [%s:%u] %s", report->filename, report->lineno, msg);
}

void cmd_js(char *data, void *server, WI_ITEM_REC *item){
	if(!*data)
		data = "list";
	command_runsub("js", data, server, item);
}

void cmd_js_exec(char *data, void *server, WI_ITEM_REC *item){
	jsval rval;

	JS_EvaluateScript(js_cx, JS_GetGlobalObject(js_cx), data, strlen(data), "<cmdline>", 0, &rval);
	JS_GC(js_cx);
}

void cmd_js_list(char *data, void *server, WI_ITEM_REC *item){
	int x;

	printtext_window(NULL, 0, "-- JS modules");
	for(x = 0; x < modules_c; x++)
		printtext_window(NULL, 0, "- %s", modules_a[x]->name);
/*	for(cur = js_envs; cur; cur = cur->next)
		if(*data == 'a' || !cur->hide)
			printtext_window(NULL, 0, "- %s", cur->name);
			*/
	printtext_window(NULL, 0, "--");
}

void cmd_js_open(char *data, void *server, WI_ITEM_REC *item){
	jsval rval;

	if(!modules_require(js_cx, data, &rval))
		printtext_window(NULL, 0, "Failed to load %s", data);
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
