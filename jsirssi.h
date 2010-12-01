#include <stdlib.h>
#include <string.h>

#include <irssi/src/common.h>
#include <irssi/src/core/modules.h>
#include <irssi/src/core/commands.h>
#include <irssi/src/irc/core/irc.h>
#include <irssi/src/fe-common/core/formats.h>
#include <irssi/src/fe-common/core/printtext.h>
#include <irssi/src/core/levels.h>

#define XP_UNIX
#include <js/jsapi.h>

#define MODULE_NAME "js"

#define JSCLASS_ROOBJ(name, flags, get, set, finalise, trace) { \
	name, flags, JS_PropertyStub, JS_PropertyStub, get, set, \
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, finalise, \
	0, 0, 0, 0, 0, 0, trace }

/* begin jsirssi.c */

void js_core_init();
void js_core_deinit();

void cmd_js(char *data, void *server, WI_ITEM_REC *item);
void cmd_js_exec(char *data, void *server, WI_ITEM_REC *item);
void cmd_js_list(char *data, void *server, WI_ITEM_REC *item);
void cmd_js_open(char *data, void *server, WI_ITEM_REC *item);

struct js_env {
	jsval exportval;
	struct js_env *next;
	char ndup, hide, live, refd;
	char *name;
};

struct js_env *js_envs, *js_envs_last, *js_cmdline;

JSRuntime *js_rt;
JSContext *js_cx;
struct JSClass js_class_global;
struct JSClass js_class_envfreeer;
struct JSFunctionSpec js_funs[20];
struct JSFunctionSpec js_funs_withenv[20];

struct js_env *js_newenv(char *name, int dup, int hide);
void js_unregenv(struct js_env *env);
void js_freeenv(struct js_env *env);
struct js_env *js_loadscript(char *fname, int hide, int force);

JSObject *js_setupglobal(struct js_env *env);

void js_errorhandler(JSContext *cx, const char *msg, JSErrorReport *report);
void js_envfreeer(JSContext *cx, JSObject *obj);
void js_envtracer(JSTracer *trc, JSObject *obj);
JSBool js_fun_print(JSContext *cx, uintN argc, jsval *vp);
JSBool js_fun_JsExport(JSContext *cx, uintN argc, jsval *vp);
JSBool js_fun_JsLoad(JSContext *cx, uintN argc, jsval *vp);
JSBool js_fun_JsExit(JSContext *cx, uintN argc, jsval *vp);

/* end jsirssi.c */
/* begin modlist.c */

struct js_moditem {
	char *name;
	jsval (*getter)(void*);
	void *data;
} js_modlist[10];

/* end modlist.c */
/* begin mod_irssi.c */

JSObject* mod_irssi_get(JSContext*);

/* end mod_irssi.c */
/* begin modules.c */

typedef JSObject *module_hook(JSContext*);
struct module {
	JSObject *exports;
	int id, type;
	char *name;
	module_hook *hook;
};
struct module *modules_create(JSContext *cx, const char *moduleid, JSObject **global);
struct module *modules_hook_native(JSContext *cx, module_hook *hook, const char *moduleid);
void modules_delete(JSContext *cx, struct module *module);
JSObject *modules_require(JSContext *cx, const char *moduleid);

/* end modules.c */
