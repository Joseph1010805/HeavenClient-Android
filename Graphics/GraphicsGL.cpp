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
#include "GraphicsGL.h"

#include <cstring>
#include <vector>

#include "../Configuration.h"

// Said out loud when a sprite cannot be given room in the atlas. Two
// explanations for the wrong-texture bug have been wrong already, so this
// reports the moment the atlas actually runs out rather than leaving it to be
// inferred from what is drawn.
#define LOG_ATLAS_FULL(x, y, w, h) \
	printf("[!] atlas full at %d,%d - %dx%d sprite skipped, reset queued\n", (x), (y), (w), (h))

#define LOG_ATLAS_PUT(id, x, y, w, h) \
	printf("[*] atlas put id %llu at %d,%d size %dx%d\n", (id), (x), (y), (w), (h))

namespace ms
{
	GraphicsGL::GraphicsGL()
	{
		locked = false;

		VWIDTH = Constants::Constants::get().get_viewwidth();
		VHEIGHT = Constants::Constants::get().get_viewheight();
		SCREEN = Rectangle<int16_t>(0, VWIDTH, 0, VHEIGHT);
	}

#if defined(PLATFORM_ANDROID)
	namespace
	{
		// GLES2 is far stricter than desktop GL about texture formats and it
		// reports that by setting an error rather than by failing loudly, so a
		// bad upload just leaves the atlas empty and everything draws as the
		// clear colour. Log the error code against the call that produced it.
		void check_gl(const char* what)
		{
			for (GLenum err = glGetError(); err != GL_NO_ERROR; err = glGetError())
				printf("[!] GL error 0x%04X after %s\n", err, what);
		}
	}
#else
	namespace
	{
		void check_gl(const char*) {}
	}
#endif

	Error GraphicsGL::init()
	{
		// Setup parameters
		// ----------------
#if defined(PLATFORM_ANDROID)
		// GLSL ES 1.00. Same program as the desktop GLSL 1.20 source below,
		// with the three things ES is stricter about:
		//
		//   1. #version 100
		//   2. fragment shaders must declare a default float precision
		//   3. no implicit int -> float conversion. The desktop source relies
		//      on it in "coord.y + yoffset", "texpos.y == 0" and
		//      "texpos.y <= fontregion"; each is made explicit here. Left
		//      implicit, these fail at glCompileShader on device - a runtime
		//      failure with no compile-time warning.
		const char* vertexShaderSource =
			"#version 100\n"
			"attribute vec4 coord;"
			"attribute vec4 color;"
			// highp is essential, not cosmetic. The atlas is 8192x8192, and a
			// mediump float carries only about 10 bits of mantissa - roughly
			// 1024 distinguishable steps across a normalised coordinate, which
			// over 8192 pixels quantises every lookup to ~8 pixel blocks.
			// Backgrounds survive that; glyphs and button labels are shredded
			// by it. Desktop GL has no such limit, so this only bites here.
			"varying highp vec2 texpos;"
			"varying vec4 colormod;"
			"uniform highp vec2 screensize;"
			"uniform int yoffset;"

			"void main(void)"
			"{"
			"	float x = -1.0 + coord.x * 2.0 / screensize.x;"
			"	float y = 1.0 - (coord.y + float(yoffset)) * 2.0 / screensize.y;"
			"   gl_Position = vec4(x, y, 0.0, 1.0);"
			"	texpos = coord.zw;"
			"	colormod = color;"
			"}";

		const char* fragmentShaderSource =
			"#version 100\n"
			"precision mediump float;"
			// The texture coordinate and the atlas size must both be highp -
			// see the vertex shader. Colour stays mediump, which is plenty for
			// an 8 bit channel, so this costs nothing where precision does not
			// matter.
			"varying highp vec2 texpos;"
			"varying vec4 colormod;"
			"uniform sampler2D texture;"
			"uniform highp vec2 atlassize;"
			"uniform int fontregion;"

			"void main(void)"
			"{"
			"	if (texpos.y == 0.0)"
			"	{"
			"		gl_FragColor = colormod;"
			"	}"
			"	else if (texpos.y <= float(fontregion))"
			"	{"
			"		gl_FragColor = vec4(1.0, 1.0, 1.0, texture2D(texture, texpos / atlassize).r) * colormod;"
			"	}"
			"	else"
			"	{"
			"		gl_FragColor = texture2D(texture, texpos / atlassize) * colormod;"
			"	}"
			"}";
#else
		const char* vertexShaderSource =
			"#version 120\n"
			"attribute vec4 coord;"
			"attribute vec4 color;"
			"varying vec2 texpos;"
			"varying vec4 colormod;"
			"uniform vec2 screensize;"
			"uniform int yoffset;"

			"void main(void)"
			"{"
			"	float x = -1.0 + coord.x * 2.0 / screensize.x;"
			"	float y = 1.0 - (coord.y + yoffset) * 2.0 / screensize.y;"
			"   gl_Position = vec4(x, y, 0.0, 1.0);"
			"	texpos = coord.zw;"
			"	colormod = color;"
			"}";

		const char* fragmentShaderSource =
			"#version 120\n"
			"varying vec2 texpos;"
			"varying vec4 colormod;"
			"uniform sampler2D texture;"
			"uniform vec2 atlassize;"
			"uniform int fontregion;"

			"void main(void)"
			"{"
			"	if (texpos.y == 0)"
			"	{"
			"		gl_FragColor = colormod;"
			"	}"
			"	else if (texpos.y <= fontregion)"
			"	{"
			"		gl_FragColor = vec4(1, 1, 1, texture2D(texture, texpos / atlassize).r) * colormod;"
			"	}"
			"	else"
			"	{"
			"		gl_FragColor = texture2D(texture, texpos / atlassize) * colormod;"
			"	}"
			"}";
#endif

		const GLsizei bufSize = 512;

		GLint success;
		GLchar infoLog[bufSize];

		// Initialize and configure
		// ------------------------
		//gladLoadGL();
		//if (GLenum error = glewInit())
		//	return Error(Error::Code::GLEW, (const char*)glewGetErrorString(error));

#if !defined(PLATFORM_ANDROID)
        // Android links GLESv2 directly, so there is no loader to run.
        if(!gladLoadGL()) {
            printf("Something went wrong!\n");
            exit(-1);
        }
#endif

		std::cout << "Using OpenGL " << glGetString(GL_VERSION) << std::endl;
		//std::cout << "Using GLEW " << glewGetString(GLEW_VERSION) << std::endl;

		if (FT_Init_FreeType(&ftlibrary))
			return Error::Code::FREETYPE;

		FT_Int ftmajor;
		FT_Int ftminor;
		FT_Int ftpatch;

		FT_Library_Version(ftlibrary, &ftmajor, &ftminor, &ftpatch);

		std::cout << "Using FreeType " << ftmajor << "." << ftminor << "." << ftpatch << std::endl;

		// Build and compile our shader program
		// ------------------------------------

		// Vertex Shader
		GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
		glCompileShader(vertexShader);

		// Check for shader compile errors
		glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);

