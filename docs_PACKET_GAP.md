# What the server says that the client does not hear

Generated 2026-08-22 by diffing Cosmic's `SendOpcode.java`
against the handlers registered in `Net/PacketSwitch.cpp`. Regenerate it with
`tools/packet_gap.py` after adding handlers.

**How to work through this.** For any row, the exact bytes are written in the
server's own `tools/PacketCreator.java` - find the method that builds that opcode
and read the layout straight off it. That is the specification; nothing online is
more reliable than the server this client actually talks to.

The client also prints `Unhandled packet detected, Opcode: N` at runtime, so
playing normally adds to this list by itself. Anything seen there is worth more
than anything guessed at here.

- Messages the server can send: **307**
- Handled by the client: **74**
- Not handled: **233**

## Worth doing, roughly in order of how often it would show

| done | opcode | name | what is missing |
|------|--------|------|-----------------|
| [ ] | 30 | `INVENTORY_GROW` | bag expansion |
| [ ] | 34 | `FORCED_STAT_SET` | stat overrides |
| [ ] | 37 | `SKILL_USE_RESULT` | skill failures |
| [ ] | 42 | `MAP_TRANSFER_RESULT` | teleport rocks |
| [ ] | 49 | `QUEST_CLEAR` | quest completion |
| [ ] | 61 | `CHAR_INFO` | inspecting another character |
| [ ] | 67 | `SPAWN_PORTAL` | special portals |
| [ ] | 86 | `MINIMAP_ON_OFF` | minimap toggling |
| [ ] | 105 | `NOTIFY_LEVELUP` | someone else levelling shows nothing |
| [ ] | 107 | `NOTIFY_JOB_CHANGE` | job change shows nothing |
| [ ] | 131 | `BLOCKED_MAP` | map entry refusal |
| [ ] | 134 | `MULTICHAT` | party/guild chat |
| [ ] | 135 | `WHISPER` | whispers |
| [ ] | 143 | `PLAY_JUKEBOX` | map music |
| [ ] | 147 | `CLOCK` | timed events |
| [ ] | 150 | `SET_QUEST_CLEAR` | quest completion |
| [ ] | 159 | `QUICKSLOT_INIT` | the quickslot bar the server remembers |
| [ ] | 163 | `CHATTEXT1` | chat variant |
| [ ] | 165 | `UPDATE_CHAR_BOX` | the box over a players head |
| [ ] | 166 | `SHOW_CONSUME_EFFECT` | consumable effects invisible |
| [ ] | 170 | `MOVE_PET` | pets |
| [ ] | 171 | `PET_CHAT` | pets |
| [ ] | 174 | `PET_COMMAND` | pets |
| [ ] | 175 | `SPAWN_SPECIAL_MAPOBJECT` | summons appearing |
| [ ] | 176 | `REMOVE_SPECIAL_MAPOBJECT` | summons leaving |
| [ ] | 177 | `MOVE_SUMMON` | summons |
| [ ] | 178 | `SUMMON_ATTACK` | summons |
| [ ] | 179 | `DAMAGE_SUMMON` | summons |
| [ ] | 180 | `SUMMON_SKILL` | summons |
| [ ] | 190 | `SKILL_EFFECT` | other players skills invisible |
| [ ] | 191 | `CANCEL_SKILL_EFFECT` | other players skills invisible |
| [ ] | 192 | `DAMAGE_PLAYER` | other players taking damage shows nothing |
| [ ] | 194 | `SHOW_ITEM_EFFECT` | item use shows nothing on other players |
| [ ] | 196 | `SHOW_CHAIR` | chairs - and chair healing |
| [ ] | 205 | `CANCEL_CHAIR` | chairs |
| [ ] | 214 | `PLAYER_HINT` | in-game hints |
| [ ] | 220 | `OPEN_UI` | server asking to open a window |
| [ ] | 225 | `SHOW_COMBO` | combo counter |
| [ ] | 242 | `APPLY_MONSTER_STATUS` | monster debuffs invisible |
| [ ] | 243 | `CANCEL_MONSTER_STATUS` | monster debuffs invisible |
| [ ] | 246 | `DAMAGE_MONSTER` | damage from other players invisible |
| [ ] | 251 | `CATCH_MONSTER` | catching mobs |
| [ ] | 253 | `SHOW_MAGNET` | mob magnet skills |
| [ ] | 258 | `REMOVE_NPC` | npcs leaving |
| [ ] | 260 | `NPC_ACTION` | npc movement |
| [ ] | 273 | `SPAWN_MIST` | mist skills |
| [ ] | 274 | `REMOVE_MIST` | mist skills |
| [ ] | 275 | `SPAWN_DOOR` | mage doors |
| [ ] | 276 | `REMOVE_DOOR` | mage doors |
| [ ] | 309 | `STORAGE` | storage keeper |

