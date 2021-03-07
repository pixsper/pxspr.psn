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

#define _USE_MATH_DEFINES
#include <cmath>

#include <asio.hpp>
#include <psn_lib.hpp>
#include <ext.h>

#include "../build/version.h"

#define PXSPR_PSN_CLIENT_CLASSNAME "pxspr.psn.client"


typedef struct _pxspr_psn_client
{
	t_object object;
	void* outlet_trackers;
	void* outlet_dump;

	std::array<char, psn::MAX_UDP_PACKET_SIZE>* buffer;
	asio::ip::udp::endpoint* remote_endpoint;
	asio::io_context* io_context;
	
	t_systhread io_thread;
	t_systhread_mutex mutex;
	void* output_data_clock;
	void* output_info_clock;
	
	asio::ip::udp::socket* socket;
	psn::psn_decoder* psn_decoder;
	t_uint8 last_frame_id ;
	

} t_pxspr_psn_client;



void* pxspr_psn_client_new(t_symbol* s, long argc, t_atom* argv);
void pxspr_psn_client_free(t_pxspr_psn_client* x);
void pxspr_psn_client_assist(t_pxspr_psn_client* x, void* b, long m, long a, char* s);

void pxspr_psn_client_getversion(t_pxspr_psn_client* x);
void pxspr_psn_client_geturl(t_pxspr_psn_client* x);

void* pxspr_psn_client_iothreadproc(t_pxspr_psn_client* x);

void pxspr_psn_client_createsocket(t_pxspr_psn_client* x);
void pxspr_psn_client_startreceive(t_pxspr_psn_client* x);

void pxspr_psn_client_outputdata(t_pxspr_psn_client* x);
void pxspr_psn_client_outputinfo(t_pxspr_psn_client* x);

static t_class* pxspr_psn_client_class = nullptr;

static t_symbol* _sym_speed;
static t_symbol* _sym_status;
static t_symbol* _sym_acceleration;
static t_symbol* _sym_targetposition;
static t_symbol* _sym_timestamp;

void ext_main(void* r)
{
	common_symbols_init();

	_sym_speed = gensym("speed");
	_sym_status = gensym("status");
	_sym_acceleration = gensym("acceleration");
	_sym_targetposition = gensym("targetposition");
	_sym_timestamp = gensym("timestamp");
	
	t_class* c = class_new(PXSPR_PSN_CLIENT_CLASSNAME,
	                       (method)pxspr_psn_client_new,
	                       (method)pxspr_psn_client_free,
	                       sizeof(t_pxspr_psn_client),
	                       (method)nullptr,
	                       A_GIMME,
	                       0);

	class_addmethod(c, (method)pxspr_psn_client_assist, "assist", A_CANT, 0);
	
	class_addmethod(c, (method)pxspr_psn_client_getversion, "getversion", 0);
	class_addmethod(c, (method)pxspr_psn_client_geturl, "geturl", 0);

	
	class_register(CLASS_BOX, c);
	pxspr_psn_client_class = c;

	object_post(nullptr, "%s v%i.%i.%i - Copyright (c) %i Pixsper Ltd. - %s", 
		PXSPR_PSN_CLIENT_CLASSNAME,
		PXSPR_PSN_VERSION_MAJOR, PXSPR_PSN_VERSION_MINOR, PXSPR_PSN_VERSION_BUGFIX, 
		PXSPR_PSN_COPYRIGHT_YEAR, PXSPR_URL);
}



void* pxspr_psn_client_new(t_symbol* s, long argc, t_atom* argv)
{
	t_pxspr_psn_client* x = nullptr;

	if ((x = (t_pxspr_psn_client*)object_alloc(pxspr_psn_client_class)))
	{
		x->outlet_dump = outlet_new((t_object*)x, nullptr);
		x->outlet_trackers = outlet_new((t_object*)x, nullptr);

		x->buffer = new std::array<char, psn::MAX_UDP_PACKET_SIZE>();
		x->remote_endpoint = new asio::ip::udp::endpoint();
		x->io_context = new asio::io_context;
		x->io_thread = nullptr;
		x->mutex = nullptr;
		x->output_data_clock = nullptr;
		x->output_info_clock = nullptr;
		x->socket = nullptr;
		x->psn_decoder = new psn::psn_decoder;
		x->last_frame_id = 0;


		if (systhread_create((method)pxspr_psn_client_iothreadproc, 
			x, 0, 0, 0, &x->io_thread))
		{
			object_error((t_object*)x, "Failed to create IO thread");
			return x;
		}

		if (systhread_mutex_new(&x->mutex, SYSTHREAD_MUTEX_NORMAL))
		{
			object_error((t_object*)x, "Failed to create thread mutex");
			return x;
		}

		x->output_data_clock = clock_new(x,(method)pxspr_psn_client_outputdata);
		x->output_info_clock = clock_new(x,(method)pxspr_psn_client_outputinfo);
		
		attr_args_process(x, argc, argv);

		pxspr_psn_client_createsocket(x);
	}

	return x;
}

