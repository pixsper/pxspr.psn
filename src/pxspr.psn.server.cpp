// MIT License
// 
// Copyright (c) 2021 Pixsper Ltd.
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <asio.hpp>
#include <psn_lib.hpp>
#include <ext.h>

#include "../build/version.h"

#define PXSPR_PSN_SERVER_CLASSNAME "pxspr.psn.server"


typedef struct _pxspr_psn_server
{
	t_object object;	// The object
	void* outlet;		// Dump outlet

} t_pxspr_psn_server;



void* pxspr_psn_server_new(t_symbol* s, long argc, t_atom* argv);
void pxspr_psn_server_free(t_pxspr_psn_server* x);
void pxspr_psn_server_assist(t_pxspr_psn_server* x, void* b, long m, long a, char* s);

void pxspr_psn_server_getversion(t_pxspr_psn_server* x);
void pxspr_psn_server_geturl(t_pxspr_psn_server* x);

static t_class* pxspr_psn_server_class = nullptr;



void ext_main(void* r)
{
	common_symbols_init();

	t_class* c = class_new(PXSPR_PSN_SERVER_CLASSNAME,
	                       (method)pxspr_psn_server_new,
	                       (method)pxspr_psn_server_free,
	                       sizeof(t_pxspr_psn_server),
	                       (method)nullptr,
	                       A_GIMME,
	                       0);

	class_addmethod(c, (method)pxspr_psn_server_assist, "assist", A_CANT, 0);

	class_addmethod(c, (method)pxspr_psn_server_getversion, "getversion", 0);
	class_addmethod(c, (method)pxspr_psn_server_geturl, "geturl", 0);

	class_register(CLASS_BOX, c);
	pxspr_psn_server_class = c;

	object_post(nullptr, "%s v%i.%i.%i - Copyright (c) %i Pixsper Ltd. - %s", 
		PXSPR_PSN_SERVER_CLASSNAME,
		PXSPR_PSN_VERSION_MAJOR, PXSPR_PSN_VERSION_MINOR, PXSPR_PSN_VERSION_BUGFIX, 
		PXSPR_PSN_COPYRIGHT_YEAR, PXSPR_URL);
}



void* pxspr_psn_server_new(t_symbol* s, long argc, t_atom* argv)
{
	t_pxspr_psn_server* x = nullptr;

	if ((x = (t_pxspr_psn_server*)object_alloc(pxspr_psn_server_class)))
	{
		x->outlet = outlet_new((t_object*)x, nullptr);
		attr_args_process(x, argc, argv);
	}

	return x;
}

void pxspr_psn_server_free(t_pxspr_psn_server* x)
{
	
}

void pxspr_psn_server_assist(t_pxspr_psn_server* x, void* b, long m, long a, char* s)
{
	if (m == ASSIST_INLET)
		sprintf(s, PXSPR_PSN_SERVER_CLASSNAME);
	else
		sprintf(s, "dump");
}

void pxspr_psn_server_getversion(t_pxspr_psn_server* x)
{
	t_atom av[3];
	atom_setlong(av, PXSPR_PSN_VERSION_MAJOR);
	atom_setlong(av + 1, PXSPR_PSN_VERSION_MINOR);
	atom_setlong(av + 2, PXSPR_PSN_VERSION_BUGFIX);
	
	outlet_anything(x->outlet, _sym_version, 3,  av);
}

void pxspr_psn_server_geturl(t_pxspr_psn_server* x)
{
	t_atom av;
	atom_setsym(&av, gensym(PXSPR_URL));
	outlet_anything(x->outlet, _sym_url, 1,  &av);
}