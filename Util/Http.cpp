//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#include "Http.h"

#include "../MapleStory.h"

#include <chrono>
#include <sstream>

#ifdef PLATFORM_ANDROID
#include <jni.h>
#include <SDL.h>
#endif

#define BOOST_DATE_TIME_NO_LIB
#define BOOST_REGEX_NO_LIB
#include "asio.hpp"

namespace ms
{
	namespace
	{
		// Long enough for a handheld to answer over wifi, short enough that a
		// device which has walked out of range does not hold anything up. A
		// peer that is gone does not refuse the connection - it simply never
		// answers - so without a deadline this waits as long as the operating
		// system feels like waiting.
		constexpr int TIMEOUT_MS = 4000;

		bool split_url(const std::string& url, std::string& host,
			std::string& port, std::string& path)
		{
			constexpr const char* SCHEME = "http://";
			constexpr size_t SCHEME_LEN = 7;

			if (url.compare(0, SCHEME_LEN, SCHEME) != 0)
				return false;

			size_t start = SCHEME_LEN;
			size_t slash = url.find('/', start);

			std::string authority = (slash == std::string::npos)
				? url.substr(start)
				: url.substr(start, slash - start);

			path = (slash == std::string::npos) ? "/" : url.substr(slash);

			size_t colon = authority.find(':');

			if (colon == std::string::npos)
			{
				host = authority;
				port = "80";
			}
			else
			{
				host = authority.substr(0, colon);
				port = authority.substr(colon + 1);
			}

			return !host.empty();
		}
	}

	std::string Http::escape(const std::string& value)
	{
		static const char* HEX = "0123456789ABCDEF";

		std::string out;

		for (unsigned char c : value)
		{
			if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
			{
				out.push_back(static_cast<char>(c));
			}
			else
			{
				out.push_back('%');
				out.push_back(HEX[c >> 4]);
				out.push_back(HEX[c & 0x0F]);
			}
		}

		return out;
	}

#ifdef PLATFORM_ANDROID
	namespace
	{
		// One call into Relay.java. SDL owns the JVM, so the environment comes
		// from it rather than being cached - a cached one belongs to whichever
		// thread first asked, and this runs on a worker.
		Http::Reply call_relay(const std::string& url, const std::string* body)
		{
			Http::Reply reply;

			JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());

			if (!env)
			{
				reply.trouble = "no Java environment for the relay";

				return reply;
			}

			jclass cls = env->FindClass("org/heavenclient/android/Relay");

			if (!cls)
			{
				env->ExceptionClear();
				reply.trouble = "no relay on this build";

				return reply;
			}

			jmethodID id = env->GetStaticMethodID(cls, "exchange",
				"(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;");

			if (!id)
			{
				env->ExceptionClear();
				env->DeleteLocalRef(cls);
				reply.trouble = "relay has no exchange method";

				return reply;
			}

			jstring jurl = env->NewStringUTF(url.c_str());
			jstring jbody = body ? env->NewStringUTF(body->c_str()) : nullptr;

			jobject answer = env->CallStaticObjectMethod(cls, id, jurl, jbody);

			// Anything thrown on the Java side has to be cleared, or the next
			// JNI call on this thread fails for no visible reason.
			if (env->ExceptionCheck())
			{
				env->ExceptionClear();
				answer = nullptr;
			}

			if (answer)
			{
				// NULL MEANS FAILED, and an empty string means "nothing is
				// waiting". Treating the two alike would make a quiet mailbox
				// look like a broken connection.
				const char* chars = env->GetStringUTFChars(
					static_cast<jstring>(answer), nullptr);

				if (chars)
				{
					reply.body = chars;
					reply.ok = true;

					env->ReleaseStringUTFChars(
						static_cast<jstring>(answer), chars);
				}

				env->DeleteLocalRef(answer);
			}
			else
			{
				reply.trouble = "the relay could not be reached";
			}

			if (jurl)
				env->DeleteLocalRef(jurl);

			if (jbody)
				env->DeleteLocalRef(jbody);

			env->DeleteLocalRef(cls);

			return reply;
		}
	}

	Http::Reply Http::secure_get(const std::string& url)
	{
		return call_relay(url, nullptr);
	}

	Http::Reply Http::secure_post(const std::string& url, const std::string& body)
	{
		return call_relay(url, &body);
	}
#else
	Http::Reply Http::secure_get(const std::string&)
	{
		Reply reply;
		reply.trouble = "no relay on this build";

		return reply;
	}

	Http::Reply Http::secure_post(const std::string&, const std::string&)
	{
		Reply reply;
		reply.trouble = "no relay on this build";

		return reply;
	}
#endif

	Http::Reply Http::get(const std::string& url)
	{
		return request(url, "GET", "");
	}

	Http::Reply Http::post(const std::string& url, const std::string& body)
	{
		return request(url, "POST", body);
	}

	Http::Reply Http::request(const std::string& url, const char* method,
		const std::string& body)
	{
		Reply reply;

		std::string host;
		std::string port;
		std::string path;

		if (!split_url(url, host, port, path))
		{
			// SAID PLAINLY. An https:// address here is not a network fault
			// and must not read like one - this build has no TLS at all, and
			// somebody reading the log needs to know that rather than go
			// looking at their wifi.
			reply.trouble = url.compare(0, 8, "https://") == 0
				? "this build cannot speak https - see Http.h"
				: "not an address I understand: " + url;

			return reply;
		}

		try
		{
			asio::io_context io;
			asio::ip::tcp::socket socket(io);
			asio::ip::tcp::resolver resolver(io);

			auto endpoints = resolver.resolve(host, port);

			std::error_code result = asio::error::would_block;

			asio::async_connect(socket, endpoints,
				[&](const std::error_code& e, const asio::ip::tcp::endpoint&)
				{
					result = e;
				});

			io.run_for(std::chrono::milliseconds(TIMEOUT_MS));

			if (result || !socket.is_open())
			{
				reply.trouble = "could not reach " + host;

				return reply;
			}

			std::ostringstream out;

			out << method << ' ' << path << " HTTP/1.1\r\n"
				<< "Host: " << host << "\r\n"
				<< "Connection: close\r\n"
				<< "Content-Type: text/plain; charset=utf-8\r\n"
				<< "Content-Length: " << body.size() << "\r\n\r\n"
				<< body;

			std::string wire = out.str();

			asio::write(socket, asio::buffer(wire), result);

			if (result)
			{
				reply.trouble = "could not send to " + host;

				return reply;
			}

			asio::streambuf incoming;
			std::error_code read_result;

			asio::read(socket, incoming, read_result);

			if (read_result && read_result != asio::error::eof)
			{
				reply.trouble = "lost the connection to " + host;

				return reply;
			}

			std::string answer(
				asio::buffers_begin(incoming.data()),
				asio::buffers_end(incoming.data()));

			size_t split = answer.find("\r\n\r\n");

			if (split == std::string::npos)
			{
				reply.trouble = host + " answered with something unreadable";

				return reply;
			}

			std::string head = answer.substr(0, split);

			reply.body = answer.substr(split + 4);
			reply.ok = head.compare(0, 12, "HTTP/1.1 200") == 0;

			if (!reply.ok)
			{
				size_t line = head.find("\r\n");

				reply.trouble =
					head.substr(0, line == std::string::npos ? head.size() : line)
					+ " - " + reply.body.substr(0, 120);
			}

			return reply;
		}
		catch (const std::exception& e)
		{
			reply.trouble = std::string("failed: ") + e.what();

			return reply;
		}
	}
}