void pxspr_psn_client_free(t_pxspr_psn_client* x)
{
	if (x->io_thread)
	{
		x->io_context->stop();
		unsigned int ret;
		systhread_join(x->io_thread, &ret);
		x->io_thread = nullptr;
	}

	if (x->mutex)
	{
		systhread_mutex_free(x->mutex);
		x->mutex = nullptr;
	}

	if (x->output_data_clock)
		clock_free((t_object*)x->output_data_clock);

	if (x->output_info_clock)
		clock_free((t_object*)x->output_info_clock);
	
	delete x->socket;
	delete x->psn_decoder;
	delete x->io_context;
	delete x->remote_endpoint;
	delete x->buffer;
}

void pxspr_psn_client_assist(t_pxspr_psn_client* x, void* b, long m, long a, char* s)
{
	if (m == ASSIST_INLET)
	{
		sprintf(s, PXSPR_PSN_CLIENT_CLASSNAME);
	}
	else
	{
		if (a == 0)
			sprintf(s, "tracker data");
		else
			sprintf(s, "dump");
	}
}

void pxspr_psn_client_getversion(t_pxspr_psn_client* x)
{
	t_atom av[3];
	atom_setlong(av, PXSPR_PSN_VERSION_MAJOR);
	atom_setlong(av + 1, PXSPR_PSN_VERSION_MINOR);
	atom_setlong(av + 2, PXSPR_PSN_VERSION_BUGFIX);
	
	outlet_anything(x->outlet_dump, _sym_version, 3,  av);
}

void pxspr_psn_client_geturl(t_pxspr_psn_client* x)
{
	t_atom av;
	atom_setsym(&av, gensym(PXSPR_URL));
	outlet_anything(x->outlet_dump, _sym_url, 1,  &av);
}

void* pxspr_psn_client_iothreadproc(t_pxspr_psn_client* x)
{
	auto work_guard = asio::make_work_guard(*x->io_context);
	x->io_context->run();

	work_guard.reset();

	systhread_exit(0);
	return nullptr;
}

void pxspr_psn_client_createsocket(t_pxspr_psn_client* x)
{
	systhread_mutex_lock(x->mutex);
	
	if (x->socket != nullptr)
	{
		x->socket->cancel();
		delete x->socket;
		x->socket = nullptr;
	}

	delete x->psn_decoder;
	x->psn_decoder = nullptr;
	
	const auto endpoint = asio::ip::udp::endpoint(asio::ip::udp::v4(), psn::DEFAULT_UDP_PORT);
	x->socket = new asio::ip::udp::socket(*x->io_context, endpoint);

	const asio::ip::multicast::enable_loopback loopback_option(true);
	x->socket->set_option(loopback_option);
	
	const asio::ip::address multicast_address = asio::ip::address::from_string(psn::DEFAULT_UDP_MULTICAST_ADDR);
	const asio::ip::multicast::join_group option(multicast_address);
	x->socket->set_option(option);

	x->psn_decoder = new psn::psn_decoder;

	systhread_mutex_unlock(x->mutex);
	
	pxspr_psn_client_startreceive(x);
}

void pxspr_psn_client_startreceive(t_pxspr_psn_client* x)
{
	if (x->socket == nullptr)
		return;
	
	x->socket->async_receive_from(asio::buffer(*x->buffer), *x->remote_endpoint, 
		[=](asio::error_code const& ec, std::size_t bytes_received)
	{
		if (ec)
		{
			
		}
		else
		{
			systhread_mutex_lock(x->mutex);
			const bool is_decoded = x->psn_decoder->decode(x->buffer->data(), bytes_received);
			systhread_mutex_unlock(x->mutex);

			if (is_decoded)
			{
				const uint16_t packet_id = (*x->buffer)[0] + ((*x->buffer)[1] << 8);

				switch(packet_id)
				{
					case psn::DATA_PACKET:
						clock_unset(x->output_data_clock);
						clock_delay(x->output_data_clock, 0);
						break;

					case psn::INFO_PACKET:
						clock_unset(x->output_info_clock);
						clock_delay(x->output_info_clock, 0);
						break;


					default:
						break;
				}
			}
		}

		pxspr_psn_client_startreceive(x);
	});
}