		if (success != GL_TRUE)
		{
			glGetShaderInfoLog(vertexShader, bufSize, NULL, infoLog);

			return Error(Error::Code::VERTEX_SHADER, infoLog);
		}

		// Fragment Shader
		GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
		glCompileShader(fragmentShader);

		// Check for shader compile errors
		glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);

		if (success != GL_TRUE)
		{
			glGetShaderInfoLog(fragmentShader, bufSize, NULL, infoLog);

			return Error(Error::Code::FRAGMENT_SHADER, infoLog);
		}

		// Link Shaders
		shaderProgram = glCreateProgram();
		glAttachShader(shaderProgram, vertexShader);
		glAttachShader(shaderProgram, fragmentShader);
		glLinkProgram(shaderProgram);

		// Check for linking errors
		glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);

		if (success != GL_TRUE)
		{
			glGetProgramInfoLog(shaderProgram, bufSize, NULL, infoLog);

			return Error(Error::Code::SHADER_PROGRAM_LINK, infoLog);
		}

		// Validate Program
		glValidateProgram(shaderProgram);

		// Check for validation errors
		glGetProgramiv(shaderProgram, GL_VALIDATE_STATUS, &success);

		if (success != GL_TRUE)
		{
			glGetProgramInfoLog(shaderProgram, bufSize, NULL, infoLog);

			return Error(Error::Code::SHADER_PROGRAM_VALID, infoLog);
		}

		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);

		attribute_coord = glGetAttribLocation(shaderProgram, "coord");
		attribute_color = glGetAttribLocation(shaderProgram, "color");
		uniform_texture = glGetUniformLocation(shaderProgram, "texture");
		uniform_atlassize = glGetUniformLocation(shaderProgram, "atlassize");
		uniform_screensize = glGetUniformLocation(shaderProgram, "screensize");
		uniform_yoffset = glGetUniformLocation(shaderProgram, "yoffset");
		uniform_fontregion = glGetUniformLocation(shaderProgram, "fontregion");

		if (attribute_coord == -1 || attribute_color == -1 || uniform_texture == -1 || uniform_atlassize == -1 || uniform_screensize == -1 || uniform_yoffset == -1)
			return Error::Code::SHADER_VARS;

		// Vertex Buffer Object
		glGenBuffers(1, &VBO);

		glGenTextures(1, &atlas);
		glBindTexture(GL_TEXTURE_2D, atlas);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
