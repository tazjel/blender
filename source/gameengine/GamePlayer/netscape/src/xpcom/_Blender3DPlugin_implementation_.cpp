/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 * Implementation of a peer for a Blender plugin.
 */

#include <stdio.h>

#include "nsCOMPtr.h"

#ifdef MOZ_NOT_NET
#include "nsIModule.h"
#else
#include "nsIComponentManager.h"
#endif

#include "nsIServiceManager.h"
#include "nsIClassInfo.h"
#include "nsIGenericFactory.h"

#include "Blender3DPlugin.h"
#include "nsClassInfoMixin.h"

#include "_Blender3DPlugin_implementation_.h"
#include "PLB_script_bindings.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

static void _BI_log(const char* msg);

#define GENERATE_LOG

//
//  The XPCOM preamble: this is needed for registration of the class,
//  with IID and CID at application initialization.
//

// Another UUID: the CID identifies an implementation. Implementation
// ID + interface ID make up the 
const char* BLENDERPLUGIN_CID_STRING = "a8aa327a-b50e-4d44-bed4-29284166aa28";

#define BLENDERPLUGIN_CID_VALUE \
{ 0xa8aa327a, 0xb50e, 0x4d44, { 0xbe, 0xd4, 0x29, 0x28, 0x41, 0x66, 0xaa, 0x28 } }

// The contract:
const char* BLENDERPLUGIN_CONTRACT = 
"@blender3d.com/plugin;1";

// These macros turn the CIDs into usable CID values
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(BLENDERPLUGIN_CID,    BLENDERPLUGIN_CID_VALUE);

//
// Generic factory facilities: 
//
NS_GENERIC_FACTORY_CONSTRUCTOR(_Blender3DPlugin_Implementation_)

// Something like this needed to make the .so register with
// mozilla. Is it enough? This registers the component, with a certain
// CID. We also need something to tell that we support certain
// interfaces. Then we need a factory to produce instantiations of
// classes that will perform the actual functions.
extern "C" PR_IMPLEMENT(nsresult)
	NSRegisterSelf(
		nsISupports* aServMgr,
		const char* aPath
		)
{
	nsresult rv;
	
	_BI_log("--> Registering self:(12:00, 21-01 build for /opt/mozilla)\n"); 

	nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
	if (NS_FAILED(rv)) return rv;
	
#ifdef MOZ_NOT_NET
	nsCOMPtr<nsIComponentManagerObsolete> compMgr = 
#else
	nsCOMPtr<nsIComponentManager> compMgr = 
#endif
		do_GetService(kComponentManagerCID, &rv);
	if (NS_FAILED(rv)) return rv;

	rv = compMgr->RegisterComponent(BLENDERPLUGIN_CID,
					"Blender3DPlugin",
					BLENDERPLUGIN_CONTRACT,
					aPath,
					PR_TRUE,
					PR_TRUE
					);

	if (NS_FAILED(rv)) return rv;

	_BI_log("--> Registering self:::::success"); 

	return NS_OK;
}

extern "C" PR_IMPLEMENT(nsresult)
	NSUnregisterSelf(
		nsISupports* aServMgr,
		const char* aPath
		)
{
	nsresult rv;

	_BI_log("--> Unregistering self:(12:00, 21-01 build for /nzc/.mozilla)\n"); 

	nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
	if (NS_FAILED(rv)) return rv;

#ifdef MOZ_NOT_NET
	nsCOMPtr<nsIComponentManagerObsolete> compMgr = 
#else
	nsCOMPtr<nsIComponentManager> compMgr = 
#endif
		do_GetService(kComponentManagerCID, &rv);
	if (NS_FAILED(rv)) return rv;

	rv = compMgr->UnregisterComponent(BLENDERPLUGIN_CID,
					  aPath);

	if (NS_FAILED(rv)) return rv;
	_BI_log("--> Unregistering self:::::success"); 
	return NS_OK;
}

// Not needed, but good for getting some diagnostics

