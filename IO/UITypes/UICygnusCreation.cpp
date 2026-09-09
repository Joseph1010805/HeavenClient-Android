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
#pragma once

#include "UICygnusCreation.h"
#include "UIRaceSelect.h"
#include "UILoginNotice.h"
#include "UICharSelect.h"

#include "../UI.h"

#include "../../Constants.h"
#include "../Configuration.h"

#include "../Components/MapleButton.h"
#include "../Data/ItemData.h"
#include "../Audio/Audio.h"

#include "../Net/Packets/CharCreationPackets.h"

#include <nlnx/nx.hpp>

namespace ms
{
	UICygnusCreation::UICygnusCreation() : UIElement(Point<int16_t>(0, 0), Point<int16_t>(Constants::Constants::get().get_viewwidth(), Constants::Constants::get().get_viewheight()))
	{
		gender = false;
		charSet = false;
		named = false;

		std::string version_text = Configuration::get().get_version();
		version = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::LEMONGRASS, "Ver. " + version_text);

		nl::node Login = nl::nx::ui["Login.img"];
		nl::node Common = Login["Common"];
		nl::node CustomizeChar = Login["CustomizeChar"]["1000"];
		//todo: (rich) nx
		//nl::node signboard = nl::nx::mapLatest["Obj"]["login.img"]["NewChar"]["signboard"];
		nl::node board = CustomizeChar["board"];
		nl::node genderSelect = CustomizeChar["genderSelect"];

		// Custom artwork from Map001.nx - see UILogin. Shares the character
		// select background; the scrolling sky and cloud layers are gone with
		// it, since the video is a whole scene.
		nl::node custom = nl::nx::map001["Custom"];

		if (custom["CharBg"])
		{
			sprites.emplace_back(custom["CharBg"], DrawArgument(Point<int16_t>(0, 0), Point<int16_t>(Constants::Constants::get().get_viewwidth(), Constants::Constants::get().get_viewheight())));
		}
		else
		{
			// The stock scene, for a checkout without the custom file. The sky
			// and cloud layers it used to scroll are gone either way - they
			// belong to a backdrop this screen no longer draws.
			nl::node back = nl::nx::map001["Back"]["login.img"]["back"];

			sprites.emplace_back(back["46"], Point<int16_t>(400, 300));
		}
		//sprites.emplace_back(signboard["2"], DrawArgument(Point<int16_t>(212, 217), 2.0f));
		sprites_gender_select.emplace_back(board["genderTop"], Point<int16_t>(423, 104));
		sprites_gender_select.emplace_back(board["boardMid"], Point<int16_t>(423, 222));
		sprites_gender_select.emplace_back(board["boardBottom"], Point<int16_t>(423, 348));
		sprites_lookboard.emplace_back(board["avatarTop"], Point<int16_t>(415, 89));
		sprites_lookboard.emplace_back(board["boardMid"], Point<int16_t>(415, 207));
		sprites_lookboard.emplace_back(board["boardBottom"], Point<int16_t>(415, 351));

		for (size_t i = 0; i <= 6; i++)
		{
			int16_t y = 0;

			if (i == 3)
				y = 2;

			sprites_lookboard.emplace_back(CustomizeChar["avatarSel"][i]["normal"], Point<int16_t>(416, 98 + y));
		}

		buttons[Buttons::BT_CHARC_GENDER_M] = std::make_unique<MapleButton>(genderSelect["male"], Point<int16_t>(425, 107));
		buttons[Buttons::BT_CHARC_GEMDER_F] = std::make_unique<MapleButton>(genderSelect["female"], Point<int16_t>(423, 107));
		buttons[Buttons::BT_CHARC_SKINL] = std::make_unique<MapleButton>(CustomizeChar["BtLeft"], Point<int16_t>(418, 81 + (4 * 18)));
		buttons[Buttons::BT_CHARC_SKINR] = std::make_unique<MapleButton>(CustomizeChar["BtRight"], Point<int16_t>(415, 81 + (4 * 18)));
		buttons[Buttons::BT_CHARC_WEPL] = std::make_unique<MapleButton>(CustomizeChar["BtLeft"], Point<int16_t>(418, 81 + (8 * 18)));
		buttons[Buttons::BT_CHARC_WEPR] = std::make_unique<MapleButton>(CustomizeChar["BtRight"], Point<int16_t>(415, 81 + (8 * 18)));