#if defined(PLATFORM_ANDROID)
		// Desktop GL converts freely in glTexSubImage2D, so this atlas could be
		// RGBA while sprites arrived as BGRA and glyphs as a single channel.
		// GLES2 does not convert: the format passed to glTexSubImage2D must
		// EQUAL the internal format, or the call raises GL_INVALID_OPERATION
		// and silently uploads nothing - leaving a blank atlas and a screen
		// painted entirely in the (white) clear colour.
		//
		// Sprite data is already BGRA and is by far the bulk of the traffic, so
		// the atlas is allocated as BGRA to make those uploads a straight copy;
		// the glyphs are expanded to four channels instead, which is cheap
		// because there are only 96 of them per font. Sampling a BGRA texture
		// still yields correctly ordered RGBA in the shader, so the fragment
		// program is unchanged.
		printf("[*] GL extensions advertise BGRA8888: %s\n",
			strstr(reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS)),
				"texture_format_BGRA8888") ? "yes" : "NO");

		glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA_EXT, ATLASW, ATLASH, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, nullptr);
#else
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ATLASW, ATLASH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
#endif
		check_gl("atlas glTexImage2D");

		fontborder.set_y(1);

		const std::string FONT_NORMAL = Setting<FontPathNormal>().get().load();
		const std::string FONT_BOLD = Setting<FontPathBold>().get().load();

		if (FONT_NORMAL.empty() || FONT_BOLD.empty())
			return Error::Code::FONT_PATH;

		const char* FONT_NORMAL_STR = FONT_NORMAL.c_str();
		const char* FONT_BOLD_STR = FONT_BOLD.c_str();

		addfont(FONT_NORMAL_STR, Text::Font::A11M, 0, 11);
		addfont(FONT_BOLD_STR, Text::Font::A11B, 0, 11);
		addfont(FONT_NORMAL_STR, Text::Font::A12M, 0, 12);
		addfont(FONT_BOLD_STR, Text::Font::A12B, 0, 12);
		addfont(FONT_NORMAL_STR, Text::Font::A13M, 0, 13);
		addfont(FONT_BOLD_STR, Text::Font::A13B, 0, 13);
		addfont(FONT_BOLD_STR, Text::Font::A15B, 0, 15);
		addfont(FONT_NORMAL_STR, Text::Font::A18M, 0, 18);

		fontymax += fontborder.y();

		leftovers = QuadTree<size_t, Leftover>(
			[](const Leftover& first, const Leftover& second)
			{
				bool width_comparison = first.width() >= second.width();
				bool height_comparison = first.height() >= second.height();

				if (width_comparison && height_comparison)
					return QuadTree<size_t, Leftover>::Direction::RIGHT;
				else if (width_comparison)
					return QuadTree<size_t, Leftover>::Direction::DOWN;
				else if (height_comparison)
					return QuadTree<size_t, Leftover>::Direction::UP;
				else
					return QuadTree<size_t, Leftover>::Direction::LEFT;
			}
		);

		return Error::Code::NONE;
	}

	bool GraphicsGL::addfont(const char* name, Text::Font id, FT_UInt pixelw, FT_UInt pixelh)
	{
		FT_Face face;

		if (FT_New_Face(ftlibrary, name, 0, &face))
			return false;

		if (FT_Set_Pixel_Sizes(face, pixelw, pixelh))
			return false;

		FT_GlyphSlot g = face->glyph;

		GLshort width = 0;
		GLshort height = 0;

		for (uint8_t c = 32; c < 128; c++)
		{
			if (FT_Load_Char(face, c, FT_LOAD_RENDER))
				continue;

			GLshort w = static_cast<GLshort>(g->bitmap.width);
			GLshort h = static_cast<GLshort>(g->bitmap.rows);

			width += w;

			if (h > height)
				height = h;
		}

		if (fontborder.x() + width > ATLASW)
		{
			fontborder.set_x(0);
			fontborder.set_y(fontymax);
			fontymax = 0;
		}

		GLshort x = fontborder.x();
		GLshort y = fontborder.y();

		fontborder.shift_x(width);

		if (height > fontymax)
			fontymax = height;

		fonts[id] = Font(width, height);

		GLshort ox = x;
		GLshort oy = y;

		for (uint8_t c = 32; c < 128; c++)
		{
			if (FT_Load_Char(face, c, FT_LOAD_RENDER))
				continue;

			GLshort ax = static_cast<GLshort>(g->advance.x >> 6);
			GLshort ay = static_cast<GLshort>(g->advance.y >> 6);
			GLshort l = static_cast<GLshort>(g->bitmap_left);
			GLshort t = static_cast<GLshort>(g->bitmap_top);
			GLshort w = static_cast<GLshort>(g->bitmap.width);
			GLshort h = static_cast<GLshort>(g->bitmap.rows);

#if defined(PLATFORM_ANDROID)
			// The atlas is BGRA (see init), and GLES2 will not convert on
			// upload, so the single-channel coverage FreeType hands back has
			// to be widened to four channels here. The shader samples .r for
			// the font region, and replicating the value across all channels
			// keeps that read correct whichever way the texel is ordered.
			if (w > 0 && h > 0)
			{
				std::vector<uint8_t> expanded(static_cast<size_t>(w) * h * 4);

				// Rows are pitch bytes apart, which is not necessarily the
				// glyph width - FreeType is free to pad. Walking the source
				// as if it were tightly packed shifts every row a little
				// further than the last, which shears the glyph rather than
				// failing outright. A negative pitch means the rows are
				// stored bottom-up.
				const int pitch = g->bitmap.pitch;
				const uint8_t* src = g->bitmap.buffer;

				if (pitch < 0)
					src += static_cast<size_t>(-pitch) * (h - 1);

				for (int row = 0; row < h; row++)
				{
					const uint8_t* srcrow = src + static_cast<ptrdiff_t>(pitch) * row;

					for (int col = 0; col < w; col++)
					{
						uint8_t coverage = srcrow[col];
						size_t i = static_cast<size_t>(row) * w + col;

						expanded[i * 4 + 0] = coverage;
						expanded[i * 4 + 1] = coverage;
						expanded[i * 4 + 2] = coverage;
						expanded[i * 4 + 3] = coverage;
					}
				}

				glTexSubImage2D(GL_TEXTURE_2D, 0, ox, oy, w, h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, expanded.data());
				check_gl("glyph glTexSubImage2D");
			}
#else
			glTexSubImage2D(GL_TEXTURE_2D, 0, ox, oy, w, h, GL_RED, GL_UNSIGNED_BYTE, g->bitmap.buffer);
#endif

			Offset offset = Offset(ox, oy, w, h);
			fonts[id].chars[c] = { ax, ay, w, h, l, t, offset };

			ox += w;
		}

		return true;
	}

	void GraphicsGL::reinit()
	{
		int32_t new_width = Constants::Constants::get().get_viewwidth();
		int32_t new_height = Constants::Constants::get().get_viewheight();

		if (VWIDTH != new_width || VHEIGHT != new_height)
		{
			VWIDTH = new_width;
			VHEIGHT = new_height;
			SCREEN = Rectangle<int16_t>(0, VWIDTH, 0, VHEIGHT);
		}

		glUseProgram(shaderProgram);

		glUniform1i(uniform_fontregion, fontymax);
		glUniform2f(uniform_atlassize, ATLASW, ATLASH);
		glUniform2f(uniform_screensize, VWIDTH, VHEIGHT);

		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glVertexAttribPointer(attribute_coord, 4, GL_SHORT, GL_FALSE, sizeof(Quad::Vertex), 0);
		glVertexAttribPointer(attribute_color, 4, GL_FLOAT, GL_FALSE, sizeof(Quad::Vertex), (const void*)8);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glBindTexture(GL_TEXTURE_2D, atlas);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		clearinternal();
	}

	void GraphicsGL::clearinternal()
	{
		// Logged because this is the only thing that ever resets the atlas -
		// GraphicsGL::clear() compares a fraction against 80.0 and so never
		// fires - and a reset mid-frame invalidates offsets that quads already
		// queued this frame are pointing at.
		printf("[*] atlas reset (was %d,%d)\n", border.x(), border.y());

		border = Point<GLshort>(0, fontymax);
		yrange = Range<GLshort>();

		offsets.clear();
		leftovers.clear();
		rlid = 1;
		wasted = 0;
	}

	double GraphicsGL::used_fraction() const
	{
		size_t used = ATLASW * border.y() + border.x() * yrange.second();

		return static_cast<double>(used) / (ATLASW * ATLASH);
	}

	void GraphicsGL::clear()
	{
		// Compared against 80.0 originally, while the value is a fraction
		// between 0 and 1 - so this could never fire and the atlas was only
		// ever emptied by running out of room completely.
		//
		// reset_pending is a request from a frame that ran out of room, which
		// could not empty the atlas at the time without corrupting itself.
		if (reset_pending || used_fraction() > 0.8)
		{
			reset_pending = false;
			clearinternal();
		}
	}

	void GraphicsGL::addbitmap(const nl::bitmap& bmp)
	{
		getoffset(bmp);
	}

	const GraphicsGL::Offset& GraphicsGL::getoffset(const nl::bitmap& bmp)
	{
		size_t id = bmp.id();
		auto offiter = offsets.find(id);

		if (offiter != offsets.end())
			return offiter->second;

		GLshort x = 0;
		GLshort y = 0;
		GLshort width = bmp.width();
		GLshort height = bmp.height();

		if (width <= 0 || height <= 0)
			return nulloffset;

		Leftover value = Leftover(x, y, width, height);

		// The free list is not used.
		//
		// It hands back gaps left behind by earlier sprites, and its
		// bookkeeping - erase one, add up to three replacements - gave out
		// overlapping rectangles once the atlas had been filled and reused a
		// few times. Two sprites then shared pixels: the second's upload
		// overwrote the first, and the first was afterwards drawn as the
		// second. That is what showed the world map as pieces of the Nexon
		// loading screen, and only after a second region had been loaded,
		// because until then nothing had been freed to reuse.
		//
		// Taking every allocation fresh from the cursor wastes the gaps, at
		// the cost of the atlas filling sooner and resetting more often - which
		// it already does safely, at the start of a frame.
		constexpr bool USE_LEFTOVERS = false;

		size_t lid = USE_LEFTOVERS ? leftovers.findnode(
			value,
			[](const Leftover& val, const Leftover& leaf)
			{
				return val.width() <= leaf.width() && val.height() <= leaf.height();
			}
		) : 0;

		if (lid > 0)
		{
			const Leftover& leftover = leftovers[lid];

			x = leftover.left;
			y = leftover.top;

			GLshort width_delta = leftover.width() - width;
			GLshort height_delta = leftover.height() - height;

			leftovers.erase(lid);

			wasted -= width * height;

			if (width_delta >= MINLOSIZE && height_delta >= MINLOSIZE)
			{
				leftovers.add(rlid, Leftover(x + width, y + height, width_delta, height_delta));
				rlid++;

				if (width >= MINLOSIZE)
				{
					leftovers.add(rlid, Leftover(x, y + height, width, height_delta));
					rlid++;
				}

				if (height >= MINLOSIZE)
				{
					leftovers.add(rlid, Leftover(x + width, y, width_delta, height));
					rlid++;
				}
			}
			else if (width_delta >= MINLOSIZE)
			{
				leftovers.add(rlid, Leftover(x + width, y, width_delta, height + height_delta));
				rlid++;
			}
			else if (height_delta >= MINLOSIZE)
			{
				leftovers.add(rlid, Leftover(x, y + height, width + width_delta, height_delta));
				rlid++;
			}
		}
		else
		{
			if (border.x() + width > ATLASW)
			{
				// Whether the next row down has room is decided BEFORE the
				// cursor is moved there. Moving it first and then bailing out
				// left it parked past the bottom of the atlas, and every
				// allocation after that was handed a rectangle outside the
				// texture - uploads went nowhere and the sprites sampled
				// whatever the edge clamped to. That is what drew the world map
				// as the Nexon loading screen, and it only started once the
				// atlas had filled a single time, which is why it looked like
				// clicking around caused it.
				GLshort next_row = border.y() + yrange.second();

				if (next_row + height > ATLASH)
				{
					// Out of room. Emptying the atlas here would invalidate
					// every offset the quads already queued this frame are
					// pointing at, so the reset is asked for and happens at the
					// start of the next frame, where nothing is queued. This
					// one sprite is missing until then, which is a far better
					// failure than drawing it as something else.
					reset_pending = true;

					LOG_ATLAS_FULL(border.x(), border.y(), width, height);

					return nulloffset;
				}

				border.set_x(0);
				border.set_y(next_row);

				yrange = Range<GLshort>();
			}

			x = border.x();
			y = border.y();

			border.shift_x(width);

			if (height > yrange.second())
			{
				if (x >= MINLOSIZE && height - yrange.second() >= MINLOSIZE)
				{
					leftovers.add(rlid, Leftover(0, yrange.first(), x, height - yrange.second()));
					rlid++;
				}

				wasted += x * (height - yrange.second());

				yrange = Range<int16_t>(y + height, height);
			}
			else if (height < yrange.first() - y)
			{
				if (width >= MINLOSIZE && yrange.first() - y - height >= MINLOSIZE)
				{
					leftovers.add(rlid, Leftover(x, y + height, width, yrange.first() - y - height));
					rlid++;
				}

				wasted += width * (yrange.first() - y - height);
			}
		}

		//size_t used = ATLASW * border.y() + border.x() * yrange.second();
		//double usedpercent = static_cast<double>(used) / (ATLASW * ATLASH);
		//double wastedpercent = static_cast<double>(wasted) / used;
		//Console::get().print("Used: " + std::to_string(usedpercent) + ", wasted: " + std::to_string(wastedpercent));

		// GLES2 has no core GL_BGRA. Adreno (and every GPU we target) exposes
		// GL_EXT_texture_format_BGRA8888, which defines GL_BGRA_EXT with the
		// same semantics, so the pixel data needs no CPU-side swizzle. The
		// atlas is allocated with this same format precisely so that holds.
		// TODO: fall back to a swizzle if the extension is ever absent.
#if defined(PLATFORM_ANDROID)
		// Bind the atlas rather than trusting whatever happens to be bound.
		// This upload used to rely on the binding left over from init, which
		// held only because nothing else ever bound a texture - no longer true
		// now that the frame is composed through an offscreen target, and a
		// fragile thing to depend on regardless. Uploading into the wrong
		// texture would silently cache a valid-looking atlas entry whose
		// pixels were never written, drawing as a blank rectangle.
		glBindTexture(GL_TEXTURE_2D, atlas);
		glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, GL_BGRA_EXT, GL_UNSIGNED_BYTE, bmp.data());
		check_gl("sprite glTexSubImage2D");