static NS_METHOD
blenderContract_register(
	nsIComponentManager * aCompMgr,
	nsIFile *aPath,
	const char *registryLocation,
	const char *componenttype,
	const nsModuleComponentInfo *aInfo
	)
{
	_BI_log("blenderContract_register\n"); 	
	_BI_log("registry:\n"); 	
	_BI_log(registryLocation);
	_BI_log("component:\n"); 	
	_BI_log(componenttype);
	return NS_OK;
}

static NS_METHOD
blenderContract_unregister(
	nsIComponentManager * aCompMgr,
	nsIFile *aPath,
	const char *registryLocation,
	const nsModuleComponentInfo *aInfo
	)
{
	_BI_log("blenderContract_unregister\n"); 	
	_BI_log("registry:\n"); 	
	_BI_log(registryLocation);
	return NS_OK;
}

//
//
static nsModuleComponentInfo components[] = 
{
	{
		"Blender3DPlugin Component",
		BLENDERPLUGIN_CID,
		BLENDERPLUGIN_CONTRACT,

		// factory. name is generated by the
		// NS_GENERIC_FACTORY macro!! :
		_Blender3DPlugin_Implementation_Constructor, 

		// registration proc :
		blenderContract_register, 

		// unregistration proc:
		blenderContract_unregister,

		// factory destructor:
		NULL,
// 		_Blender3DPlugin_Implementation_Destructor, 

		// get interface proc
		NULL,

		// get languagehelper
		NULL,

		// classinfoglobal **
		NULL,

		// flags
		0
	}
};

void _components_anti_optimize_removal_hook_DO_NOT_CALL(void)
{
	nsModuleComponentInfo *p = components;
	p = 0;
}

// Expose our claim to implement Blender3DPlugin and nsIClassInfo
NS_IMPL_ISUPPORTS2(_Blender3DPlugin_Implementation_, Blender3DPlugin, nsIClassInfo)

//
// Here the real class starts:
//

_Blender3DPlugin_Implementation_::_Blender3DPlugin_Implementation_()
{
	NS_INIT_ISUPPORTS();
	_BI_log("_Blender3DPlugin_Implementation_::constructor\n"); 
	/* member initializers and constructor code */
	pluginref = NULL;  
}

void
_Blender3DPlugin_Implementation_::set_plugin_reference(
	void* ref
	)
{
	_BI_log("_Blender3DPlugin_Implementation_::set_plugin_reference\n"); 
	/* Implicit... but I'd rather not pull in any plugin specifics
	 * already at this point... :/ */
	pluginref = ref;
}


_Blender3DPlugin_Implementation_::~_Blender3DPlugin_Implementation_()
{
	/* destructor code */
}

/* void blenderURL (in string url); */
NS_IMETHODIMP 
_Blender3DPlugin_Implementation_::BlenderURL(
	const char *url
	)
{
	_BI_log("_Blender3DPlugin_Implementation_::BlenderURL\n"); 
	if (pluginref) {
		PLB_native_blenderURL_func(pluginref, 
					   url);
	}
//	return NS_ERROR_NOT_IMPLEMENTED;
	return NS_OK;
}

/* void SendMessage (in string to, in string from, in string subject, in string body); */
NS_IMETHODIMP 
_Blender3DPlugin_Implementation_::SendMessage(
	const char *to, 
	const char *from, 
	const char *subject, 
	const char *body
	)
{
	_BI_log("_Blender3DPlugin_Implementation_::SendMessage\n"); 
	if (pluginref) {
		PLB_native_SendMessage_func(pluginref, 
					    to,
					    from,
					    subject,
					    body);
	}
//	return NS_ERROR_NOT_IMPLEMENTED;
	return NS_OK;
}



static void _BI_log(const char* msg)
{
#ifdef GENERATE_LOG
	FILE* fp = fopen("/tmp/plugin_log","a");
	if (fp) {
		fprintf(fp, "_Blender3dPlugin_implementation:: %s\n", msg); 
		fflush(fp);
		fclose (fp);
	}
#endif
}
