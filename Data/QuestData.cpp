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
#include "QuestData.h"

#include <nlnx/nx.hpp>
#include <nlnx/node.hpp>

namespace ms
{
	namespace
	{
		// Every "id and count" list in Quest.nx has the same shape: numbered
		// children, each holding an `id` and a `count`.
		void read_pairs(nl::node list, std::map<int32_t, int16_t>& into)
		{
			for (nl::node entry : list)
			{
				int32_t id = entry["id"];

				if (!id)
					continue;

				into[id] = static_cast<int16_t>(static_cast<int32_t>(entry["count"]));
			}
		}

		void read_requirements(nl::node src, QuestData::Requirements& into)
		{
			if (!src)
				return;

			into.npc = src["npc"];
			into.lvmin = static_cast<int16_t>(static_cast<int32_t>(src["lvmin"]));
			into.lvmax = static_cast<int16_t>(static_cast<int32_t>(src["lvmax"]));

			for (nl::node job : src["job"])
				into.jobs.push_back(static_cast<int16_t>(static_cast<int32_t>(job)));

			for (nl::node quest : src["quest"])
			{
				int16_t id = static_cast<int16_t>(static_cast<int32_t>(quest["id"]));

				if (id)
					into.quests[id] = static_cast<int8_t>(static_cast<int32_t>(quest["state"]));
			}

			read_pairs(src["item"], into.items);
			read_pairs(src["mob"], into.mobs);

			// `startscript` on the start phase and `endscript` on the end one.
			// Which is present is what decides whether QUEST_ACTION carries a
			// plain action or a scripted one.
			into.scripted = static_cast<bool>(src["startscript"])
				|| static_cast<bool>(src["endscript"]);
		}

		// Item names for the journal's `#t<id>#`. String.nx keeps them in a
		// different file per kind, and the leading digit says which.
		std::string name_of_item(int32_t itemid)
		{
			std::string id = std::to_string(itemid);

			switch (itemid / 1000000)
			{
			case 1:
			{
				// Equips are filed under their category, which is not in the
				// id - so the whole tree is searched once and remembered.
				static std::map<int32_t, std::string> equips;

				if (equips.empty())
					for (nl::node category : nl::nx::string["Eqp.img"]["Eqp"])
						for (nl::node item : category)
							equips[std::stoi(item.name())] = std::string(item["name"]);

				auto iter = equips.find(itemid);

				return (iter == equips.end()) ? id : iter->second;
			}
			case 2: return std::string(nl::nx::string["Consume.img"][id]["name"]);
			case 3: return std::string(nl::nx::string["Ins.img"][id]["name"]);
			case 4: return std::string(nl::nx::string["Etc.img"]["Etc"][id]["name"]);
			case 5: return std::string(nl::nx::string["Cash.img"][id]["name"]);
			default: return id;
			}
		}

		void read_rewards(nl::node src, QuestData::Rewards& into)
		{
			if (!src)
				return;

			into.exp = src["exp"];
			into.money = src["money"];
			into.fame = static_cast<int16_t>(static_cast<int32_t>(src["pop"]));
			into.nextquest = static_cast<int16_t>(static_cast<int32_t>(src["nextQuest"]));

			read_pairs(src["item"], into.items);
		}
	}

	QuestData::QuestData(int16_t id) : questid(id)
	{
		std::string strid = std::to_string(questid);

		nl::node info = nl::nx::quest["QuestInfo.img"][strid];

		valid = static_cast<bool>(info);

		if (!valid)
			return;

		name = std::string(info["name"]);
		area = info["area"];

		for (size_t i = 0; i < 3; i++)
			text[i] = std::string(info[std::to_string(i)]);

		nl::node check = nl::nx::quest["Check.img"][strid];
		read_requirements(check["0"], start);
		read_requirements(check["1"], finish);

		nl::node act = nl::nx::quest["Act.img"][strid];
		read_rewards(act["0"], startgives);
		read_rewards(act["1"], finishgives);
	}

