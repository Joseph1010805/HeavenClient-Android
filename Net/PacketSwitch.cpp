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
#include "PacketSwitch.h"

#include "Handlers/CommonHandlers.h"
#include "Handlers/LoginHandlers.h"
#include "Handlers/SetfieldHandlers.h"
#include "Handlers/PlayerHandlers.h"
#include "Handlers/AttackHandlers.h"
#include "Handlers/MapObjectHandlers.h"
#include "Handlers/InventoryHandlers.h"
#include "Handlers/MessagingHandlers.h"
#include "Handlers/NpcInteractionHandlers.h"
#include "Handlers/SocialHandlers.h"
#include "Handlers/CashShopHandlers.h"
#include "Handlers/TestingHandlers.h"

#include "../Console.h"
#include "../Configuration.h"
#include "../Util/Silent.h"

namespace ms
{
	// Opcodes for InPackets.
	enum Opcode : uint16_t
	{
		// Login 1
		LOGIN_RESULT = 0,
		SERVERLIST = 10,
		CHARLIST = 11,
		SERVER_IP = 12,
		CHANGE_CHANNEL = 16,
		CHARNAME_RESPONSE = 13,
		ADD_NEWCHAR_ENTRY = 14,
		DELCHAR_RESPONSE = 15,
		PING = 17,

		// Player 1
		APPLY_BUFF = 20,

		// Login 2
		SELECT_WORLD = 26,
		RECOMMENDED_WORLDS = 27,
		CHECK_SPW_RESULT = 28,

		// Inventory 1
		MODIFY_INVENTORY = 29,

		// Player 2
		CHANGE_STATS = 31,
		GIVE_BUFF = 32,
		CANCEL_BUFF = 33,
		RECALCULATE_STATS = 35,
		UPDATE_SKILL = 36,

		// Messaging 1
		SHOW_STATUS_INFO = 39,
		MEMO_RESULT = 41,
		ENABLE_REPORT = 47,

		//Inventory 2
		GATHER_RESULT = 52,
		SORT_RESULT = 53,

		// Player 3
		SPAWN_SPECIAL_MAPOBJECT = 175,
		REMOVE_SPECIAL_MAPOBJECT = 176,
		MOVE_SUMMON = 177,
		SUMMON_ATTACK = 178,
		DAMAGE_SUMMON = 179,
		SUMMON_SKILL = 180,
		SPAWN_MIST = 273,
		REMOVE_MIST = 274,
		SPAWN_DOOR = 275,
		REMOVE_DOOR = 276,

		SET_ITC = 126,
		SET_CASH_SHOP = 127,
		QUERY_CASH_RESULT = 324,
		CS_OPERATION = 325,
		CS_CHECK_NAME_CHANGE = 328,
		CS_NAME_CHANGE_POSSIBLE = 329,
		CS_TRANSFER_WORLD = 331,
		CS_CASH_GACHAPON_RESULT = 333,

		UPDATE_GENDER = 58,
		PARTY_OPERATION = 62,
		BUDDY_LIST = 63,
		GUILD_OPERATION = 65,

		// Messaging 2
		SERVER_MESSAGE = 68,
		WEEK_EVENT_MESSAGE = 77,

		FIELD_SET_VARIABLE = 92,
		FAMILY_PRIV_LIST = 100,
		CANCEL_RENAME_BY_OTHER = 120,
		SCRIPT_PROGRESS_MESSAGE = 122,
		RECEIVE_POLICE = 123,
		SKILL_MACROS = 124,
		SET_FIELD = 125,
		FIELD_EFFECT = 138,
		FIELD_OBSTACLE_ONOFF_LIST = 140,
		ADMIN_RESULT = 144,
		CLOCK = 147,

		// Mapobject
		SPAWN_CHAR = 160,
		REMOVE_CHAR = 161,

		// Messaging
		CHAT_RECEIVED = 162,
		SCROLL_RESULT = 167,

		// Mapobject
		SPAWN_PET = 168,
		CHAR_MOVED = 185,

		// Attack
		ATTACKED_CLOSE = 186,
		ATTACKED_RANGED = 187,
		ATTACKED_MAGIC = 188,

		FACIAL_EXPRESSION = 193,
		SHOW_ITEM_EFFECT = 194,
		SHOW_CHAIR = 196,
		UPDATE_CHARLOOK = 197,
		SHOW_FOREIGN_EFFECT = 198,
		GIVE_FOREIGN_BUFF = 199,
		CANCEL_FOREIGN_BUFF = 200,
		SHOW_ITEM_GAIN_INCHAT = 206, // TODO: Rename this (Terribly named)
		UPDATE_QUEST_INFO = 211,
		LOCK_UI = 221,
		TOGGLE_UI = 222,

		// Player
		ADD_COOLDOWN = 234,

		// Mapobject
		SPAWN_MOB = 236,
		KILL_MOB = 237,
		SPAWN_MOB_C = 238,
		MOB_MOVED = 239,
		MOVE_MOB_RESPONSE = 240,
		SHOW_MOB_HP = 250,
		SPAWN_NPC = 257,
		SPAWN_NPC_C = 259,
		MAKE_NPC_SCRIPTED = 263,
		DROP_LOOT = 268,
		REMOVE_LOOT = 269,
		HIT_REACTOR = 277,
		SPAWN_REACTOR = 279,
		REMOVE_REACTOR = 280,

