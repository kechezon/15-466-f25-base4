#include <ft2build.h>
#include FT_FREETYPE_H

#include <hb.h>
#include <hb-ft.h>

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <hb.h>
#include <hb-ft.h>
#include <cassert>

#include <iostream>
// #include "hbshaper.h"
// #include "freetypelib.h"

#define FONT_SIZE 36
#define MARGIN (FONT_SIZE * 0.5)

// vector<gl::Mesh*> meshes;

//This file exists to check that programs that use freetype / harfbuzz link properly in this base code.
//You probably shouldn't be looking here to learn to use either library.

void my_draw_bitmap(FT_Bitmap *bitmap, int left, int top) {
	unsigned int rows = bitmap->rows;
	unsigned int columns = bitmap->width;

	char intensity[10] = " .-*#xOX@";

	assert(bitmap->pixel_mode == FT_PIXEL_MODE_GRAY);
	for (unsigned int r = 0; r < rows; r++) {
		for (unsigned int c = 0; c < columns; c++) {
			printf("%c", intensity[(bitmap->buffer)[r * bitmap->pitch + c] * 8 / 255]);
		}
		printf("\n");
	}
	printf("\n");
	return;
}

int main(int argc, char **argv) {

	/******************
	 * Freetype things
	 ******************/
	const char *fontfile;
	const char *text;

	if (argc < 3) {
		fprintf(stderr, "usage: freetype-text font-file.ttf text\n");
		exit (1);
	}

	fontfile = argv[1];
	text = argv[2];

	FT_Library library;
	FT_Face face;
	FT_Error ft_error;
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

	// HBShaper latinShaper(fontfile, &lib);
	// latinShaper.init();
	// HBText hbt = {
	// 	text,
	// 	"en",
	// 	HB_SCRIPT_LATIN,
	// 	HB_DIRECTION_LTR
	// };

	// latinShaper.addFeature(HBFeature::KerningOn);

	// gl::initGL(argc, argc);

	// for(auto mesh: latinShaper.drawText(hbt1, 20, 320)) {
    //     meshes.push_back(mesh);
    // }

	// gl::uploadMeshes(meshes);

	// gl::loop(meshes);
	// gl::finish();
	// gl::deleteMeshes(meshes);

	/* Create hb-ft font*/
	hb_font_t *hb_font;
	hb_font = hb_ft_font_create(face, NULL); // uses face to create hb font. second argument is a callback to call when font object is not needed (destructor)
	
	/* Create hb-buffer and populate*/
	hb_buffer_t *hb_buffer;
	hb_buffer = hb_buffer_create(); // takes no arguments
	// replaces invalid UTF-8 characters with hb_buffer's replacement codepoint
	// Arguments 3-5 are text length (-1 for null termination),
	// 						  offset (first char to be added to buffer),
	//						  and number of characters to add (-1 if null terminated)
	hb_buffer_add_utf8(hb_buffer, text, -1, 0, -1);
	hb_buffer_guess_segment_properties(hb_buffer); // sets unset buffer segment properties based on buffer's contents

	/* Shape the buffer! */
	hb_shape(hb_font, hb_buffer, NULL, 0); // arg 3 (features) controls features applied during shaping.
										   // arg 4 tells you length of features array
	
	/* Glyph info */
	unsigned int len = hb_buffer_get_length(hb_buffer);
	hb_glyph_info_t *info = hb_buffer_get_glyph_infos(hb_buffer, NULL); // arg 2 is pointer to an unsigned int to write length of output array to
																		// if you modify the buffer contents, the return pointer is invalidated!
																		// Making arg 2 null means I don't care about the length written to the hb_buffer
	hb_glyph_position_t *pos = hb_buffer_get_glyph_positions(hb_buffer, NULL); // returns array of positions. Same arg 2 return and buffer modification disclaimer
	
	/* Print as is */
	printf("Raw buffer contents:\n");
	for (unsigned int i = 0; i < len; i++) {
		hb_codepoint_t gid = info[i].codepoint; // glyph id
		unsigned int cluster = info[i].cluster; // index of character in the original text that corresponds to this glyph
		double x_advance = pos[i].x_advance / 64.; // line advances along x this much after drawing
		double y_advance = pos[i].y_advance / 64.; // line advances along y ^^
		double x_offset = pos[i].x_offset / 64.; // glyph moves along x this much before drawing
		double y_offset = pos[i].y_offset / 64.; // glyph moves along y ^^

		char glyphname[32];
		hb_font_get_glyph_name(hb_font, gid, glyphname, sizeof(glyphname)); // glyphname is output (written to by function). Fetches glyph id from font

		printf ("glyph='%s'	cluster=%d	advance=(%g,%g)	offset=(%g,%g)\n",
            glyphname, cluster, x_advance, y_advance, x_offset, y_offset);
	}

	printf ("Converted to absolute positions:\n");
	/* And converted to absolute positions. */
	{
		double current_x = 0;
		double current_y = 0;
		for (unsigned int i = 0; i < len; i++)
		{
			hb_codepoint_t gid   = info[i].codepoint;
			unsigned int cluster = info[i].cluster;
			double x_position = current_x + pos[i].x_offset / 64.;
			double y_position = current_y + pos[i].y_offset / 64.;

			char glyphname[32];
			hb_font_get_glyph_name (hb_font, gid, glyphname, sizeof (glyphname));

			printf ("glyph='%s'	cluster=%d	position=(%g,%g)\n",
				glyphname, cluster, x_position, y_position);

			current_x += pos[i].x_advance / 64.;
			current_y += pos[i].y_advance / 64.;
		}
	}

	FT_GlyphSlot slot = face->glyph; // shortcut to where we'll draw the glyph. FT_GlyphSlot is a pointer type

	int pen_x, pen_y;//, num_chars;
	pen_x = 300;
	pen_y = 200;
	// num_chars = 13;

	// // char text[num_chars] = "Hello world!";

	for (unsigned int n = 0; n < len; n++) {
		// Takes a face and UTF-32 unicode character code, and returns the face's corresponding glyph index
		// Returns charcode if no charmap is selected (0 corresponds to missing glyph)
		
		/*
		FT_UInt	glyph_index = FT_Get_Char_Indx(face, text[n]);
		FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);

		// convert to anti-aliased bitmap
		FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
		*/
		//...does the same thing as...
		FT_Load_Char(face, text[n], FT_LOAD_RENDER); // the new flag immediately converts to an anti-aliased bitmap
		
		my_draw_bitmap(&slot->bitmap, pen_x + slot->bitmap_left, pen_y - slot->bitmap_top);

		pen_x += slot->advance.x >> 6; // plus advance divided by 64.
		pen_y += slot->advance.y >> 6; // Slot advanced vector is in 1/64-pixel units, and thus must be truncated
	}

	// {
	// // 	int	glyph_index = FT_Get_Char_Indx(face, 1);

	// // 	FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT); // puts the glyph at glyph_index of the font's charmap in face->slot
	// // 	FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL); // reformats the glyph image in the slot using the flag (edits face->glyph->format)  
	// // 	// FT_Select_Charmap(face, some encoding); platform_id and encoding_id are found in each charmap in face->charmaps
	// // 	FT_Set_Transform(face, NULL, NULL); // you can also affine transform a glyph, if the font you pick is a vector font.
	// // 										// NULL applies identity transform and (0,0) translate, respectively
	// // 										// Vector coordinates are in units of 1/64 of a pixel

	// // 	// use face->glyph->bitmap to directly access a glyph,
	// // 	// and its position using face->glyph->bitmap_[left/top]
	// // 	// these are the distance from the current pen position to the left/top borders of the bitmap
	// }

	// // FT_GlyphSlot slot = face->glyph; // shortcut to where we'll draw the glyph. FT_GlyphSlot is a pointer type

	// // int pen_x, pen_y, n, num_chars;
	// // pen_x = 300;
	// // pen_y = 200;
	// // num_chars = 13;

	// // const char text[num_chars] = "Hello world!";

	// for (n = 0; n < num_chars; n++) {
	// 	// Takes a face and UTF-32 unicode character code, and returns the face's corresponding glyph index
	// 	// Returns charcode if no charmap is selected (0 corresponds to missing glyph)
		
	// 	/*
	// 	FT_UInt	glyph_index = FT_Get_Char_Indx(face, text[n]);
	// 	FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);

	// 	// convert to anti-aliased bitmap
	// 	FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
	// 	*/
	// 	//...does the same thing as...
	// 	FT_Load_Char(face, text[n], FT_LOAD_RENDER); // the new flag immediately converts to an anti-aliased bitmap

	// 	my_draw_bitmap(&slot->bitmap, pen_x + slot->bitmap_left, pen_y - slot->bitmap_top);

	// 	pen_x += slot->advance.x >> 6; // plus advance divided by 64.
	// 	pen_y += slot->advance.y >> 6; // Slot advanced vector is in 1/64-pixel units, and thus must be truncated
	// }

	hb_buffer_t *buf = hb_buffer_create();
	hb_buffer_destroy(buf);

	std::cout << "It worked?" << std::endl;
}
