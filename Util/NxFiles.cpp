//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//	Copyright (C) 2015-2019  Daniel Allendorf, Ryan Payton						//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//																				//
//	This program is distributed in the hope that it will be useful,				//
//	but WITHOUT ANY WARRANTY; without even the implied warranty of				//
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the				//
//	GNU Affero General Public License for more details.							//
//																				//
//	You should have received a copy of the GNU Affero General Public License	//
//	along with this program.  If not, see <https://www.gnu.org/licenses/>.		//
//////////////////////////////////////////////////////////////////////////////////
#include "NxFiles.h"

#include "../Console.h"

#include <fstream>

// For telling "the file is not there" apart from "the file cannot be opened",
// which are the same thing to ifstream and very different things to fix.
#include <sys/stat.h>

#include <nlnx/nx.hpp>
#include <nlnx/node.hpp>

namespace ms
{
	Error NxFiles::init()
	{
		for (auto filename : NxFiles::filenames) {
		    std::string path = "HeavenClient/" + std::string(filename);
            if (!std::ifstream{path}.good()) {
                // Name it, and say which of the two things went wrong.
                //
                // "Missing nx file" was printed for a file that was present
                // and merely unreadable. adb writes these as the SHELL user
                // into a folder it creates with no world permissions at all -
                // drwxrws--- - so the game, running as somebody else, cannot
                // even traverse in. A complete 4.5 GB install reported every
                // file missing, and the search went looking for an absent
                // file that was sitting right there.
                //
                // A path that exists but will not open is a PERMISSION
                // problem, and saying so is the entire diagnosis.
                struct stat st;
                bool there = stat(path.c_str(), &st) == 0;

                printf("[!] %s: %s\n", filename,
                    there ? "present but CANNOT BE READ - try"
                            " chmod -R a+rX on the HeavenClient folder"
                          : "not found");

                return Error(Error::Code::MISSING_FILE, filename);
            }
        }

		try
		{
			printf("[*] Loading all nx files\n");
			nl::nx::load_all();
		}
		catch (const std::exception& ex)
		{
			static const std::string message = ex.what();

			return Error(Error::Code::NLNX, message.c_str());
		}

		constexpr const char* POSTCHAOS_BITMAP = "Login.img/WorldSelect/BtChannel/layer:bg";

		if (nl::nx::ui.resolve(POSTCHAOS_BITMAP).data_type() != nl::node::type::bitmap)
			return Error::Code::WRONG_UI_FILE;

		return Error::Code::NONE;
	}
}