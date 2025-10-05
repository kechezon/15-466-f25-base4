#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"
#include "ColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <hb.h>
#include <hb-ft.h>

#include <random>

/****************
 * Render things
 ****************/
#define FONT_SIZE 36
#define MARGIN (FONT_SIZE * 0.5)

// harfbuzz and freetype get data from glyphs
const char *fontfile = &("dist/HammersmithOne-Regular.ttf"[0]); // idk how to convert from std::string to char*, so I didn't use datapath
const char *text = &("Hello World!"[0]);

FT_Library library;
FT_Face face;
FT_Error ft_error;

// if ((ft_error = FT_Init_FreeType( &library )))
// 	abort();
// // if ((ft_error = FT_New_Face(library, "dist/HammersmithOne-Regular.ttf", 0, &face)))
// // Loads a font face (descriptor of a given typeface and style)
// // Now to access that face data with a face object (models information that globally describes a face)
// if ((ft_error = FT_New_Face(library, fontfile, 0, &face)))
// 	abort();
// // face->size lets you get pixel size. This size object models all information needed w.r.t. character sizes
// // Use FT_Set_Char_Size, passing a handle to the face object you have.
// // Width and height are expressed in 1/64th of a point (0 means same as the other). You can also see the pixel size.
// if ((ft_error = FT_Set_Char_Size(face, FONT_SIZE*64, FONT_SIZE*64, 0, 0)))
// 	abort();

/* Create hb-ft font*/
hb_font_t *hb_font;

/* Create hb-buffer and populate*/
hb_buffer_t *hb_buffer;

/* Glyph info */
unsigned int hb_buffer_len;
hb_glyph_info_t *info;
hb_glyph_position_t *pos; // returns array of positions. Same arg 2 return and buffer modification disclaimer

GLuint hexapod_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > hexapod_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("hexapod.pnct"));
	hexapod_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > hexapod_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("hexapod.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = hexapod_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = hexapod_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

Load< Sound::Sample > dusty_floor_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("dusty-floor.opus"));
});


Load< Sound::Sample > honk_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("honk.wav"));
});