#else
		glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, GL_BGRA, GL_UNSIGNED_BYTE, bmp.data());
#endif

		// Temporary: every upload of a large sprite, with the id it was filed
		// under and where it went. If two different maps are filed under one
		// id, only the first is ever uploaded and the second is drawn as the
		// first - which would explain a correct rectangle full of the wrong
		// pixels.
		if (width > 200 && height > 200)
			LOG_ATLAS_PUT((unsigned long long)id, x, y, width, height);

		return offsets.emplace(
			std::piecewise_construct,
			std::forward_as_tuple(id),
			std::forward_as_tuple(x, y, width, height)
		).first->second;
	}

	void GraphicsGL::draw(const nl::bitmap& bmp, const Rectangle<int16_t>& rect, const Color& color, float angle)
	{
		if (locked)
			return;

		if (color.invisible())
			return;

		if (!rect.overlaps(SCREEN))
			return;

		const Offset& offset = getoffset(bmp);

		// No room in the atlas for this one. Queueing it anyway would draw
		// whatever happens to sit at the atlas origin - which is how a map
		// came to be drawn as a login video frame. Better to draw nothing for
		// the frame or two until the atlas is emptied and it fits.
		if (offset.right == offset.left || offset.bottom == offset.top)
			return;

		quads.emplace_back(rect.left(), rect.right(), rect.top(), rect.bottom(), offset, color, angle);
	}

	Text::Layout GraphicsGL::createlayout(const std::string& text, Text::Font id, Text::Alignment alignment, int16_t maxwidth, bool formatted, int16_t line_adj)
	{
		size_t length = text.length();

		if (length == 0)
			return Text::Layout();

		LayoutBuilder builder(fonts[id], alignment, maxwidth, formatted, line_adj);

		const char* p_text = text.c_str();

		size_t first = 0;
		size_t offset = 0;

		while (offset < length)
		{
			size_t last = text.find_first_of(" \\#", offset + 1);

			if (last == std::string::npos)
				last = length;

			first = builder.add(p_text, first, offset, last);
			offset = last;
		}

		return builder.finish(first, offset);
	}

	GraphicsGL::LayoutBuilder::LayoutBuilder(const Font& f, Text::Alignment a, int16_t mw, bool fm, int16_t la) : font(f), alignment(a), maxwidth(mw), formatted(fm), line_adj(la)
	{
		fontid = Text::Font::NUM_FONTS;
		color = Color::Name::NUM_COLORS;
		ax = 0;
		ay = font.linespace();
		width = 0;
		endy = 0;

		// A width of zero means "no limit". A NEGATIVE one means a caller
		// worked one out badly, and would otherwise be a line no character can
		// fit on.
		if (maxwidth <= 0)
			maxwidth = 800;
	}

	size_t GraphicsGL::LayoutBuilder::add(const char* text, size_t prev, size_t first, size_t last)
	{
		if (first == last)
			return prev;

		Text::Font last_font = fontid;
		Color::Name last_color = color;
		size_t skip = 0;
		bool linebreak = false;

		if (formatted)
		{
			switch (text[first])
			{
			case '\\':
				if (first + 1 < last)
				{
					switch (text[first + 1])
					{
					case 'n':
						linebreak = true;
						break;
					case 'r':
						linebreak = ax > 0;
						break;
					}

					skip++;
				}

				skip++;
				break;
			case '#':
				if (first + 1 < last)
				{
					switch (text[first + 1])
					{
					case 'k':
						color = Color::Name::DARKGREY;
						break;
					case 'b':
						color = Color::Name::BLUE;
						break;
					case 'r':
						color = Color::Name::RED;
						break;
					case 'c':
						color = Color::Name::ORANGE;
						break;
					}

					skip++;
				}

				skip++;
				break;
			}
		}

		int16_t wordwidth = 0;

		if (!linebreak)
		{
			for (size_t i = first; i < last; i++)
			{
				char c = text[i];
				wordwidth += font.glyph(c).ax;

				if (wordwidth > maxwidth)
				{
					if (last - first == 1)
					{
						return last;
					}
					else
					{
						// The word is too long for the line, so it is split and
						// each half laid out on its own.
						//
						// The split has to actually move. If the very FIRST
						// character is already wider than the whole line, i is
						// still first, and splitting there hands this call its
						// own arguments straight back - which recurses until
						// the stack runs out. Keeping at least one character on
						// this side guarantees both halves are shorter than the
						// word, so the recursion always ends.
						size_t at = i > first ? i : first + 1;

						prev = add(text, prev, first, at);
						return add(text, prev, at, last);
					}
				}
			}
		}

		bool newword = skip > 0;
		bool newline = linebreak || ax + wordwidth > maxwidth;

		if (newword || newline)
			add_word(prev, first, last_font, last_color);

		if (newline)
		{
			add_line();

			endy = ay;
			ax = 0;
			ay += font.linespace();

			if (lines.size() > 0)
				ay -= line_adj;
		}

		for (size_t pos = first; pos < last; pos++)
		{
			char c = text[pos];
			const Font::Char& ch = font.glyph(c);

			advances.push_back(ax);

			if (pos < first + skip || newline && c == ' ')
				continue;

			ax += ch.ax;

			if (width < ax)
				width = ax;
		}

		if (newword || newline)
			return first + skip;
		else
			return prev;
	}

	Text::Layout GraphicsGL::LayoutBuilder::finish(size_t first, size_t last)
	{
		add_word(first, last, fontid, color);
		add_line();

		advances.push_back(ax);

		return Text::Layout(lines, advances, width, ay, ax, endy);
	}

	void GraphicsGL::LayoutBuilder::add_word(size_t word_first, size_t word_last, Text::Font word_font, Color::Name word_color)
	{
		words.push_back({ word_first, word_last, word_font, word_color });
	}

	void GraphicsGL::LayoutBuilder::add_line()
	{
		int16_t line_x = 0;
		int16_t line_y = ay;

		switch (alignment)
		{
		case Text::Alignment::CENTER:
			line_x -= ax / 2;
			break;
		case Text::Alignment::RIGHT:
			line_x -= ax;
			break;
		}

		lines.push_back({ words, { line_x, line_y } });
		words.clear();
	}

	void GraphicsGL::drawtext(const DrawArgument& args, const Range<int16_t>& vertical, const std::string& text, const Text::Layout& layout, Text::Font id, Color::Name colorid, Text::Background background)
	{
		if (locked)
			return;

		const Color& color = args.get_color();

		if (text.empty() || color.invisible())
			return;

		const Font& font = fonts[id];

		GLshort x = args.getpos().x();
		GLshort y = args.getpos().y();
		GLshort w = layout.width();
		GLshort h = layout.height();
		GLshort minheight = vertical.first() > 0 ? vertical.first() : SCREEN.top();
		GLshort maxheight = vertical.second() > 0 ? vertical.second() : SCREEN.bottom();

		switch (background)
		{
		case Text::Background::NAMETAG:
			// If ever changing code in here confirm placements with map 10000
			for (const Text::Layout::Line& line : layout)
			{
				GLshort left = x + line.position.x() - 1;
				GLshort right = left + w + 3;
				GLshort top = y + line.position.y() - font.linespace() + 6;
				GLshort bottom = top + h - 2;
				Color ntcolor = Color(0.0f, 0.0f, 0.0f, 0.6f);

				quads.emplace_back(left, right, top, bottom, nulloffset, ntcolor, 0.0f);
				quads.emplace_back(left - 1, left, top + 1, bottom - 1, nulloffset, ntcolor, 0.0f);
				quads.emplace_back(right, right + 1, top + 1, bottom - 1, nulloffset, ntcolor, 0.0f);
			}

			break;
		}

		for (const Text::Layout::Line& line : layout)
		{
			Point<int16_t> position = line.position;

			for (const Text::Layout::Word& word : line.words)
			{
				GLshort ax = position.x() + layout.advance(word.first);
				GLshort ay = position.y();

				const GLfloat* wordcolor;

				if (word.color < Color::Name::NUM_COLORS)
					wordcolor = Color::colors[word.color];
				else
					wordcolor = Color::colors[colorid];

				Color abscolor = color * Color(wordcolor[0], wordcolor[1], wordcolor[2], 1.0f);

				for (size_t pos = word.first; pos < word.last; ++pos)
				{
					const char c = text[pos];
					const Font::Char& ch = font.glyph(c);

					GLshort char_x = x + ax + ch.bl;
					GLshort char_y = y + ay - ch.bt;
					GLshort char_width = ch.bw;
					GLshort char_height = ch.bh;
					GLshort char_bottom = char_y + char_height;

					Offset offset = ch.offset;

					if (char_bottom > maxheight)
					{
						GLshort bottom_adjust = char_bottom - maxheight;

						if (bottom_adjust < 10)
						{
							offset.bottom -= bottom_adjust;
							char_bottom -= bottom_adjust;
						}
						else
						{
							continue;
						}
					}

					if (char_y < minheight)
						continue;

					if (ax == 0 && c == ' ')
						continue;

					ax += ch.ax;

					if (char_width <= 0 || char_height <= 0)
						continue;

					quads.emplace_back(char_x, char_x + char_width, char_y, char_bottom, offset, abscolor, 0.0f);
				}
			}
		}
	}

	void GraphicsGL::drawrectangle(int16_t x, int16_t y, int16_t width, int16_t height, float red, float green, float blue, float alpha)
	{
		if (locked)
			return;

		quads.emplace_back(x, x + width, y, y + height, nulloffset, Color(red, green, blue, alpha), 0.0f);
	}

	void GraphicsGL::drawscreenfill(float red, float green, float blue, float alpha)
	{
		drawrectangle(0, 0, VWIDTH, VHEIGHT, red, green, blue, alpha);
	}

	void GraphicsGL::lock()
	{
		locked = true;
	}

	void GraphicsGL::unlock()
	{
		locked = false;
	}

	void GraphicsGL::flush(float opacity)
	{
		bool coverscene = opacity != 1.0f;

		if (coverscene)
		{
			float complement = 1.0f - opacity;
			Color color = Color(0.0f, 0.0f, 0.0f, complement);

			quads.emplace_back(SCREEN.left(), SCREEN.right(), SCREEN.top(), SCREEN.bottom(), nulloffset, color, 0.0f);
		}

		glClearColor(clear_red, clear_green, clear_blue, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		GLsizeiptr csize = quads.size() * sizeof(Quad);
		GLsizeiptr fsize = quads.size() * Quad::LENGTH;

		glEnableVertexAttribArray(attribute_coord);
		glEnableVertexAttribArray(attribute_color);
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, csize, quads.data(), GL_STREAM_DRAW);

#if defined(PLATFORM_ANDROID)
		// No GL_QUADS in GLES2; Quad emits 6 vertices as two triangles.
		glDrawArrays(GL_TRIANGLES, 0, fsize);
#else
		glDrawArrays(GL_QUADS, 0, fsize);
#endif

		glDisableVertexAttribArray(attribute_coord);
		glDisableVertexAttribArray(attribute_color);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		if (coverscene)
			quads.pop_back();
	}

	void GraphicsGL::forget(const nl::bitmap& bmp)
	{
		offsets.erase(bmp.id());
	}

	Point<int16_t> GraphicsGL::atlas_size_of(const nl::bitmap& bmp)
	{
		const Offset& o = getoffset(bmp);

		return Point<int16_t>(o.right - o.left, o.bottom - o.top);
	}

	void GraphicsGL::set_clearcolour(float red, float green, float blue)
	{
		clear_red = red;
		clear_green = green;
		clear_blue = blue;
	}

	void GraphicsGL::begin_screen(int16_t width, int16_t height)
	{
		quads.clear();

		SCREEN = Rectangle<int16_t>(0, width, 0, height);

		glUniform2f(uniform_screensize, width, height);
	}

	void GraphicsGL::clearscene()
	{
		if (!locked)
			quads.clear();

		// Empty the atlas here, at the start of a frame, while nothing is
		// queued to draw from it.
		//
		// Before, the only thing that ever emptied it was running out of room
		// part way through drawing - which threw away every cached position
		// while quads already queued that frame were still pointing at them,
		// so those sprites came out as blank rectangles. Entering a map made it
		// obvious, since that is when a lot of new artwork arrives at once and
		// fills the remaining space.
		clear();
	}
}