	const QuestData& QuestData::get(int16_t questid)
	{
		return Cache<QuestData>::get(questid);
	}

	bool QuestData::is_valid() const { return valid; }
	int16_t QuestData::get_id() const { return questid; }
	const std::string& QuestData::get_name() const { return name; }
	int32_t QuestData::get_area() const { return area; }

	const std::string& QuestData::get_text(size_t which) const
	{
		static const std::string nothing;

		return (which < 3) ? text[which] : nothing;
	}

	const QuestData::Requirements& QuestData::to_start() const { return start; }
	const QuestData::Requirements& QuestData::to_finish() const { return finish; }
	const QuestData::Rewards& QuestData::start_rewards() const { return startgives; }
	const QuestData::Rewards& QuestData::finish_rewards() const { return finishgives; }

	std::vector<int16_t> QuestData::candidates(int16_t level)
	{
		std::vector<int16_t> out;

		for (nl::node quest : nl::nx::quest["Check.img"])
		{
			nl::node start = quest["0"];

			int32_t lvmin = start["lvmin"];
			int32_t lvmax = start["lvmax"];

			// A little above the character's level as well as below: seeing
			// what is nearly in reach is most of the point of the list.
			if (lvmin && lvmin > level + 10)
				continue;

			if (lvmax && lvmax < level)
				continue;

			out.push_back(static_cast<int16_t>(std::stoi(quest.name())));
		}

		return out;
	}

	std::string QuestData::strip_markup(const std::string& text)
	{
		std::string out;

		for (size_t i = 0; i < text.size(); )
		{
			if (text[i] != '#' || i + 1 >= text.size())
			{
				// A literal carriage return draws as a stray glyph.
				if (text[i] != '\r')
					out += text[i];

				i++;
				continue;
			}

			char code = text[i + 1];

			switch (code)
			{
			// Colour and style switches carry nothing and simply go.
			case 'b': case 'k': case 'r': case 'g': case 'd':
			case 'e': case 'n': case 'f': case 'B': case 'h':
				i += 2;
				break;
			// Codes that name something, ending at the next '#'.
			case 't': case 'i': case 'm': case 'p': case 'o': case 'c':
			{
				size_t close = text.find('#', i + 2);

				if (close == std::string::npos)
				{
					i += 2;
					break;
				}

				std::string idstr = text.substr(i + 2, close - i - 2);
				i = close + 1;

				int32_t id = 0;

				try { id = std::stoi(idstr); }
				catch (...) { break; }

				if (code == 'm')
					out += std::string(nl::nx::string["Map.img"]["name"]);
				else if (code == 'o')
					out += std::string(nl::nx::string["Mob.img"][std::to_string(id)]["name"]);
				else
					out += name_of_item(id);

				break;
			}
			default:
				// An unknown code costs its '#' rather than a word.
				i++;
				break;
			}
		}

		return out;
	}

	const std::vector<int16_t>& QuestData::quests_of_npc(int32_t npcid, bool finishing)
	{
		// Built once, on first use, by walking Check.img - about 2,800
		// quests. The alternative is walking them again every time an NPC
		// comes into view, which is every map change.
		static std::map<int32_t, std::vector<int16_t>> starters;
		static std::map<int32_t, std::vector<int16_t>> finishers;
		static bool built = false;

		if (!built)
		{
			built = true;

			for (nl::node quest : nl::nx::quest["Check.img"])
			{
				int16_t id = static_cast<int16_t>(std::stoi(quest.name()));

				if (int32_t npc = quest["0"]["npc"])
					starters[npc].push_back(id);

				if (int32_t npc = quest["1"]["npc"])
					finishers[npc].push_back(id);
			}
		}

		static const std::vector<int16_t> nothing;

		const auto& from = finishing ? finishers : starters;
		auto iter = from.find(npcid);

		return (iter == from.end()) ? nothing : iter->second;
	}
}