void pxspr_psn_client_outputdata(t_pxspr_psn_client* x)
{
	systhread_mutex_lock(x->mutex);
	psn::psn_decoder::data_t data(x->psn_decoder->get_data());
	systhread_mutex_unlock(x->mutex);

	for(const auto& it : data.trackers)
	{
		const psn::tracker& tracker = it.second;

		if (tracker.is_pos_set())
		{
			t_atom argv[5];
			atom_setlong(argv, tracker.get_id());
			atom_setsym(argv + 1, _sym_position);
			atom_setfloat(argv + 2, tracker.get_pos().x);
			atom_setfloat(argv + 3, tracker.get_pos().y);
			atom_setfloat(argv + 4, tracker.get_pos().z);
			
			outlet_atoms(x->outlet_trackers,5, argv);
		}
		
		if (tracker.is_speed_set())
		{
			t_atom argv[5];
			atom_setlong(argv, tracker.get_id());
			atom_setsym(argv + 1, _sym_speed);
			atom_setfloat(argv + 2, tracker.get_speed().x);
			atom_setfloat(argv + 3, tracker.get_speed().y);
			atom_setfloat(argv + 4, tracker.get_speed().z);
			
			outlet_atoms(x->outlet_trackers,5, argv);
		}

		if (tracker.is_ori_set())
		{
			const t_atom_float ori_x = tracker.get_ori().x;
			const t_atom_float ori_y = tracker.get_ori().y;
			const t_atom_float ori_z = tracker.get_ori().z;
			const t_atom_float length = sqrt(pow(ori_x, 2) + pow(ori_y, 2) + pow(ori_z, 2));

			t_atom argv[6];
			atom_setlong(argv, tracker.get_id());
			atom_setsym(argv + 1, _sym_rotate);
			atom_setfloat(argv + 2, length * (180 / M_PI));
			atom_setfloat(argv + 3, ori_x / length);
			atom_setfloat(argv + 4, ori_y / length);
			atom_setfloat(argv + 5, ori_z / length);
			
			outlet_atoms(x->outlet_trackers,6, argv);
		}

		if (tracker.is_status_set())
		{
			t_atom argv[3];
			atom_setlong(argv, tracker.get_id());
			atom_setsym(argv + 1, _sym_status);
			atom_setfloat(argv + 2, tracker.get_status());
			
			outlet_atoms(x->outlet_trackers,3, argv);
		}

		if (tracker.is_accel_set())
		{
			t_atom argv[5];
			atom_setlong(argv, tracker.get_id());
			atom_setsym(argv + 1, _sym_acceleration);
			atom_setfloat(argv + 2, tracker.get_accel().x);
			atom_setfloat(argv + 3, tracker.get_accel().y);
			atom_setfloat(argv + 4, tracker.get_accel().z);
			
			outlet_atoms(x->outlet_trackers,5, argv);
		}

		if (tracker.is_target_pos_set())
		{
			t_atom argv[5];
			atom_setlong(argv, tracker.get_id());
			atom_setsym(argv + 1, _sym_targetposition);
			atom_setfloat(argv + 2, tracker.get_target_pos().x);
			atom_setfloat(argv + 3, tracker.get_target_pos().y);
			atom_setfloat(argv + 4, tracker.get_target_pos().z);
			
			outlet_atoms(x->outlet_trackers,5, argv);
		}

		if (tracker.is_timestamp_set())
		{
			t_atom argv[3];
			atom_setlong(argv, tracker.get_id());
			atom_setsym(argv + 1, _sym_timestamp);
			atom_setlong(argv + 2, tracker.get_timestamp());
			
			outlet_atoms(x->outlet_trackers,3, argv);
		}
	}
}

void pxspr_psn_client_outputinfo(t_pxspr_psn_client* x)
{
	systhread_mutex_lock(x->mutex);
	psn::psn_decoder::info_t info(x->psn_decoder->get_info());
	systhread_mutex_unlock(x->mutex);

	for(const auto& it : info.tracker_names)
	{
		t_atom argv[3];
		atom_setlong(argv, it.first);
		atom_setsym(argv + 1, _sym_name);
		atom_setsym(argv + 2, gensym(it.second.c_str()));

		outlet_atoms(x->outlet_trackers,3, argv);
	}
}