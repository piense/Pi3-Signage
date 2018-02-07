#include "fontTest.h"

#include <cairo.h>
#include <freetype/ftbitmap.h>
#include <pango/pangocairo.h>
#include <pango/pangoft2.h>
#include <pango/pango-coverage.h>

#include <stdio.h>
#include <stdlib.h>

#include <stdint.h>

#define USE_FREETYPE 1
#define USE_RGBA 0

void printFontList()
{
    int i;
    PangoFontFamily ** families;
    int n_families;
    PangoFontMap * fontmap;

    PangoFontFace ** faces;
    int nFaces,a;

    fontmap = pango_cairo_font_map_get_default();
    pango_font_map_list_families (fontmap, & families, & n_families);
    printf ("There are %d families\n", n_families);
    for (i = 0; i < n_families; i++) {
        PangoFontFamily * family = families[i];
        const char * family_name;

        family_name = pango_font_family_get_name (family);
        pango_font_family_list_faces(family,&faces,&nFaces);
        for(a=0;a<nFaces;a++){
        	printf("\tFace: %s\n", pango_font_description_to_string(pango_font_face_describe(faces[a])) );

        }
        printf ("Family %d: %s\n", i, family_name);
    }
    g_free (families);
}

void renderTextToScreen(uint32_t *screenBuf,
		uint32_t screenWidth, uint32_t screenHeight,
		uint32_t xPos, uint32_t yPos,
		uint32_t fontSize, char* string,
		uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha
		)
		{

  cairo_surface_t* surf = NULL;
  cairo_t* cr = NULL;
  PangoContext* context = NULL;
  PangoLayout* layout = NULL;
  PangoFontDescription* font_desc = NULL;
  PangoFontMap* font_map = NULL;


  /* ------------------------------------------------------------ */
  /*                   I N I T I A L I Z E                        */
  /* ------------------------------------------------------------ */


  surf = cairo_image_surface_create_for_data((uint8_t*)screenBuf,
		  CAIRO_FORMAT_ARGB32, screenWidth, screenHeight, screenWidth * 4);

  if (CAIRO_STATUS_SUCCESS != cairo_surface_status(surf)) {
    printf("+ error: couldn't create the surface.\n");
    exit(EXIT_FAILURE);
  }

  /* create our cairo context object that tracks state. */
  cr = cairo_create(surf);
  if (CAIRO_STATUS_NO_MEMORY == cairo_status(cr)) {
    printf("+ error: out of memory, cannot create cairo_t*\n");
    exit(EXIT_FAILURE);
  }

  /* ------------------------------------------------------------ */
  /*               D R A W   I N T O  C A N V A S                 */
  /* ------------------------------------------------------------ */

  font_map = pango_ft2_font_map_new();
  if (NULL == font_map) {
    printf("+ error: cannot create the pango font map.\n");
    exit(EXIT_FAILURE);
  }

  context = pango_font_map_create_context(font_map);
  if (NULL == context) {
    printf("+ error: cannot create pango font context.\n");
    exit(EXIT_FAILURE);
  }

  /* create layout object. */
  layout = pango_cairo_create_layout(cr);
  if (NULL == layout) {
    printf("+ error: cannot create the pango layout.\n");
    exit(EXIT_FAILURE);
  }

  /* create the font description @todo the reference does not tell how/when to free this */
  font_desc = pango_font_description_from_string("Vera");
  pango_font_description_set_size(font_desc, fontSize*PANGO_SCALE);
  pango_layout_set_font_description(layout, font_desc);
  pango_font_map_load_font(font_map, context, font_desc);
  pango_font_description_free(font_desc);

  /* set the width around which pango will wrap */
  //pango_layout_set_width(layout, 300 * PANGO_SCALE);

  /*
  // write using the markup feature
  const gchar* text = ""
    "<span foreground=\"blue\" font_family=\"Vera\">"
    "   <b> bold </b>"
    "   <u> is </u>"
    "   <i> nice </i>"
    "</span>"
    "<tt> hello </tt>"
    "<span font_family=\"sans\" font_stretch=\"ultracondensed\" letter_spacing=\"500\" font_weight=\"light\"> SANS</span>"
    "<span foreground=\"#FFCC00\"> colored</span>"
    "";
   */

  pango_layout_set_markup(layout, string, -1);

  pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);

  cairo_set_source_rgba(cr, ((float)red)/255.0,
		  ((float)green)/255.0, ((float)blue)/255.0, ((float)alpha)/255.0);

  PangoRectangle rect;
  pango_layout_get_pixel_extents (layout, NULL, &rect);

  cairo_move_to(cr, xPos - rect.width/2,yPos - rect.height/2);

  pango_cairo_update_layout(cr, layout);
  pango_cairo_show_layout(cr, layout);


  cairo_surface_destroy(surf);
  cairo_destroy(cr);

  g_object_unref(layout);
  g_object_unref(font_map);
  g_object_unref(context);

  return;
}
