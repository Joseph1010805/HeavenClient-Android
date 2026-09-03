//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <string>

namespace ms
{
	// ENOUGH HTTP TO TALK TO A POST BOX ON THE NEXT SOFA.
	//
	// The carry port and the message box both answer plain text over plain
	// TCP on the local network, and nothing else in this client speaks HTTP -
	// so a whole library would be a dependency for two verbs. This is written
	// out instead: no chunked encoding, no keep-alive, no redirects, because
	// the only servers it talks to never use them.
	//
	// ⚠ PLAIN HTTP ONLY - THERE IS NO TLS IN THIS BUILD.
	//
	// mbedtls is compiled in for the Switch and REMOVED for Android
	// (CMakeLists.txt), which has a normal POSIX stack and uses asio. So this
	// cannot fetch an https:// address at all, and must not pretend to.
	//
	// That is fine for everything on the LAN, which is the case that has to
	// work with the power out. Anything across the internet is HTTPS and goes
	// through the Java layer instead, where Android's own stack does the
	// certificate checking properly - see Relay.
	class Http
	{
	public:
		struct Reply
		{
			// From the STATUS LINE, never from "is there a body". A 404 has a
			// body too, and treating one as success is how an error message
			// gets stored as if it were data.
			bool ok = false;

			std::string body;

			// Why not, in words a screen can show.
			std::string trouble;
		};

		// `url` is http://host:port/path. Anything else is refused rather
		// than attempted, so an https:// address fails with a reason instead
		// of a confusing connection error.
		static Reply get(const std::string& url);
		static Reply post(const std::string& url, const std::string& body);

		// Percent-encode one query value. Names can contain spaces.
		static std::string escape(const std::string& value);

		// THE SAME TWO VERBS, FOR AN ADDRESS OUT ON THE INTERNET.
		//
		// Handed to Android's own HTTP stack through JNI, because that one
		// has TLS and a maintained certificate store and this build has
		// neither. Everything about the caller is unchanged - it is the same
		// Reply, and the same rule that only a 200 counts.
		//
		// On a platform with no Java layer these answer "no relay here",
		// which is a perfectly ordinary state: the relay is never required.
		static Reply secure_get(const std::string& url);
		static Reply secure_post(const std::string& url, const std::string& body);

	private:
		static Reply request(const std::string& url, const char* method,
			const std::string& body);
	};
}
