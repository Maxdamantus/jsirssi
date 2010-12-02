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

/* begin modules.c */

typedef JSObject *module_hook(JSContext*);

struct module {
	int id, type;
	char *name;
	JSObject *moduleobj;
	union {
		JSObject *exports;
		module_hook *hook;
	} o;
} *modules_a[1000];
int modules_c;

struct module *modules_create(JSContext *cx, const char *moduleid, JSObject **global);
struct module *modules_hook_native(JSContext *cx, module_hook *hook, const char *moduleid);
void modules_delete(JSContext *cx, struct module *module);
void modules_shutdown(JSContext *cx);
JSObject *modules_require(JSContext *cx, const char *moduleid);

/* end modules.c */
/* begin jsirssi.c */

void js_core_init();
void js_core_deinit();

void cmd_js(char *data, void *server, WI_ITEM_REC *item);
void cmd_js_exec(char *data, void *server, WI_ITEM_REC *item);
void cmd_js_list(char *data, void *server, WI_ITEM_REC *item);
void cmd_js_open(char *data, void *server, WI_ITEM_REC *item);

JSRuntime *js_rt;
JSContext *js_cx;

struct JSClass js_class_global;

struct module *js_cmdline;

module_hook js_mod_test;

void js_errorhandler(JSContext *cx, const char *msg, JSErrorReport *report);
JSBool js_fun_print(JSContext *cx, uintN argc, jsval *vp);

/* end jsirssi.c */
/* begin mod_irssi.c */

module_hook mod_irssi_get;

/* end mod_irssi.c */