		// THE ROWS THAT HAD NO ARROWS.
		//
		// Skin sits at button row 4 and its label at text row 2; weapon at 8
		// and 6. So a row's arrows are always TWO steps further down in
		// button space than the label is in text space, and the two spaces
		// are a constant 101px apart. Everything below is placed off that
		// relationship rather than off measured pixels, so a row cannot end
		// up somewhere its label is not.
		//
		//   face  label -1  ->  arrows 1
		//   hair  label  0  ->  arrows 2
		//   top   label  3  ->  arrows 5
		//   bot   label  4  ->  arrows 6
		//   shoe  label  5  ->  arrows 7
		auto arrows = [&](uint16_t left, uint16_t right, int row)
		{
			buttons[left] = std::make_unique<MapleButton>(
				CustomizeChar["BtLeft"], Point<int16_t>(418, 81 + (row * 18)));
			buttons[right] = std::make_unique<MapleButton>(
				CustomizeChar["BtRight"], Point<int16_t>(415, 81 + (row * 18)));

			buttons[left]->set_active(false);
			buttons[right]->set_active(false);
		};

		arrows(Buttons::BT_CHARC_FACEL, Buttons::BT_CHARC_FACER, 1);
		arrows(Buttons::BT_CHARC_HAIRL, Buttons::BT_CHARC_HAIRR, 2);
		arrows(Buttons::BT_CHARC_TOPL,  Buttons::BT_CHARC_TOPR,  5);
		arrows(Buttons::BT_CHARC_BOTL,  Buttons::BT_CHARC_BOTR,  6);
		arrows(Buttons::BT_CHARC_SHOEL, Buttons::BT_CHARC_SHOER, 7);

		for (size_t i = 0; i <= 7; i++)
		{
			buttons[Buttons::BT_CHARC_HAIRC0 + i] = std::make_unique<MapleButton>(CustomizeChar["hairSelect"][i], Point<int16_t>(553 + (i * 15), 238));
			buttons[Buttons::BT_CHARC_HAIRC0 + i]->set_active(false);
		}

		buttons[Buttons::BT_CHARC_SKINL]->set_active(false);
		buttons[Buttons::BT_CHARC_SKINR]->set_active(false);
		buttons[Buttons::BT_CHARC_WEPL]->set_active(false);
		buttons[Buttons::BT_CHARC_WEPR]->set_active(false);

		buttons[Buttons::BT_CHARC_OK] = std::make_unique<MapleButton>(CustomizeChar["BtYes"], Point<int16_t>(510, 396));
		buttons[Buttons::BT_CHARC_CANCEL] = std::make_unique<MapleButton>(CustomizeChar["BtNo"], Point<int16_t>(615, 396));

		nameboard = CustomizeChar["charName"];
		namechar = Textfield(Text::Font::A13M, Text::Alignment::LEFT, Color::Name::BLACK, Rectangle<int16_t>(Point<int16_t>(539, 209), Point<int16_t>(631, 252)), 12);

		sprites.emplace_back(Common["frame"], Point<int16_t>(400, 300));
		sprites.emplace_back(Common["step"]["3"], Point<int16_t>(40, 0));

		buttons[Buttons::BT_BACK] = std::make_unique<MapleButton>(Login["Common"]["BtStart"], Point<int16_t>(0, 515));

		namechar.set_state(Textfield::DISABLED);

		namechar.set_enter_callback(
			[&](std::string)
			{
				button_pressed(Buttons::BT_CHARC_OK);
			}
		);

		namechar.set_key_callback(
			KeyAction::Id::ESCAPE,
			[&]()
			{
				button_pressed(Buttons::BT_CHARC_CANCEL);
			}
		);