		// NPC Interaction
		NPC_DIALOGUE = 304,
		OPEN_NPC_SHOP = 305,
		CONFIRM_SHOP_TRANSACTION = 306,
		PLAYER_INTERACTION = 314,
		KEYMAP = 335,
		AUTO_HP_POT = 336,
		AUTO_MP_POT = 337
	};

	PacketSwitch::PacketSwitch()
	{
		// Common handlers
		emplace<PING, PingHandler>();

		// Login handlers
		emplace<LOGIN_RESULT, LoginResultHandler>();
		emplace<SERVERLIST, ServerlistHandler>();
		emplace<RECOMMENDED_WORLDS, RecommendedWorldsHandler>();
		emplace<CHARLIST, CharlistHandler>();
		emplace<CHARNAME_RESPONSE, CharnameResponseHandler>();
		emplace<ADD_NEWCHAR_ENTRY, AddNewCharEntryHandler>();
		emplace<DELCHAR_RESPONSE, DeleteCharResponseHandler>();
		emplace<SERVER_IP, ServerIPHandler>();
		emplace<CHANGE_CHANNEL, ChangeChannelHandler>();

		// 'Setfield' handlers
		emplace<SET_FIELD, SetfieldHandler>();

		// MapObject handlers
		emplace<SPAWN_CHAR, SpawnCharHandler>();
		emplace<CHAR_MOVED, CharMovedHandler>();
		emplace<UPDATE_CHARLOOK, UpdateCharLookHandler>();
		emplace<SHOW_FOREIGN_EFFECT, ShowForeignEffectHandler>();
		emplace<REMOVE_CHAR, RemoveCharHandler>();
		emplace<SPAWN_PET, SpawnPetHandler>();
		emplace<SPAWN_NPC, SpawnNpcHandler>();
		emplace<SPAWN_NPC_C, SpawnNpcControllerHandler>();
		emplace<SPAWN_MOB, SpawnMobHandler>();
		emplace<SPAWN_MOB_C, SpawnMobControllerHandler>();
		emplace<MOB_MOVED, MobMovedHandler>();
		emplace<SHOW_MOB_HP, ShowMobHpHandler>();
		emplace<KILL_MOB, KillMobHandler>();
		emplace<DROP_LOOT, DropLootHandler>();
		emplace<REMOVE_LOOT, RemoveLootHandler>();
		emplace<HIT_REACTOR, HitReactorHandler>();
		emplace<SPAWN_REACTOR, SpawnReactorHandler>();
		emplace<REMOVE_REACTOR, RemoveReactorHandler>();

		// Doors, mists and summons
		emplace<SPAWN_DOOR, SpawnDoorHandler>();
		emplace<REMOVE_DOOR, RemoveDoorHandler>();
		emplace<SPAWN_MIST, SpawnMistHandler>();
		emplace<REMOVE_MIST, RemoveMistHandler>();
		emplace<SPAWN_SPECIAL_MAPOBJECT, SpawnSummonHandler>();
		emplace<REMOVE_SPECIAL_MAPOBJECT, RemoveSummonHandler>();
		emplace<MOVE_SUMMON, MoveSummonHandler>();
		emplace<SUMMON_ATTACK, SummonAttackHandler>();
		emplace<DAMAGE_SUMMON, DamageSummonHandler>();
		emplace<SUMMON_SKILL, SummonSkillHandler>();

		// Attack handlers
		emplace<ATTACKED_CLOSE, CloseAttackHandler>();
		emplace<ATTACKED_RANGED, RangedAttackHandler>();
		emplace<ATTACKED_MAGIC, MagicAttackHandler>();

		// Player handlers
		emplace<KEYMAP, KeymapHandler>();
		emplace<SKILL_MACROS, SkillMacrosHandler>();
		emplace<CHANGE_STATS, ChangeStatsHandler>();
		emplace<GIVE_BUFF, ApplyBuffHandler>();
		emplace<CANCEL_BUFF, CancelBuffHandler>();
		emplace<RECALCULATE_STATS, RecalculateStatsHandler>();
		emplace<UPDATE_SKILL, UpdateSkillHandler>();
		emplace<ADD_COOLDOWN, AddCooldownHandler>();

		// Cash shop
		emplace<SET_CASH_SHOP, SetCashShopHandler>();
		emplace<SET_ITC, SetITCHandler>();
		emplace<CS_OPERATION, CashShopOperationHandler>();
		emplace<QUERY_CASH_RESULT, QueryCashResultHandler>();
		emplace<CS_CHECK_NAME_CHANGE, CashShopNameChangeHandler>();
		emplace<CS_NAME_CHANGE_POSSIBLE, CashShopNameChangePossibleHandler>();
		emplace<CS_TRANSFER_WORLD, CashShopTransferWorldHandler>();
		emplace<CS_CASH_GACHAPON_RESULT, CashGachaponResultHandler>();

		// Social handlers
		emplace<PARTY_OPERATION, PartyOperationHandler>();

		// Messaging handlers
		emplace<SHOW_STATUS_INFO, ShowStatusInfoHandler>();
		emplace<CHAT_RECEIVED, ChatReceivedHandler>();
		emplace<SCROLL_RESULT, ScrollResultHandler>();
		emplace<SERVER_MESSAGE, ServerMessageHandler>();
		emplace<WEEK_EVENT_MESSAGE, WeekEventMessageHandler>();
		emplace<SHOW_ITEM_GAIN_INCHAT, ShowItemGainInChatHandler>();

		// Inventory Handlers
		emplace<MODIFY_INVENTORY, ModifyInventoryHandler>();
		emplace<GATHER_RESULT, GatherResultHandler>();
		emplace<SORT_RESULT, SortResultHandler>();

		// Npc Interaction Handlers
		emplace<NPC_DIALOGUE, NpcDialogueHandler>();
		emplace<OPEN_NPC_SHOP, OpenNpcShopHandler>();

		// TODO: Handle packets below correctly
		emplace<MOVE_MOB_RESPONSE, NullHandler>();
		emplace<MEMO_RESULT, NullHandler>();
		emplace<ENABLE_REPORT, NullHandler>();
		emplace<BUDDY_LIST, NullHandler>();
		emplace<GUILD_OPERATION, NullHandler>();
		emplace<FAMILY_PRIV_LIST, NullHandler>();
		emplace<SCRIPT_PROGRESS_MESSAGE, NullHandler>();
		emplace<RECEIVE_POLICE, NullHandler>();
		emplace<MAKE_NPC_SCRIPTED, NullHandler>();

		// Ignored
		emplace<SELECT_WORLD, NullHandler>();
		emplace<UPDATE_GENDER, NullHandler>();

		// New handlers for testing only
		// Once these are handled properly, they need moved to a proper file
		emplace<CHECK_SPW_RESULT, CheckSpwResultHandler>();
		emplace<FIELD_EFFECT, FieldEffectHandler>();
		emplace<FIELD_OBSTACLE_ONOFF_LIST, FieldObstacleOnOffListHandler>();
		emplace<ADMIN_RESULT, AdminResultHandler>();
		emplace<FACIAL_EXPRESSION, FacialExpressionHandler>();
		emplace<GIVE_FOREIGN_BUFF, GiveForeignBuffHandler>();
		emplace<CANCEL_FOREIGN_BUFF, CancelForeignBuffHandler>();
		emplace<UPDATE_QUEST_INFO, UpdateQuestInfoHandler>();
		emplace<LOCK_UI, LockUiHandler>();
		emplace<TOGGLE_UI, ToggleUiHandler>();
		emplace<CONFIRM_SHOP_TRANSACTION, ConfirmShopTransactionHandler>();
		emplace<PLAYER_INTERACTION, PlayerInteractionHandler>();
		emplace<AUTO_HP_POT, AutoHpPotHandler>();
		emplace<AUTO_MP_POT, AutoMpPotHandler>();
	}

