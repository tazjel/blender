/* DO NOT EDIT THIS FILE - it is machine generated */
#include "jri.h"

/* Header for class Blender3DPlugin */

#ifndef _Blender3DPlugin_H_
#define _Blender3DPlugin_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct java_lang_String;
struct java_lang_Class;

/*******************************************************************************
 * Class Blender3DPlugin
 ******************************************************************************/

typedef struct Blender3DPlugin Blender3DPlugin;

#define classname_Blender3DPlugin	"Blender3DPlugin"

#define class_Blender3DPlugin(env) \
	((struct java_lang_Class*)JRI_FindClass(env, classname_Blender3DPlugin))

/*******************************************************************************
 * Public Methods
 ******************************************************************************/

#ifdef DEBUG

/*** public native SendMessage (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V ***/
extern JRI_PUBLIC_API(void)
Blender3DPlugin_SendMessage(JRIEnv* env, struct Blender3DPlugin* self, struct java_lang_String *a, struct java_lang_String *b, struct java_lang_String *c, struct java_lang_String *d);

/*** public native blenderURL (Ljava/lang/String;)V ***/
extern JRI_PUBLIC_API(void)
Blender3DPlugin_blenderURL(JRIEnv* env, struct Blender3DPlugin* self, struct java_lang_String *a);

#else /* !DEBUG */

/*** public native SendMessage (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V ***/
#define Blender3DPlugin_SendMessage(env, obj, a, b, c, d) \
	((void)JRI_CallMethod(env)(env, JRI_CallMethod_op, obj, methodID_Blender3DPlugin_SendMessage, a, b, c, d))

/*** public native blenderURL (Ljava/lang/String;)V ***/
#define Blender3DPlugin_blenderURL(env, obj, a) \
	((void)JRI_CallMethod(env)(env, JRI_CallMethod_op, obj, methodID_Blender3DPlugin_blenderURL, a))

#endif /* !DEBUG*/

/*** public native SendMessage (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V ***/
extern JRIMethodID FAR methodID_Blender3DPlugin_SendMessage;
#define name_Blender3DPlugin_SendMessage	"SendMessage"
#define sig_Blender3DPlugin_SendMessage 	"(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V"
#define use_Blender3DPlugin_SendMessage(env, clazz)	\
	(methodID_Blender3DPlugin_SendMessage =	\
		JRI_GetMethodID(env, clazz,	\
			name_Blender3DPlugin_SendMessage,	\
			sig_Blender3DPlugin_SendMessage))
#define unuse_Blender3DPlugin_SendMessage(env, clazz)	\
	(methodID_Blender3DPlugin_SendMessage = JRIUninitialized)

/*** public native blenderURL (Ljava/lang/String;)V ***/
extern JRIMethodID FAR methodID_Blender3DPlugin_blenderURL;
#define name_Blender3DPlugin_blenderURL	"blenderURL"
#define sig_Blender3DPlugin_blenderURL 	"(Ljava/lang/String;)V"
#define use_Blender3DPlugin_blenderURL(env, clazz)	\
	(methodID_Blender3DPlugin_blenderURL =	\
		JRI_GetMethodID(env, clazz,	\
			name_Blender3DPlugin_blenderURL,	\
			sig_Blender3DPlugin_blenderURL))
#define unuse_Blender3DPlugin_blenderURL(env, clazz)	\
	(methodID_Blender3DPlugin_blenderURL = JRIUninitialized)

/*******************************************************************************
 * IMPLEMENTATION SECTION: 
 * Define the IMPLEMENT_Blender3DPlugin symbol 
 * if you intend to implement the native methods of this class. This 
 * symbol makes the private and protected methods available. You should 
 * also call the register_Blender3DPlugin routine 
 * to make your native methods available.
 ******************************************************************************/

extern JRI_PUBLIC_API(struct java_lang_Class*)
use_Blender3DPlugin(JRIEnv* env);