PlayMode::PlayMode() : scene(*hexapod_scene) {
	//get pointers to leg for convenience:
	for (auto &transform : scene.transforms) {
		if (transform.name == "Hip.FL") hip = &transform;
		else if (transform.name == "UpperLeg.FL") upper_leg = &transform;
		else if (transform.name == "LowerLeg.FL") lower_leg = &transform;
	}
	if (hip == nullptr) throw std::runtime_error("Hip not found.");
	if (upper_leg == nullptr) throw std::runtime_error("Upper leg not found.");
	if (lower_leg == nullptr) throw std::runtime_error("Lower leg not found.");

	hip_base_rotation = hip->rotation;
	upper_leg_base_rotation = upper_leg->rotation;
	lower_leg_base_rotation = lower_leg->rotation;

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();

	//start music loop playing:
	// (note: position will be over-ridden in update())
	leg_tip_loop = Sound::loop_3D(*dusty_floor_sample, 1.0f, get_leg_tip_position(), 10.0f);

	// TEXT RENDERING ATTEMPT
	/*
	 * The rest of the code in this scope comes from the following freetype tutorial:
	 * https://freetype.org/freetype2/docs/tutorial/step1.html
	 */
	// harfbuzz and freetype get data from glyphs
	if ((ft_error = FT_Init_FreeType( &library )))
		abort();
	// if ((ft_error = FT_New_Face(library, "dist/HammersmithOne-Regular.ttf", 0, &face)))
	// Loads a font face (descriptor of a given typeface and style)
	// Now to access that face data with a face object (models information that globally describes a face)
	if ((ft_error = FT_New_Face(library, fontfile, 0, &face)))
		abort();
	// face->size lets you get pixel size. This size object models all information needed w.r.t. character sizes
	// Use FT_Set_Char_Size, passing a handle to the face object you have.
	// Width and height are expressed in 1/64th of a point (0 means same as the other). You can also see the pixel size.
	if ((ft_error = FT_Set_Char_Size(face, FONT_SIZE*64, FONT_SIZE*64, 0, 0)))
		abort();

	/* Create hb-ft font*/
	hb_font = hb_ft_font_create(face, NULL); // uses face to create hb font. second argument is a callback to call when font object is not needed (destructor)

	/* Create hb-buffer and populate*/
	hb_buffer = hb_buffer_create(); // takes no arguments
	// replaces invalid UTF-8 characters with hb_buffer's replacement codepoint
	hb_buffer_add_utf8(hb_buffer, text, -1, 0, -1);
	hb_buffer_guess_segment_properties(hb_buffer); // sets unset buffer segment properties based on buffer's contents

	/* Shape the buffer! */
	hb_shape(hb_font, hb_buffer, NULL, 0); // arg 3 (features) controls features applied during shaping.
											// arg 4 tells you length of features array

	/* Glyph info */
	// hb_buffer_len = hb_buffer_get_length(hb_buffer);
	info = hb_buffer_get_glyph_infos(hb_buffer, &hb_buffer_len); // arg 2 is pointer to an unsigned int to write length of output array to
																		// if you modify the buffer contents, the return pointer is invalidated!
																		// Making arg 2 null means I don't care about the length written to the hb_buffer
	pos = hb_buffer_get_glyph_positions(hb_buffer, NULL); // returns array of positions. Same arg 2 return and buffer modification disclaimer
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_EVENT_KEY_DOWN) {
		if (evt.key.key == SDLK_ESCAPE) {
			SDL_SetWindowRelativeMouseMode(Mode::window, false);
			return true;
		} else if (evt.key.key == SDLK_A) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_D) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_W) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_S) {
			down.downs += 1;
			down.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_SPACE) {
			if (honk_oneshot) honk_oneshot->stop();
			honk_oneshot = Sound::play_3D(*honk_sample, 0.3f, glm::vec3(4.6f, -7.8f, 6.9f)); //hardcoded position of front of car, from blender
		}
	} else if (evt.type == SDL_EVENT_KEY_UP) {
		if (evt.key.key == SDLK_A) {
			left.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_D) {
			right.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_W) {
			up.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_S) {
			down.pressed = false;
			return true;
		}
	} else if (evt.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
		if (SDL_GetWindowRelativeMouseMode(Mode::window) == false) {
			SDL_SetWindowRelativeMouseMode(Mode::window, true);
			return true;
		}
	} else if (evt.type == SDL_EVENT_MOUSE_MOTION) {
		if (SDL_GetWindowRelativeMouseMode(Mode::window) == true) {
			glm::vec2 motion = glm::vec2(
				evt.motion.xrel / float(window_size.y),
				-evt.motion.yrel / float(window_size.y)
			);
			camera->transform->rotation = glm::normalize(
				camera->transform->rotation
				* glm::angleAxis(-motion.x * camera->fovy, glm::vec3(0.0f, 1.0f, 0.0f))
				* glm::angleAxis(motion.y * camera->fovy, glm::vec3(1.0f, 0.0f, 0.0f))
			);
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {

	//slowly rotates through [0,1):
	wobble += elapsed / 10.0f;
	wobble -= std::floor(wobble);

	hip->rotation = hip_base_rotation * glm::angleAxis(
		glm::radians(5.0f * std::sin(wobble * 2.0f * float(M_PI))),
		glm::vec3(0.0f, 1.0f, 0.0f)
	);
	upper_leg->rotation = upper_leg_base_rotation * glm::angleAxis(
		glm::radians(7.0f * std::sin(wobble * 2.0f * 2.0f * float(M_PI))),
		glm::vec3(0.0f, 0.0f, 1.0f)
	);
	lower_leg->rotation = lower_leg_base_rotation * glm::angleAxis(
		glm::radians(10.0f * std::sin(wobble * 3.0f * 2.0f * float(M_PI))),
		glm::vec3(0.0f, 0.0f, 1.0f)
	);

	//move sound to follow leg tip position:
	leg_tip_loop->set_position(get_leg_tip_position(), 1.0f / 60.0f);

	//move camera:
	{

		//combine inputs into a move:
		constexpr float PlayerSpeed = 30.0f;
		glm::vec2 move = glm::vec2(0.0f);
		if (left.pressed && !right.pressed) move.x =-1.0f;
		if (!left.pressed && right.pressed) move.x = 1.0f;
		if (down.pressed && !up.pressed) move.y =-1.0f;
		if (!down.pressed && up.pressed) move.y = 1.0f;

		//make it so that moving diagonally doesn't go faster:
		if (move != glm::vec2(0.0f)) move = glm::normalize(move) * PlayerSpeed * elapsed;

		glm::mat4x3 frame = camera->transform->make_parent_from_local();
		glm::vec3 frame_right = frame[0];
		//glm::vec3 up = frame[1];
		glm::vec3 frame_forward = -frame[2];

		camera->transform->position += move.x * frame_right + move.y * frame_forward;
	}

	{ //update listener to camera position:
		glm::mat4x3 frame = camera->transform->make_parent_from_local();
		glm::vec3 frame_right = frame[0];
		glm::vec3 frame_at = frame[3];
		Sound::listener.set_position_right(frame_at, frame_right, 1.0f / 60.0f);
	}

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*camera);

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.09f;
		lines.draw_text("Mouse motion rotates camera; WASD moves; escape ungrabs mouse",
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("Mouse motion rotates camera; WASD moves; escape ungrabs mouse",
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}
	GL_ERRORS();

	
		//---------------  create and upload texture data ----------------
		// TEXT RENDERING ATTEMPT
		/******************************************************
		 * Based on circle drawing code provided by Jim McCann
		 ******************************************************/
		//texture size:
		unsigned int width = 0;
		unsigned int height = 0;
		//pixel data for texture:
		FT_GlyphSlot slot = face->glyph; // shortcut to where we'll draw the glyph. FT_GlyphSlot is a pointer type

		for (unsigned int i = 0; i < hb_buffer_len; i++) { // get GLsizei length (assume it's on one line)
			FT_UInt	glyph_index = FT_Get_Char_Index(face, text[i]);
			FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
			FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);

			printf("%c: (%i + %i), (%i + %i)\n", text[i], pos[i].x_offset, pos[i].x_advance, pos[i].y_offset, pos[i].y_advance);

			width += (GLsizei)(pos[i].x_offset + pos[i].x_advance); // right now im basically trying to draw the letters next to each other
			height = (GLsizei)(std::max(height, slot->bitmap.rows)); //pos[i].y_offset and y_advance are 0...
			// std::cout << "(" << pos[n].x_advance << ", " << pos[n].y_advance << ")" << std::endl;
		}
		std::vector< uint8_t > data(width*height, 0);
		printf("Vector of size %i by %i\n", width, height);

		// DEBUG
		char intensity[10] = " .-*#xOX@";

		// GLsizei current_x = 0;
		// GLsizei current_y = 0;
		// for (unsigned int n = 0; n < hb_buffer_len; n++) {
		// 	FT_Load_Char(face, text[n], FT_LOAD_RENDER); // the new flag immediately converts to an anti-aliased bitmap
		// 	// // std::cout << "(" << current_x << ", " << current_y << ")" << std::endl;
		// 	GLsizei maxY = current_y + (GLsizei)(slot->bitmap.rows);
		// 	for (GLsizei y = current_y; y < maxY; ++y) {
		// 		for (GLsizei x = current_x; x < current_x + (GLsizei)(slot->bitmap.pitch); ++x) {
		// 			uint8_t c = (slot->bitmap.buffer)[y * slot->bitmap.pitch + x];
		// 			// DEBUG:
		// 			printf("%c", intensity[c * 8 / 255]);
		// 			// draw_bitmap_char(&(slot->bitmap), data, current_x, current_y); // second attempt, but nothing gets rendered
		// 			data[(maxY - 1 - (y - current_y)) * width + x] = c;
		// 		}
		// 		// DEBUG:
		// 		printf("\n");
		// 	}
		// 	current_x += (int)slot->bitmap.pitch;
		// 	current_y += (int)pos[n].y_advance;
		// }


		unsigned int cursor_x = 0;
		unsigned int cursor_y = 0;
		for (unsigned int i = 0; i < hb_buffer_len; i++) {
			FT_UInt	glyph_index = FT_Get_Char_Index(face, text[i]);
			FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
			FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);

			// loop things
			for (unsigned int y = 0; y < slot->bitmap.rows; y++) {
				unsigned int glyph_width = pos[i].x_offset + pos[i].x_advance;
				for (unsigned int x = 0; x < glyph_width; x++) {
					uint8_t c = (slot->bitmap.buffer)[y * glyph_width + x]; // retrieve value
					printf("%c", intensity[((unsigned int) c) * 8 / 255]);
					data[(cursor_y + y) * width + (cursor_x + x)] = c; // place in data to be drawn
				}
				printf("\n");
			}

			printf("\n\n");

			cursor_x += slot->bitmap.width;
		}

		//need a name for the texture object:
		GLuint tex = 0;
		glGenTextures(1, &tex); // store 1 texture name in tex
		//attach texture object to the GL_TEXTURE_2D binding point:
		glBindTexture(GL_TEXTURE_2D, tex);
		//upload data: (see: https://registry.khronos.org/OpenGL-Refpages/gl4/html/glTexImage2D.xhtml )
		glTexImage2D(
			GL_TEXTURE_2D, //target -- the binding point this call is uploading to
			0, //level -- the mip level; 0 = the base level
			GL_R8,  //internalformat -- channels and storage used on the GPU for the texture; GL_R8 means one channel, 8-bit fixed point
			width, //width of texture
			height, //height of texture
			0, //border -- must be 0
			GL_RED, //format -- how the data to be uploaded is structured; GL_RED means one channel
			GL_UNSIGNED_BYTE, //type -- how each element of a pixel is stored
			data.data() //data -- pointer to the texture data
		);
		//set up texture sampling state:
		//clamp texture coordinate to edge of texture: (GL_REPEAT can also be useful in some cases)
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		//use linear interpolation to magnify:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		//use trilinear interpolation (linear interpolation of linearly-interpolated mipmaps) to minify:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

		//ask OpenGL to make the mipmaps for us:
		glGenerateMipmap(GL_TEXTURE_2D);

		//de-attach texture:
		glBindTexture(GL_TEXTURE_2D, 0);

		//----------- set up place to store mesh that references the data -----------

		//format for the mesh data:
		struct Vertex {
			glm::vec2 Position;
			glm::u8vec4 Color;
			glm::vec2 TexCoord;
		};

		//create a buffer object to store mesh data in:
		GLuint buffer = 0;
		glGenBuffers(1, &buffer);
		glBindBuffer(GL_ARRAY_BUFFER, buffer); //(buffer created when bound)
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		//create a vertex array object that references the buffer:
		GLuint buffer_for_color_texture_program = 0;
		glGenVertexArrays(1, &buffer_for_color_texture_program);
		glBindVertexArray(buffer_for_color_texture_program);

		//configure the vertex array object:

		glBindBuffer(GL_ARRAY_BUFFER, buffer); //will take data from 'buffer'

		//set up Position to read from the buffer:
		//see https://registry.khronos.org/OpenGL-Refpages/gl4/html/glVertexAttribPointer.xhtml
		glVertexAttribPointer(
			color_texture_program->Position_vec4, //attribute
			2, //size
			GL_FLOAT, //type
			GL_FALSE, //normalized
			sizeof(Vertex), //stride
			(GLbyte *)0 + offsetof(Vertex, Position) //offset
		);
		glEnableVertexAttribArray(color_texture_program->Position_vec4);

		//set up Color to read from the buffer:
		glVertexAttribPointer( color_texture_program->Color_vec4, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (GLbyte *)0 + offsetof(Vertex, Color));
		glEnableVertexAttribArray(color_texture_program->Color_vec4);

		//set up TexCoord to read from the buffer:
		glVertexAttribPointer( color_texture_program->TexCoord_vec2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLbyte *)0 + offsetof(Vertex, TexCoord));
		glEnableVertexAttribArray(color_texture_program->TexCoord_vec2);

		//done configuring vertex array, so unbind things:
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);


		//----------- create and upload a mesh that references the data -----------

		std::vector< Vertex > attribs;
		attribs.reserve(4);

		//if drawn as a triangle strip, this will be a square with the lower-left corner at (0,0) and the upper right at (1,1):
		attribs.emplace_back(Vertex{
			.Position = glm::vec2(-0.9f, -1.0f),
			.Color = glm::u8vec4(0xff, 0xff, 0xff, 0xff),
			.TexCoord = glm::vec2(0.0f, 0.0f),
		});
			attribs.emplace_back(Vertex{
			.Position = glm::vec2(-0.9f, -0.25f),
			.Color = glm::u8vec4(0xff, 0xff, 0xff, 0xff),
			.TexCoord = glm::vec2(0.0f, 1.0f),
		});
		attribs.emplace_back(Vertex{
			.Position = glm::vec2(0.9f, -1.0f),
			.Color = glm::u8vec4(0xff, 0xff, 0xff, 0xff),
			.TexCoord = glm::vec2(1.0f, 0.0f),
		});
		attribs.emplace_back(Vertex{
			.Position = glm::vec2(0.9f, -0.25f),
			.Color = glm::u8vec4(0xff, 0xff, 0xff, 0xff),
			.TexCoord = glm::vec2(1.0f, 1.0f),
		});

		//upload attribs to buffer:
		glBindBuffer(GL_ARRAY_BUFFER, buffer);
		glBufferData(GL_ARRAY_BUFFER, attribs.size() * sizeof(attribs[0]), attribs.data(), GL_STREAM_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		/******************************************
		 * /end Jim's provided text rendering code
		 ******************************************/


		//----------- draw the mesh -----------

		//draw using the color_texture_program:
		glUseProgram(color_texture_program->program);
		//draw with attributes from our buffer, as referenced by the vertex array:
		glBindVertexArray(buffer_for_color_texture_program);
		//draw using texture stored in tex:
		glBindTexture(GL_TEXTURE_2D, tex);
		
		//this particular shader program multiplies all positions by this matrix: (hmm, old naming style; I should have fixed that)
		// (just setting it to the identity, so Positions are directly in clip space)
		glUniformMatrix4fv(color_texture_program->OBJECT_TO_CLIP_mat4, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));

		//draw without depth testing (so will draw atop everything else):
		glDisable(GL_DEPTH_TEST);
		//draw with alpha blending (so transparent parts of the texture look transparent):
		glEnable(GL_BLEND);
		//standard 'over' blending:
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		//actually draw:
		glDrawArrays(GL_TRIANGLE_STRIP, 0, (GLsizei)attribs.size());


		//turn off blending:
		glDisable(GL_BLEND);
		//...leave depth test off, since code that wants it will turn it back on


		//unbind texture, vertex array, program:
		glBindTexture(GL_TEXTURE_2D, 0);
		glBindVertexArray(0);
		glUseProgram(0);

		//----------- free allocated buffers / data -----------

		glDeleteVertexArrays(1, &buffer_for_color_texture_program);
		buffer_for_color_texture_program = 0;
		glDeleteBuffers(1, &buffer);
		buffer = 0;
		glDeleteTextures(1, &tex);
		tex = 0;
}

glm::vec3 PlayMode::get_leg_tip_position() {
	//the vertex position here was read from the model in blender:
	return lower_leg->make_world_from_local() * glm::vec4(-1.26137f, -11.861f, 0.0f, 1.0f);
}