		facename = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::BLACK);
		hairname = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::BLACK);
		bodyname = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::BLACK);
		topname = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::BLACK);
		botname = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::BLACK);
		shoename = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::BLACK);
		wepname = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::BLACK);

		// Cygnus Knights have their own set - PremiumChar* - which is NOT
		// under Info, where the explorer lists live. Reading Info meant a
		// Noblesse was built entirely out of explorer parts.
		//
		// In v83 data this changes nothing visible: PremiumCharMale holds the
		// same hair (30030/30020/30000) and the same clothes (1040002/6/10)
		// as the explorer list, because character creation for this class had
		// not been drawn yet even though Ereve exists. Corrected anyway - it
		// is the right node, and data that does carry the white-and-blue
		// Noblesse outfit will then simply work.
		nl::node mkinfo = nl::nx::etc["MakeCharInfo.img"];

		for (size_t i = 0; i < 2; i++)
		{
			bool f;
			nl::node CharGender;

			if (i == 0)
			{
				f = true;
				CharGender = mkinfo["PremiumCharFemale"];
			}
			else
			{
				f = false;
				CharGender = mkinfo["PremiumCharMale"];
			}

			for (auto node : CharGender)
			{
				int num = stoi(node.name());

				for (auto idnode : node)
				{
					int32_t value = idnode;

					switch (num)
					{
					case 0:
						faces[f].push_back(value);
						break;
					case 1:
						hairs[f].push_back(value);
						break;
					case 2:
						haircolors[f].push_back(static_cast<uint8_t>(value));
						break;
					case 3:
						skins[f].push_back(static_cast<uint8_t>(value));
						break;
					case 4:
						tops[f].push_back(value);
						break;
					case 5:
						bots[f].push_back(value);
						break;
					case 6:
						shoes[f].push_back(value);
						break;
					case 7:
						weapons[f].push_back(value);
						break;
					}
				}
			}
		}

		// ⚠ THE KNIGHTS' OWN SET, WHICH THE DATA DOES NOT CARRY.
		//
		// v83 shipped the class before it shipped the look: PremiumChar* in
		// Etc.nx is the explorer list under a different name, so a Cygnus
		// Knight came out looking like anybody else.
		//
		// These are chosen from art that IS in v83 - a restricted palette
		// led by blond and BLUE, which is as close as this version gets to
		// the blue-and-gold Ereve look, plus the neatest of the short cuts.
		//
		// ⚠ THE SERVER VALIDATES AGAINST ITS OWN COPY of MakeCharInfo
		// (wz/Etc.wz/MakeCharInfo.img.xml, read by MakeCharInfoValidator).
		// THESE TWO LISTS MUST AGREE or creation is refused with no
		// explanation. Cosmic's copy was missing sections 0 and 4-7 for
		// Premium entirely, which made Cygnus creation impossible - it has
		// been filled in to match this.
		{
			static const int32_t M_FACE[] = { 20000, 20001, 20012 };
			static const int32_t F_FACE[] = { 21000, 21001, 21012 };
			static const int32_t M_HAIR[] = { 30300, 30030, 30830, 30930 };
			static const int32_t F_HAIR[] = { 31050, 31000, 31150, 31410 };

			// Blond and blue first - the two that read as a Knight.
			static const uint8_t COLOURS[] = { 3, 5, 7, 0 };

			// THE ROBE IS WHAT A KNIGHT STARTS IN.
			//
			// 1052177, the Fancy Noblesse Robe - the only top offered, so it
			// is worn from the first second rather than randomised away.
			//
			// The HAT (1002869) is deliberately NOT here: Ereve's intro
			// quests hand it over (20002, and 20011 with the robe), and
			// giving it at creation would take the reward off the only
			// quests that exist to give it.
			static const int32_t ROBE[] = { 1052177 };

			auto fill = [](std::vector<int32_t>& into, const int32_t* from, size_t n)
			{
				into.assign(from, from + n);
			};

			fill(faces[false], M_FACE, 3);
			fill(faces[true],  F_FACE, 3);
			fill(hairs[false], M_HAIR, 4);
			fill(hairs[true],  F_HAIR, 4);

			fill(tops[false], ROBE, 1);
			fill(tops[true],  ROBE, 1);

			for (size_t g = 0; g < 2; g++)
				haircolors[g].assign(COLOURS, COLOURS + 4);
		}

		female = false;
		randomize_look();

		newchar.set_direction(true);
	}

	void UICygnusCreation::draw(float inter) const
	{
		if (!gender)
		{
			UIElement::draw_sprites(inter);

			for (size_t i = 0; i < sprites_gender_select.size(); i++)
			{
				if (i == 1)
				{
					for (size_t f = 0; f <= 6; f++)
						sprites_gender_select[i].draw(position + Point<int16_t>(0, 18 * f), inter);
				}
				else
				{
					sprites_gender_select[i].draw(position, inter);
				}
			}

			UIElement::draw_buttons(inter);

			newchar.draw(Point<int16_t>(394, 339), inter);
		}
		else
		{
			if (!charSet)
			{
				UIElement::draw_sprites(inter);

				for (size_t i = 0; i < sprites_lookboard.size(); i++)
				{
					if (i == 1)
					{
						for (size_t f = 0; f <= 7; f++)
							sprites_lookboard[i].draw(position + Point<int16_t>(0, 18 * f), inter);
					}
					else
					{
						sprites_lookboard[i].draw(position, inter);
					}
				}

				facename.draw(Point<int16_t>(620, 218 + (-1 * 18)));
				hairname.draw(Point<int16_t>(620, 218 + (0 * 18)));
				bodyname.draw(Point<int16_t>(620, 218 + (2 * 18)));
				topname.draw(Point<int16_t>(620, 218 + (3 * 18)));
				botname.draw(Point<int16_t>(620, 218 + (4 * 18)));
				shoename.draw(Point<int16_t>(620, 218 + (5 * 18)));
				wepname.draw(Point<int16_t>(620, 218 + (6 * 18)));

				newchar.draw(Point<int16_t>(394, 339), inter);

				UIElement::draw_buttons(inter);
			}
			else
			{
				if (!named)
				{
					UIElement::draw_sprites(inter);

					nameboard.draw(Point<int16_t>(423, 104));

					namechar.draw(position);
					newchar.draw(Point<int16_t>(394, 339), inter);

					UIElement::draw_buttons(inter);
				}
				else
				{
					UIElement::draw_sprites(inter);

					nameboard.draw(Point<int16_t>(423, 104));

					UIElement::draw_buttons(inter);

					for (auto& sprite : sprites_keytype)
						sprite.draw(position, inter);
				}
			}
		}

		version.draw(position + Point<int16_t>(707, 1));
	}

	void UICygnusCreation::update()
	{
		if (!gender)
		{
			for (auto& sprite : sprites_gender_select)
				sprite.update();

			newchar.update(Constants::TIMESTEP);
		}
		else
		{
			if (!charSet)
			{
				for (auto& sprite : sprites_lookboard)
					sprite.update();

				newchar.update(Constants::TIMESTEP);
			}
			else
			{
				if (!named)
				{
					namechar.update(position);
					newchar.update(Constants::TIMESTEP);
				}
				else
				{
					for (auto& sprite : sprites_keytype)
						sprite.update();

					namechar.set_state(Textfield::State::DISABLED);
				}
			}
		}

		UIElement::update();
	}

	Cursor::State UICygnusCreation::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		if (namechar.get_state() == Textfield::State::NORMAL)
		{
			if (namechar.get_bounds().contains(cursorpos))
			{
				if (clicked)
				{
					namechar.set_state(Textfield::State::FOCUSED);

					return Cursor::State::CLICKING;
				}
				else
				{
					return Cursor::State::IDLE;
				}
			}
		}

		return UIElement::send_cursor(clicked, cursorpos);
	}

	void UICygnusCreation::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed)
		{
			if (escape)
				button_pressed(Buttons::BT_CHARC_CANCEL);
			else if (keycode == KeyAction::Id::RETURN)
				button_pressed(Buttons::BT_CHARC_OK);
		}
	}

	UIElement::Type UICygnusCreation::get_type() const
	{
		return TYPE;
	}

	void UICygnusCreation::send_naming_result(bool nameused)
	{
		if (!named)
		{
			if (!nameused)
			{
				named = true;

				std::string cname = namechar.get_text();
				int32_t cface = faces[female][face];
				int32_t chair = hairs[female][hair];
				uint8_t chairc = haircolors[female][haircolor];
				uint8_t cskin = skins[female][skin];
				int32_t ctop = tops[female][top];
				int32_t cbot = bots[female][bot];
				int32_t cshoe = shoes[female][shoe];
				int32_t cwep = weapons[female][weapon];

				CreateCharPacket(cname, 0, cface, chair, chairc, cskin, ctop, cbot, cshoe, cwep, female).dispatch();

				auto onok = [&](bool alternate)
				{
					Sound(Sound::Name::SCROLLUP).play();

					UI::get().remove(UIElement::Type::LOGINNOTICE_CONFIRM);
					UI::get().remove(UIElement::Type::LOGINNOTICE);
					UI::get().remove(UIElement::Type::CLASSCREATION);
					UI::get().remove(UIElement::Type::RACESELECT);

					if (auto charselect = UI::get().get_element<UICharSelect>())
						charselect->post_add_character();
				};

				UI::get().emplace<UIKeySelect>(onok, true);
			}
			else
			{
				auto onok = [&]()
				{
					namechar.set_state(Textfield::State::FOCUSED);

					buttons[Buttons::BT_CHARC_OK]->set_state(Button::State::NORMAL);
					buttons[Buttons::BT_CHARC_CANCEL]->set_state(Button::State::NORMAL);
				};

				UI::get().emplace<UILoginNotice>(UILoginNotice::Message::NAME_IN_USE, onok);
			}
		}
	}

	Button::State UICygnusCreation::button_pressed(uint16_t buttonid)
	{
		switch (buttonid)
		{
		case Buttons::BT_CHARC_OK:
			if (!gender)
			{
				gender = true;

				buttons[Buttons::BT_CHARC_GENDER_M]->set_active(false);
				buttons[Buttons::BT_CHARC_GEMDER_F]->set_active(false);

				buttons[Buttons::BT_CHARC_SKINL]->set_active(true);
				buttons[Buttons::BT_CHARC_SKINR]->set_active(true);

				buttons[Buttons::BT_CHARC_WEPL]->set_active(true);
				buttons[Buttons::BT_CHARC_WEPR]->set_active(true);

				for (uint16_t b : { Buttons::BT_CHARC_FACEL, Buttons::BT_CHARC_FACER,
					Buttons::BT_CHARC_HAIRL, Buttons::BT_CHARC_HAIRR,
					Buttons::BT_CHARC_TOPL, Buttons::BT_CHARC_TOPR,
					Buttons::BT_CHARC_BOTL, Buttons::BT_CHARC_BOTR,
					Buttons::BT_CHARC_SHOEL, Buttons::BT_CHARC_SHOER })
					buttons[b]->set_active(true);

				// ONLY THE COLOURS THIS CLASS ACTUALLY OFFERS.
				//
				// The eight swatches are one per colour 0-7 in the game's own
				// order, and the artwork on swatch i IS colour i. Enabling
				// all eight against a restricted palette gave four buttons
				// that did nothing and four that produced a colour other than
				// the one pictured - exactly how it looked on screen.
				for (size_t i = 0; i <= 7; i++)
				{
					bool offered = false;

					for (uint8_t c : haircolors[female])
						offered = offered || (c == i);

					buttons[Buttons::BT_CHARC_HAIRC0 + i]->set_active(offered);
				}

				buttons[Buttons::BT_CHARC_OK]->set_position(Point<int16_t>(502, 381));
				buttons[Buttons::BT_CHARC_CANCEL]->set_position(Point<int16_t>(607, 381));

				return Button::State::NORMAL;
			}
			else
			{
				if (!charSet)
				{
					charSet = true;

					buttons[Buttons::BT_CHARC_SKINL]->set_active(false);
					buttons[Buttons::BT_CHARC_SKINR]->set_active(false);

					buttons[Buttons::BT_CHARC_WEPL]->set_active(false);
					buttons[Buttons::BT_CHARC_WEPR]->set_active(false);

					for (size_t i = 0; i <= 7; i++)
						buttons[Buttons::BT_CHARC_HAIRC0 + i]->set_active(false);

					buttons[Buttons::BT_CHARC_OK]->set_position(Point<int16_t>(510, 289));
					buttons[Buttons::BT_CHARC_CANCEL]->set_position(Point<int16_t>(615, 289));

					namechar.set_state(Textfield::State::FOCUSED);

					return Button::State::NORMAL;
				}
				else
				{
					if (!named)
					{
						std::string name = namechar.get_text();

						if (name.size() <= 0)
						{
							return Button::State::NORMAL;
						}
						else if (name.size() >= 4)
						{
							namechar.set_state(Textfield::State::DISABLED);

							buttons[Buttons::BT_CHARC_OK]->set_state(Button::State::DISABLED);
							buttons[Buttons::BT_CHARC_CANCEL]->set_state(Button::State::DISABLED);

							if (auto raceselect = UI::get().get_element<UIRaceSelect>())
							{
								if (raceselect->check_name(name))
								{
									NameCharPacket(name).dispatch();

									return Button::State::IDENTITY;
								}
							}

							std::function<void()> okhandler = [&]()
							{
								namechar.set_state(Textfield::State::FOCUSED);

								buttons[Buttons::BT_CHARC_OK]->set_state(Button::State::NORMAL);
								buttons[Buttons::BT_CHARC_CANCEL]->set_state(Button::State::NORMAL);
							};

							UI::get().emplace<UILoginNotice>(UILoginNotice::Message::ILLEGAL_NAME, okhandler);

							return Button::State::NORMAL;
						}
						else
						{
							namechar.set_state(Textfield::State::DISABLED);

							buttons[Buttons::BT_CHARC_OK]->set_state(Button::State::DISABLED);
							buttons[Buttons::BT_CHARC_CANCEL]->set_state(Button::State::DISABLED);

							std::function<void()> okhandler = [&]()
							{
								namechar.set_state(Textfield::State::FOCUSED);

								buttons[Buttons::BT_CHARC_OK]->set_state(Button::State::NORMAL);
								buttons[Buttons::BT_CHARC_CANCEL]->set_state(Button::State::NORMAL);
							};

							UI::get().emplace<UILoginNotice>(UILoginNotice::Message::ILLEGAL_NAME, okhandler);

							return Button::State::IDENTITY;
						}
					}
					else
					{
						return Button::State::NORMAL;
					}
				}
			}
		case BT_BACK:
			Sound(Sound::Name::SCROLLUP).play();

			UI::get().remove(UIElement::Type::CLASSCREATION);
			UI::get().emplace<UIRaceSelect>();

			return Button::State::NORMAL;
		case Buttons::BT_CHARC_CANCEL:
			if (charSet)
			{
				charSet = false;

				buttons[Buttons::BT_CHARC_SKINL]->set_active(true);
				buttons[Buttons::BT_CHARC_SKINR]->set_active(true);

				buttons[Buttons::BT_CHARC_WEPL]->set_active(true);
				buttons[Buttons::BT_CHARC_WEPR]->set_active(true);

				for (uint16_t b : { Buttons::BT_CHARC_FACEL, Buttons::BT_CHARC_FACER,
					Buttons::BT_CHARC_HAIRL, Buttons::BT_CHARC_HAIRR,
					Buttons::BT_CHARC_TOPL, Buttons::BT_CHARC_TOPR,
					Buttons::BT_CHARC_BOTL, Buttons::BT_CHARC_BOTR,
					Buttons::BT_CHARC_SHOEL, Buttons::BT_CHARC_SHOER })
					buttons[b]->set_active(true);

				// ONLY THE COLOURS THIS CLASS ACTUALLY OFFERS.
				//
				// The eight swatches are one per colour 0-7 in the game's own
				// order, and the artwork on swatch i IS colour i. Enabling
				// all eight against a restricted palette gave four buttons
				// that did nothing and four that produced a colour other than
				// the one pictured - exactly how it looked on screen.
				for (size_t i = 0; i <= 7; i++)
				{
					bool offered = false;

					for (uint8_t c : haircolors[female])
						offered = offered || (c == i);

					buttons[Buttons::BT_CHARC_HAIRC0 + i]->set_active(offered);
				}

				buttons[Buttons::BT_CHARC_OK]->set_position(Point<int16_t>(502, 381));
				buttons[Buttons::BT_CHARC_CANCEL]->set_position(Point<int16_t>(607, 381));

				namechar.set_state(Textfield::State::DISABLED);

				return Button::State::NORMAL;
			}
			else
			{
				if (gender)
				{
					gender = false;

					buttons[Buttons::BT_CHARC_GENDER_M]->set_active(true);
					buttons[Buttons::BT_CHARC_GEMDER_F]->set_active(true);

					buttons[Buttons::BT_CHARC_SKINL]->set_active(false);
					buttons[Buttons::BT_CHARC_SKINR]->set_active(false);

					buttons[Buttons::BT_CHARC_WEPL]->set_active(false);
					buttons[Buttons::BT_CHARC_WEPR]->set_active(false);

					for (size_t i = 0; i <= 7; i++)
						buttons[Buttons::BT_CHARC_HAIRC0 + i]->set_active(false);

					buttons[Buttons::BT_CHARC_OK]->set_position(Point<int16_t>(510, 396));
					buttons[Buttons::BT_CHARC_CANCEL]->set_position(Point<int16_t>(615, 396));

					return Button::State::NORMAL;
				}
				else
				{
					button_pressed(Buttons::BT_BACK);

					return Button::State::NORMAL;
				}
			}
		case Buttons::BT_CHARC_HAIRC0:
		case Buttons::BT_CHARC_HAIRC1:
		case Buttons::BT_CHARC_HAIRC2:
		case Buttons::BT_CHARC_HAIRC3:
		case Buttons::BT_CHARC_HAIRC4:
		case Buttons::BT_CHARC_HAIRC5:
		case Buttons::BT_CHARC_HAIRC6:
		case Buttons::BT_CHARC_HAIRC7:
		{
			// A swatch, not a step.
			//
			// These eight are colour samples - clicking the third one should
			// give you the third colour. Every one of them used to walk
			// BACKWARDS through the list by one instead, so the colour you got
			// depended on how many times you had clicked rather than on which
			// you picked, and reaching a particular one meant counting.
			// THE SWATCH IS A COLOUR, NOT AN INDEX.
			//
			// This read the button number as a position in haircolors[] - so
			// with a restricted palette of {3,5,7,0}, pressing the BLACK
			// swatch produced blond and the last four did nothing. The swatch
			// number IS the colour; find where that colour sits in the list.
			uint8_t colour = static_cast<uint8_t>(buttonid - Buttons::BT_CHARC_HAIRC0);

			size_t wanted = haircolors[female].size();

			for (size_t i = 0; i < haircolors[female].size(); i++)
				if (haircolors[female][i] == colour)
					wanted = i;

			if (wanted >= haircolors[female].size())
				return Button::State::NORMAL;

			haircolor = static_cast<int32_t>(wanted);
			newchar.set_hair(hairs[female][hair] + haircolors[female][haircolor]);

			return Button::State::NORMAL;
		}
		// STEPPING THE ROWS THAT COULD ONLY BE READ BEFORE.
		//
		// Same wrap-around walk the skin and weapon arrows already use. A
		// list of one - the robe is the only Cygnus top - simply lands back
		// on itself, so a fixed row costs nothing and stays consistent if
		// more are offered later.
		case Buttons::BT_CHARC_FACEL:
		case Buttons::BT_CHARC_FACER:
		{
			size_t n = faces[female].size();
			face = (buttonid == Buttons::BT_CHARC_FACEL)
				? (face > 0 ? face - 1 : n - 1)
				: (face + 1 < n ? face + 1 : 0);

			newchar.set_face(faces[female][face]);
			facename.change_text(newchar.get_face()->get_name());

			return Button::State::NORMAL;
		}
		case Buttons::BT_CHARC_HAIRL:
		case Buttons::BT_CHARC_HAIRR:
		{
			size_t n = hairs[female].size();
			hair = (buttonid == Buttons::BT_CHARC_HAIRL)
				? (hair > 0 ? hair - 1 : n - 1)
				: (hair + 1 < n ? hair + 1 : 0);

			newchar.set_hair(hairs[female][hair] + haircolors[female][haircolor]);
			hairname.change_text(newchar.get_hair()->get_name());

			return Button::State::NORMAL;
		}
		case Buttons::BT_CHARC_TOPL:
		case Buttons::BT_CHARC_TOPR:
		{
			size_t n = tops[female].size();
			top = (buttonid == Buttons::BT_CHARC_TOPL)
				? (top > 0 ? top - 1 : n - 1)
				: (top + 1 < n ? top + 1 : 0);

			newchar.add_equip(tops[female][top]);
			topname.change_text(get_equipname(Equipslot::Id::TOP));

			return Button::State::NORMAL;
		}
		case Buttons::BT_CHARC_BOTL:
		case Buttons::BT_CHARC_BOTR:
		{
			size_t n = bots[female].size();
			bot = (buttonid == Buttons::BT_CHARC_BOTL)
				? (bot > 0 ? bot - 1 : n - 1)
				: (bot + 1 < n ? bot + 1 : 0);

			newchar.add_equip(bots[female][bot]);
			botname.change_text(get_equipname(Equipslot::Id::BOTTOM));

			return Button::State::NORMAL;
		}
		case Buttons::BT_CHARC_SHOEL:
		case Buttons::BT_CHARC_SHOER:
		{
			size_t n = shoes[female].size();
			shoe = (buttonid == Buttons::BT_CHARC_SHOEL)
				? (shoe > 0 ? shoe - 1 : n - 1)
				: (shoe + 1 < n ? shoe + 1 : 0);

			newchar.add_equip(shoes[female][shoe]);
			shoename.change_text(get_equipname(Equipslot::Id::SHOES));

			return Button::State::NORMAL;
		}
		case Buttons::BT_CHARC_SKINL:
			skin = (skin > 0) ? skin - 1 : skins[female].size() - 1;
			newchar.set_body(skins[female][skin]);
			bodyname.change_text(newchar.get_body()->get_name());

			return Button::State::NORMAL;
		case Buttons::BT_CHARC_SKINR:
			skin = (skin < skins[female].size() - 1) ? skin + 1 : 0;
			newchar.set_body(skins[female][skin]);
			bodyname.change_text(newchar.get_body()->get_name());

			return Button::State::NORMAL;
		case Buttons::BT_CHARC_WEPL:
			weapon = (weapon > 0) ? weapon - 1 : weapons[female].size() - 1;
			newchar.add_equip(weapons[female][weapon]);
			wepname.change_text(get_equipname(Equipslot::Id::WEAPON));

			return Button::State::NORMAL;
		case Buttons::BT_CHARC_WEPR:
			weapon = (weapon < weapons[female].size() - 1) ? weapon + 1 : 0;
			newchar.add_equip(weapons[female][weapon]);
			wepname.change_text(get_equipname(Equipslot::Id::WEAPON));

			return Button::State::NORMAL;
		case Buttons::BT_CHARC_GENDER_M:
			if (female)
			{
				female = false;
				randomize_look();
			}

			return Button::State::NORMAL;
		case Buttons::BT_CHARC_GEMDER_F:
			if (!female)
			{
				female = true;
				randomize_look();
			}

			return Button::State::NORMAL;
		}

		return Button::State::PRESSED;
	}

	void UICygnusCreation::randomize_look()
	{
		hair = 0;
		face = 0;
		skin = randomizer.next_int(skins[female].size());
		haircolor = randomizer.next_int(haircolors[female].size());
		top = 0;
		bot = 0;
		shoe = 0;
		weapon = randomizer.next_int(weapons[female].size());

		newchar.set_body(skins[female][skin]);
		newchar.set_face(faces[female][face]);
		newchar.set_hair(hairs[female][hair] + haircolors[female][haircolor]);
		newchar.add_equip(tops[female][top]);
		newchar.add_equip(bots[female][bot]);
		newchar.add_equip(shoes[female][shoe]);
		newchar.add_equip(weapons[female][weapon]);

		bodyname.change_text(newchar.get_body()->get_name());
		facename.change_text(newchar.get_face()->get_name());
		hairname.change_text(newchar.get_hair()->get_name());
		topname.change_text(get_equipname(Equipslot::Id::TOP));
		botname.change_text(get_equipname(Equipslot::Id::BOTTOM));
		shoename.change_text(get_equipname(Equipslot::Id::SHOES));
		wepname.change_text(get_equipname(Equipslot::Id::WEAPON));
	}

	const std::string& UICygnusCreation::get_equipname(Equipslot::Id slot) const
	{
		if (int32_t item_id = newchar.get_equips().get_equip(slot))
		{
			return ItemData::get(item_id).get_name();
		}
		else
		{
			static const std::string& nullstr = "Missing name.";

			return nullstr;
		}
	}
}