extern JRI_PUBLIC_API(void)
unuse_Blender3DPlugin(JRIEnv* env);

extern JRI_PUBLIC_API(struct java_lang_Class*)
register_Blender3DPlugin(JRIEnv* env);

extern JRI_PUBLIC_API(void)
unregister_Blender3DPlugin(JRIEnv* env);

#ifdef IMPLEMENT_Blender3DPlugin

/*******************************************************************************
 * Native Methods: 
 * These are the signatures of the native methods which you must implement.
 ******************************************************************************/

/*** public native SendMessage (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V ***/
extern JRI_PUBLIC_API(void)
native_Blender3DPlugin_SendMessage(JRIEnv* env, struct Blender3DPlugin* self, struct java_lang_String *a, struct java_lang_String *b, struct java_lang_String *c, struct java_lang_String *d);

/*** public native blenderURL (Ljava/lang/String;)V ***/
extern JRI_PUBLIC_API(void)
native_Blender3DPlugin_blenderURL(JRIEnv* env, struct Blender3DPlugin* self, struct java_lang_String *a);

/*******************************************************************************
 * Implementation Methods: 
 * You should only use these from within the native method definitions.
 ******************************************************************************/

#ifdef DEBUG

/*** <init> ()V ***/
extern JRI_PUBLIC_API(struct Blender3DPlugin*)
Blender3DPlugin_new(JRIEnv* env, struct java_lang_Class* clazz);

#else /* !DEBUG */

/*** <init> ()V ***/
#define Blender3DPlugin_new(env, clazz)	\
	((struct Blender3DPlugin*)JRI_NewObject(env)(env, JRI_NewObject_op, clazz, methodID_Blender3DPlugin_new))

#endif /* !DEBUG*/

/*** <init> ()V ***/
extern JRIMethodID FAR methodID_Blender3DPlugin_new;
#define name_Blender3DPlugin_new	"<init>"
#define sig_Blender3DPlugin_new 	"()V"
#define use_Blender3DPlugin_new(env, clazz)	\
	(methodID_Blender3DPlugin_new =	\
		JRI_GetMethodID(env, clazz,	\
			name_Blender3DPlugin_new,	\
			sig_Blender3DPlugin_new))
#define unuse_Blender3DPlugin_new(env, clazz)	\
	(methodID_Blender3DPlugin_new = JRIUninitialized)

#endif /* IMPLEMENT_Blender3DPlugin */

#ifdef __cplusplus
} /* extern "C" */

/*******************************************************************************
 * C++ Definitions
 ******************************************************************************/

#include "netscape_plugin_Plugin.h"

struct Blender3DPlugin : public netscape_plugin_Plugin {

	static struct java_lang_Class* _use(JRIEnv* env) {
		return use_Blender3DPlugin(env);
	}

	static void _unuse(JRIEnv* env) {
		unuse_Blender3DPlugin(env);
	}

	static struct java_lang_Class* _register(JRIEnv* env) {
		return register_Blender3DPlugin(env);
	}

	static void _unregister(JRIEnv* env) {
		unregister_Blender3DPlugin(env);
	}

	static struct java_lang_Class* _class(JRIEnv* env) {
		return class_Blender3DPlugin(env);
	}

	/* Public Methods */
	/*** public native SendMessage (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V ***/
	void SendMessage(JRIEnv* env, struct java_lang_String *a, struct java_lang_String *b, struct java_lang_String *c, struct java_lang_String *d) {
		Blender3DPlugin_SendMessage(env, this, a, b, c, d);
	}

	/*** public native blenderURL (Ljava/lang/String;)V ***/
	void blenderURL(JRIEnv* env, struct java_lang_String *a) {
		Blender3DPlugin_blenderURL(env, this, a);
	}

#ifdef IMPLEMENT_Blender3DPlugin

#endif /* IMPLEMENT_Blender3DPlugin */
};

#endif /* __cplusplus */

#endif /* Class Blender3DPlugin */

