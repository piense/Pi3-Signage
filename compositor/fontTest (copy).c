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

uint32_t *renderTextTest(uint32_t width, uint32_t height) {

  cairo_surface_t* surf = NULL;
  cairo_t* cr = NULL;
  cairo_status_t status;
  PangoContext* context = NULL;
  PangoLayout* layout = NULL;
  PangoFontDescription* font_desc = NULL;
  PangoFontMap* font_map = NULL;
  FT_Bitmap bmp = {0};

  int stride = 0;

  /* ------------------------------------------------------------ */
  /*                   I N I T I A L I Z E                        */
  /* ------------------------------------------------------------ */

  /* FT buffer */
  FT_Bitmap_New(&bmp);
  bmp.rows = height;
  bmp.width = width;

  bmp.buffer = (unsigned char*)malloc(bmp.rows * bmp.width*4);
  if (NULL == bmp.buffer) {
    printf("+ error: cannot allocate the buffer for the output bitmap.\n");
    exit(EXIT_FAILURE);
  }

  /* create our "canvas" */
  bmp.pitch = width*4;
  bmp.pixel_mode = FT_PIXEL_MODE_BGRA; /*< Grayscale*/

  //bmp.num_grays = 256;
  stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
  surf = cairo_image_surface_create_for_data(bmp.buffer, CAIRO_FORMAT_ARGB32, width, height, stride);

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
  layout = pango_layout_new(context);
  if (NULL == layout) {
    printf("+ error: cannot create the pango layout.\n");
    exit(EXIT_FAILURE);
  }


  /* create the font description @todo the reference does not tell how/when to free this */
  font_desc = pango_font_description_from_string("Vera 40");
  pango_layout_set_font_description(layout, font_desc);
  pango_font_map_load_font(font_map, context, font_desc);
  pango_font_description_free(font_desc);

  /* set the width around which pango will wrap */
  pango_layout_set_width(layout, 300 * PANGO_SCALE);

  /* write using the markup feature */
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

  pango_layout_set_markup(layout, text, -1);

  pango_cairo_context_set_font_options


  /* render */
  pango_ft2_render_layout(&bmp, layout, 30, 100);
  pango_cairo_update_layout(cr, layout);

  /* ------------------------------------------------------------ */
  /*               O U T P U T  A N D  C L E A N U P              */
  /* ------------------------------------------------------------ */

  uint32_t *ret = malloc(width*height*4);

  cairo_surface_flush(surf);
  uint32_t* data = (uint32_t *)cairo_image_surface_get_data(surf);

  printf("%d\n",cairo_image_surface_get_format(surf));

  uint32_t x,y;
  for(x = 0;x<width;x++)
	  for(y = 0;y<height;y++)
		  ret[x+y*width] = ((uint32_t*)bmp.buffer)[x+y*width];
		  //ret[x+y*width] = (0xFF<<24) | bmp.buffer[x+y*width] << 16 | bmp.buffer[x+y*width] << 8 | bmp.buffer[x+y*width];



  cairo_surface_destroy(surf);
  cairo_destroy(cr);

  g_object_unref(layout);
  g_object_unref(font_map);
  g_object_unref(context);

  return ret;

  return 0;
}
