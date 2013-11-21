/*
 * \brief  Basic Genode types
 * \author Martin Stein
 * \date   2012-01-02
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BASE__NATIVE_TYPES_H_
#define _BASE__NATIVE_TYPES_H_

/* Genode includes */
#include <kernel/interface.h>
#include <base/native_capability.h>
#include <base/stdint.h>
#include <util/string.h>

/* base-hw includes */
#include <kernel/log.h>

namespace Genode
{
	class Platform_thread;
	class Tlb;

	typedef unsigned Native_thread_id;

	struct Native_thread
	{
		Platform_thread  * platform_thread;
		Native_thread_id   thread_id;
	};

	typedef int Native_connection_state;

	/* FIXME needs to be MMU dependent */
	enum { MIN_MAPPING_SIZE_LOG2 = 12 };

	/**
	 * Return kernel thread-name of the caller
	 */
	Native_thread_id thread_get_my_native_id();

	/**
	 * Return an invalid kernel thread-name
	 */
	inline Native_thread_id thread_invalid_id() { return 0; }

	/**
	 * Data bunch with variable size that is communicated between threads
	 *
	 * \param MAX_SIZE  maximum size the object is allowed to take
	 */
	template <size_t MAX_SIZE>
	struct Message;

	/**
	 * Message that is communicated from a thread creator to the new thread
	 */
	class Startup_msg;

	/**
	 * Memory region that is exclusive to every thread and known by the kernel
	 */
	struct Native_utcb;

	struct Cap_dst_policy
	{
		typedef Native_thread_id Dst;

		/**
		 * Validate capability destination
		 */
		static bool valid(Dst pt) { return pt != 0; }

		/**
		 * Get invalid capability destination
		 */
		static Dst invalid() { return 0; }

		/**
		 * Copy capability 'src' to a given memory destination 'dst'
		 */
		static void
		copy(void * dst, Native_capability_tpl<Cap_dst_policy> * src);
	};

	typedef Native_capability_tpl<Cap_dst_policy> Native_capability;

	/**
	 * Coherent address region
	 */
	struct Native_region
	{
		addr_t base;
		size_t size;
	};

	struct Native_config
	{
		/**
		 * Thread-context area configuration.
		 */
		static addr_t context_area_virtual_base() { return 0x40000000UL; }
		static addr_t context_area_virtual_size() { return 0x10000000UL; }

		/**
		 * Size of virtual address region holding the context of one thread
		 */
		static addr_t context_virtual_size() { return 0x00100000UL; }
	};

	struct Native_pd_args { };
}

template <Genode::size_t MAX_SIZE>
class Genode::Message
{
	private:

		size_t  _data_size;
		uint8_t _data[];

		/**
		 * Return size of payload-preceding meta data
		 */
		size_t _header_size() const { return (addr_t)_data - (addr_t)this; }

		/**
		 * Return maximum payload size
		 */
		size_t _max_data_size() const { return MAX_SIZE - _header_size(); }

		/**
		 * Return size of header and current payload
		 */
		size_t _size() const { return _header_size() + _data_size; }

	public:

		/**
		 * Get information about current await-request operation
		 *
		 * \return buf_base  base of receive buffer
		 * \return buf_size  size of receive buffer
		 */
		void info_about_await_request(void * & buf_base, size_t & buf_size)
		{
			buf_base = this;
			buf_size = MAX_SIZE;
		}

		/**
		 * Get information about current send-request operation
		 *
		 * \return msg_base  base of complete send-message data
		 * \return msg_size  size of complete send-message data
		 * \return buf_base  base of receive buffer
		 * \return buf_size  size of receive buffer
		 */
		void info_about_send_request(void * & msg_base, size_t & msg_size,
		                             void * & buf_base, size_t & buf_size)
		{
			msg_base = this;
			msg_size = _size();
			buf_base = this;
			buf_size = MAX_SIZE;
		}

		/**
		 * Get information about current send-reply operation
		 *
		 * \return msg_base  base of complete send-message data
		 * \return msg_size  size of complete send-message data
		 */
		void info_about_send_reply(void * & msg_base, size_t & msg_size)
		{
			msg_base = this;
			msg_size = _size();
		}

		/**
		 * Install message that shall be send
		 *
		 * \param data           base of payload
		 * \param raw_data_size  size of payload without preceding name
		 * \param name           local name that shall precede raw payload
		 */
		void prepare_send(void * const data, size_t raw_data_size,
		                  unsigned const name)
		{
			/* limit data size */
			enum { NAME_SIZE = sizeof(name) };
			size_t const data_size = raw_data_size + NAME_SIZE;
			if (data_size > _max_data_size()) {
				kernel_log() << __func__ << ": oversized message outgoing\n";
				raw_data_size = _max_data_size() - NAME_SIZE;
			}
			/* copy data */
			*(unsigned *)_data = name;
			void * const raw_data_dst = (void *)((addr_t)_data + NAME_SIZE);
			void * const raw_data_src  = (void *)((addr_t)data + NAME_SIZE);
			memcpy(raw_data_dst, raw_data_src, raw_data_size);
			_data_size = data_size;
		}

		/**
		 * Read out message that was received
		 *
		 * \param buf_base  base of read buffer
		 * \param buf_size  size of read buffer
		 */
		void finish_receive(void * const buf_base, size_t const buf_size)
		{
			/* limit data size */
			if (_data_size > buf_size) {
				kernel_log() << __func__ << ": oversized message incoming\n";
				_data_size = buf_size;
			}
			/* copy data */
			memcpy(buf_base, _data, _data_size);
		}
};

class Genode::Startup_msg
{
	private:

		Native_thread_id _thread_id;

	public:

		/**
		 * Set-up valid startup message
		 *
		 * \param thread_id  kernel name of the thread that is started
		 */
		void init(Native_thread_id const thread_id) { _thread_id = thread_id; }


		/***************
		 ** Accessors **
		 ***************/

		Native_thread_id thread_id() const { return _thread_id; }
};

struct Genode::Native_utcb
{
	enum { SIZE = 1 << MIN_MAPPING_SIZE_LOG2 };

	union {
		uint8_t       data[SIZE];
		Message<SIZE> message;
		Startup_msg   startup_msg;
	};

	size_t size() const { return SIZE; }

	void * base() const { return (void *)data; }
};

#endif /* _BASE__NATIVE_TYPES_H_ */