	void PacketSwitch::forward(const int8_t* bytes, size_t length) const
	{
		// Wrap the bytes with a parser.
		InPacket recv = { bytes, length };
		// Read the opcode to determine handler responsible.
		uint16_t opcode = recv.read_short();

		if (Configuration::get().get_show_packets())
			std::cout << "Received Packet: " << std::to_string(opcode) << std::endl;

		if (opcode < NUM_HANDLERS)
		{
			if (auto& handler = handlers[opcode])
			{
				// Handler ok. Packet is passed on.
				try
				{
					handler->handle(recv);

					// A handler that stopped early is a silent bug: it read
					// a layout that is not the one the server wrote, and
					// whatever it did not read is whatever it got wrong. The
					// quest-log parser that broke character loading looked
					// exactly like this from the outside - nothing.
					//
					// Some handlers stop on purpose (the cash shop catalogue
					// is deliberately not read), so this is a place to look,
					// not a fault on its own.
					if (size_t left = recv.length())
						Silent::report("PacketSwitch",
							"opcode " + std::to_string(opcode) + " left "
							+ std::to_string(left) + " bytes unread");
				}
				catch (const PacketError& err)
				{
					// Notice about an error.
					warn(err.what(), opcode);
				}
			}
			else
			{
				// Warn about an unhandled packet.
				warn(MSG_UNHANDLED, opcode);
			}
		}
		else
		{
			// Warn about a packet with opcode out of bounds.
			warn(MSG_OUTOFBOUNDS, opcode);
		}
	}

	void PacketSwitch::warn(const std::string& message, size_t opcode) const
	{
		// This used to go only to `Console`, which is compiled to nothing
		// unless PRINT_WARNINGS is set and writes to stdout even then - and
		// stdout on Android goes nowhere. So the client has been telling
		// itself about every packet it could not handle, into a void, for the
		// whole life of this port.
		Silent::report("PacketSwitch", message + ", opcode " + std::to_string(opcode));

		Console::get().print(message + ", Opcode: " + std::to_string(opcode));
	}
}