## Everything else unhandled

Not judged - most will never come up on a private server for one family,
but they are listed so nothing is a surprise.

| opcode | name | probably irrelevant here |
|--------|------|--------------------------|
| 1 | `GUEST_ID_LOGIN` | yes |
| 2 | `ACCOUNT_INFO` | yes |
| 3 | `SERVERSTATUS` | yes |
| 4 | `GENDER_DONE` | yes |
| 5 | `CONFIRM_EULA_RESULT` | yes |
| 6 | `CHECK_PINCODE` | yes |
| 7 | `UPDATE_PINCODE` | yes |
| 8 | `VIEW_ALL_CHAR` |  |
| 9 | `SELECT_CHARACTER_BY_VAC` | yes |
| 16 | `CHANGE_CHANNEL` |  |
| 18 | `KOREAN_INTERNET_CAFE_SHIT` | yes |
| 20 | `CHANNEL_SELECTED` |  |
| 21 | `HACKSHIELD_REQUEST` | yes |
| 22 | `RELOG_RESPONSE` |  |
| 25 | `CHECK_CRC_RESULT` | yes |
| 38 | `FAME_RESPONSE` |  |
| 40 | `OPEN_FULL_CLIENT_DOWNLOAD_LINK` |  |
| 43 | `WEDDING_PHOTO` | yes |
| 45 | `CLAIM_RESULT` | yes |
| 46 | `CLAIM_AVAILABLE_TIME` | yes |
| 48 | `SET_TAMING_MOB_INFO` |  |
| 50 | `ENTRUSTED_SHOP_CHECK_RESULT` | yes |
| 51 | `SKILL_LEARN_ITEM_RESULT` |  |
| 55 | `SUE_CHARACTER_RESULT` | yes |
| 57 | `TRADE_MONEY_LIMIT` | yes |
| 59 | `GUILD_BBS_PACKET` | yes |
| 62 | `PARTY_OPERATION` |  |
| 66 | `ALLIANCE_OPERATION` | yes |
| 69 | `INCUBATOR_RESULT` | yes |
| 70 | `SHOP_SCANNER_RESULT` | yes |
| 71 | `SHOP_LINK_RESULT` |  |
| 72 | `MARRIAGE_REQUEST` | yes |
| 73 | `MARRIAGE_RESULT` | yes |
| 74 | `WEDDING_GIFT_RESULT` | yes |
| 75 | `NOTIFY_MARRIED_PARTNER_MAP_TRANSFER` |  |
| 76 | `CASH_PET_FOOD_RESULT` | yes |
| 78 | `SET_POTION_DISCOUNT_RATE` |  |
| 79 | `BRIDLE_MOB_CATCH_FAIL` |  |
| 80 | `IMITATED_NPC_RESULT` |  |
| 81 | `IMITATED_NPC_DATA` |  |
| 82 | `LIMITED_NPC_DISABLE_INFO` |  |
| 83 | `MONSTER_BOOK_SET_CARD` |  |
| 84 | `MONSTER_BOOK_SET_COVER` |  |
| 85 | `HOUR_CHANGED` |  |
| 87 | `CONSULT_AUTHKEY_UPDATE` | yes |
| 88 | `CLASS_COMPETITION_AUTHKEY_UPDATE` | yes |
| 89 | `WEB_BOARD_AUTHKEY_UPDATE` | yes |
| 90 | `SESSION_VALUE` |  |
| 91 | `PARTY_VALUE` |  |
| 92 | `FIELD_SET_VARIABLE` |  |
| 93 | `BONUS_EXP_CHANGED` |  |
| 94 | `FAMILY_CHART_RESULT` | yes |
| 95 | `FAMILY_INFO_RESULT` | yes |
| 96 | `FAMILY_RESULT` | yes |
| 97 | `FAMILY_JOIN_REQUEST` | yes |
| 98 | `FAMILY_JOIN_REQUEST_RESULT` | yes |
| 99 | `FAMILY_JOIN_ACCEPTED` | yes |
| 101 | `FAMILY_REP_GAIN` | yes |
| 102 | `FAMILY_NOTIFY_LOGIN_OR_LOGOUT` | yes |
| 103 | `FAMILY_SET_PRIVILEGE` | yes |
| 104 | `FAMILY_SUMMON_REQUEST` | yes |
| 106 | `NOTIFY_MARRIAGE` | yes |
| 109 | `MAPLE_TV_USE_RES` | yes |
| 110 | `AVATAR_MEGAPHONE_RESULT` | yes |
| 111 | `SET_AVATAR_MEGAPHONE` | yes |
| 112 | `CLEAR_AVATAR_MEGAPHONE` | yes |
| 113 | `CANCEL_NAME_CHANGE_RESULT` |  |
| 114 | `CANCEL_TRANSFER_WORLD_RESULT` |  |
| 115 | `DESTROY_SHOP_RESULT` |  |
| 116 | `FAKE_GM_NOTICE` |  |
| 117 | `SUCCESS_IN_USE_GACHAPON_BOX` | yes |
| 118 | `NEW_YEAR_CARD_RES` | yes |
| 119 | `RANDOM_MORPH_RES` |  |
| 120 | `CANCEL_NAME_CHANGE_BY_OTHER` |  |
| 121 | `SET_EXTRA_PENDANT_SLOT` |  |
| 126 | `SET_ITC` |  |
| 127 | `SET_CASH_SHOP` | yes |
| 128 | `SET_BACK_EFFECT` |  |
| 129 | `SET_MAP_OBJECT_VISIBLE` |  |
| 130 | `CLEAR_BACK_EFFECT` |  |
| 132 | `BLOCKED_SERVER` |  |
| 133 | `FORCED_MAP_EQUIP` |  |
| 136 | `SPOUSE_CHAT` |  |
| 137 | `SUMMON_ITEM_INAVAILABLE` |  |
| 139 | `FIELD_OBSTACLE_ONOFF` |  |
| 141 | `FIELD_OBSTACLE_ALL_RESET` |  |
| 142 | `BLOW_WEATHER` |  |
| 145 | `OX_QUIZ` | yes |
| 146 | `GMEVENT_INSTRUCTIONS` |  |
| 148 | `CONTI_MOVE` |  |
| 149 | `CONTI_STATE` |  |
| 151 | `SET_QUEST_TIME` |  |
| 152 | `ARIANT_RESULT` | yes |
| 153 | `SET_OBJECT_STATE` |  |
| 154 | `STOP_CLOCK` |  |
| 155 | `ARIANT_ARENA_SHOW_RESULT` | yes |
| 157 | `PYRAMID_GAUGE` | yes |
| 158 | `PYRAMID_SCORE` | yes |
| 164 | `CHALKBOARD` |  |
| 172 | `PET_NAMECHANGE` |  |
| 173 | `PET_EXCEPTION_LIST` |  |
| 181 | `SPAWN_DRAGON` |  |
| 182 | `MOVE_DRAGON` |  |
| 183 | `REMOVE_DRAGON` |  |
| 189 | `ENERGY_ATTACK` |  |
| 201 | `UPDATE_PARTYMEMBER_HP` |  |
| 202 | `GUILD_NAME_CHANGED` | yes |
| 203 | `GUILD_MARK_CHANGED` | yes |
| 204 | `THROW_GRENADE` |  |
| 207 | `DOJO_WARP_UP` | yes |
| 208 | `LUCKSACK_PASS` |  |
| 209 | `LUCKSACK_FAIL` |  |
| 210 | `MESO_BAG_MESSAGE` |  |
| 217 | `MAKER_RESULT` |  |
| 219 | `KOREAN_EVENT` | yes |
| 223 | `SPAWN_GUIDE` |  |
| 224 | `TALK_GUIDE` |  |
| 244 | `RESET_MONSTER_ANIMATION` |  |
| 249 | `ARIANT_THING` | yes |
| 252 | `CATCH_MONSTER_WITH_ITEM` |  |
| 265 | `SPAWN_HIRED_MERCHANT` | yes |
| 266 | `DESTROY_HIRED_MERCHANT` | yes |
| 267 | `UPDATE_HIRED_MERCHANT` | yes |
| 270 | `CANNOT_SPAWN_KITE` |  |
| 271 | `SPAWN_KITE` |  |
| 272 | `REMOVE_KITE` |  |
| 281 | `SNOWBALL_STATE` | yes |
| 282 | `HIT_SNOWBALL` | yes |
| 283 | `SNOWBALL_MESSAGE` | yes |
| 284 | `LEFT_KNOCK_BACK` |  |
| 285 | `COCONUT_HIT` | yes |
| 286 | `COCONUT_SCORE` | yes |
| 287 | `GUILD_BOSS_HEALER_MOVE` | yes |
| 288 | `GUILD_BOSS_PULLEY_STATE_CHANGE` | yes |
| 289 | `MONSTER_CARNIVAL_START` | yes |
| 290 | `MONSTER_CARNIVAL_OBTAINED_CP` | yes |
| 291 | `MONSTER_CARNIVAL_PARTY_CP` | yes |
| 292 | `MONSTER_CARNIVAL_SUMMON` | yes |
| 293 | `MONSTER_CARNIVAL_MESSAGE` | yes |
| 294 | `MONSTER_CARNIVAL_DIED` | yes |
| 295 | `MONSTER_CARNIVAL_LEAVE` | yes |
| 297 | `ARIANT_ARENA_USER_SCORE` | yes |
| 299 | `SHEEP_RANCH_INFO` | yes |
| 300 | `SHEEP_RANCH_CLOTHES` | yes |
| 301 | `WITCH_TOWER_SCORE_UPDATE` | yes |
| 302 | `HORNTAIL_CAVE` |  |
| 303 | `ZAKUM_SHRINE` |  |
| 307 | `ADMIN_SHOP_MESSAGE` | yes |
| 308 | `ADMIN_SHOP` | yes |
| 310 | `FREDRICK_MESSAGE` | yes |
| 311 | `FREDRICK` | yes |
| 312 | `RPS_GAME` | yes |
| 313 | `MESSENGER` | yes |
| 315 | `TOURNAMENT` | yes |
| 316 | `TOURNAMENT_MATCH_TABLE` | yes |
| 317 | `TOURNAMENT_SET_PRIZE` | yes |
| 318 | `TOURNAMENT_UEW` | yes |
| 319 | `TOURNAMENT_CHARACTERS` | yes |
| 320 | `WEDDING_PROGRESS` | yes |
| 321 | `WEDDING_CEREMONY_END` | yes |
| 322 | `PARCEL` | yes |
| 323 | `CHARGE_PARAM_RESULT` | yes |
| 324 | `QUERY_CASH_RESULT` | yes |
| 325 | `CASHSHOP_OPERATION` | yes |
| 326 | `CASHSHOP_PURCHASE_EXP_CHANGED` | yes |
| 327 | `CASHSHOP_GIFT_INFO_RESULT` | yes |
| 328 | `CASHSHOP_CHECK_NAME_CHANGE` | yes |
| 329 | `CASHSHOP_CHECK_NAME_CHANGE_POSSIBLE_RESULT` | yes |
| 330 | `CASHSHOP_REGISTER_NEW_CHARACTER_RESULT` | yes |
| 331 | `CASHSHOP_CHECK_TRANSFER_WORLD_POSSIBLE_RESULT` | yes |
| 332 | `CASHSHOP_GACHAPON_STAMP_RESULT` | yes |
| 333 | `CASHSHOP_CASH_ITEM_GACHAPON_RESULT` | yes |
| 334 | `CASHSHOP_CASH_GACHAPON_OPEN_RESULT` | yes |
| 337 | `AUTO_MP_POT` |  |
| 341 | `SEND_TV` | yes |
| 342 | `REMOVE_TV` | yes |
| 343 | `ENABLE_TV` | yes |
| 347 | `MTS_OPERATION2` | yes |
| 348 | `MTS_OPERATION` | yes |
| 349 | `MAPLELIFE_RESULT` | yes |
| 350 | `MAPLELIFE_ERROR` | yes |
| 354 | `VICIOUS_HAMMER` |  |
| 358 | `VEGA_SCROLL` |  |
