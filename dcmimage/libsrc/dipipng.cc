/*
 *
 *  Copyright (C) 2001-2003, OFFIS
 *
 *  This software and supporting documentation were developed by
 *
 *    Kuratorium OFFIS e.V.
 *    Healthcare Information and Communication Systems
 *    Escherweg 2
 *    D-26121 Oldenburg, Germany
 *
 *  THIS SOFTWARE IS MADE AVAILABLE,  AS IS,  AND OFFIS MAKES NO  WARRANTY
 *  REGARDING  THE  SOFTWARE,  ITS  PERFORMANCE,  ITS  MERCHANTABILITY  OR
 *  FITNESS FOR ANY PARTICULAR USE, FREEDOM FROM ANY COMPUTER DISEASES  OR
 *  ITS CONFORMITY TO ANY SPECIFICATION. THE ENTIRE RISK AS TO QUALITY AND
 *  PERFORMANCE OF THE SOFTWARE IS WITH THE USER.
 *
 *  Module:  dcmimage
 *
 *  Author:  Alexander Haderer
 *
 *  Purpose: Implements PNG interface for plugable image formats
 *
 *  Last Update:      $Author: meichel $
 *  Update Date:      $Date: 2003-02-11 13:18:38 $
 *  Source File:      $Source: /export/gitmirror/dcmtk-git/../dcmtk-cvs/dcmtk/dcmimage/libsrc/dipipng.cc,v $
 *  CVS/RCS Revision: $Revision: 1.1 $
 *  Status:           $State: Exp $
 *
 *  CVS/RCS Log at end of file
 *
 */


#include "osconfig.h"

#ifdef WITH_LIBPNG

#include "dctypes.h"
#include "diimage.h"
#include "dipipng.h"
#include "dcuid.h"	/* for dcmtk version */

BEGIN_EXTERN_C
#include <png.h>
END_EXTERN_C


DiPNGPlugin::DiPNGPlugin()
: DiPluginFormat()
, interlaceType(E_pngInterlaceAdam7)
, metainfoType(E_pngFileMetainfo)
{
}


DiPNGPlugin::~DiPNGPlugin()
{
}


int DiPNGPlugin::write(
  DiImage *image,
  FILE *stream,
  const unsigned long frame) const
{
  volatile int result = 0;  // gcc -W requires volatile here because of longjmp
  if ((image != NULL) && (stream != NULL))
  {
    /* create bitmap with 8 bits per sample */
    void *data = image->getOutputData(frame, 8 /*bits*/, 0 /*planar*/);
    if (data != NULL)
    {
      OFBool isMono = (image->getInternalColorModel() == EPI_Monochrome1) || (image->getInternalColorModel() == EPI_Monochrome2);

      png_struct	*png_ptr = NULL;
      png_info	*info_ptr = NULL;
      png_byte	*pix_ptr = NULL;

      png_byte ** volatile row_ptr = NULL;
      volatile png_textp  text_ptr = NULL;
      png_time ptime;

      int width  = image->getColumns();
      int height = image->getRows();
      int 	color_type;
      int	bpp;		// bytesperpixel
      int	bit_depth = 8;

      int 	row;
      
      // create png write struct
      png_ptr = png_create_write_struct( PNG_LIBPNG_VER_STRING, 
					 NULL, NULL, NULL );
      if( png_ptr == NULL ) {
	return 0;
      }

      // create png info struct
      info_ptr = png_create_info_struct( png_ptr );
      if( info_ptr == NULL ) {
	png_destroy_write_struct( &png_ptr, (png_infopp) NULL );
	return 0;
      }
    
      // setjmp stuff for png lib
      if( setjmp(png_jmpbuf(png_ptr) ) ) {
	png_destroy_write_struct( &png_ptr, (png_infopp) NULL );
	if( row_ptr )  delete[] row_ptr;
	if( text_ptr ) delete[] text_ptr;
	return 0;
      }

      if( isMono ) {
	color_type = PNG_COLOR_TYPE_GRAY;
	bpp        = 1;
      }else {
	color_type = PNG_COLOR_TYPE_RGB;
	bpp        = 3;
      }

      int opt_interlace = 0;
      switch (interlaceType) {
	case E_pngInterlaceAdam7:
	  opt_interlace = PNG_INTERLACE_ADAM7;
	  break;
	case E_pngInterlaceNone:
	  opt_interlace = PNG_INTERLACE_NONE;
	  break;
      }

      // init png io structure
      png_init_io( png_ptr, stream );

      // set write mode
      png_set_IHDR( png_ptr, info_ptr, width, height, bit_depth, color_type,
		    opt_interlace, PNG_COMPRESSION_TYPE_BASE, 
		    PNG_FILTER_TYPE_BASE);

      // set text & time
      if( metainfoType == E_pngFileMetainfo ) {
	text_ptr = new png_text[3];
	if( text_ptr == NULL ) {
	  png_destroy_write_struct( &png_ptr, (png_infopp) NULL );
	  return result;
	}
	text_ptr[0].key  	= "Title";
	text_ptr[0].text 	= "Converted DICOM Image";
	text_ptr[0].compression = PNG_TEXT_COMPRESSION_NONE;
	text_ptr[1].key  	= "Software";
	text_ptr[1].text 	= "OFFIS DCMTK";
	text_ptr[1].compression = PNG_TEXT_COMPRESSION_NONE;
	text_ptr[2].key  	= "Version";
	text_ptr[2].text 	= OFFIS_DCMTK_VERSION;
	text_ptr[2].compression = PNG_TEXT_COMPRESSION_NONE;
#ifdef PNG_iTXt_SUPPORTED
	text_ptr[0].lang = NULL;
	text_ptr[1].lang = NULL;
	text_ptr[2].lang = NULL;
#endif
	png_set_text( png_ptr, info_ptr, text_ptr, 3 );

	png_convert_from_time_t( &ptime, time(NULL) );
	png_set_tIME( png_ptr, info_ptr, &ptime );
      }

      // write header
      png_write_info( png_ptr, info_ptr );
      row_ptr = new png_bytep[height];
      if( row_ptr == NULL ) {
	png_destroy_write_struct( &png_ptr, (png_infopp) NULL );
	if( text_ptr ) delete[] text_ptr;
	return result;
      }
      for( row=0, pix_ptr=(png_byte*)data;
	row<height; 
	row++, pix_ptr+=width*bpp ) 
      {
	row_ptr[row] = pix_ptr;
      }

      // write image
      png_write_image( png_ptr, row_ptr );

      // write additional chunks
      png_write_end( png_ptr, info_ptr );

      // finish
      png_destroy_write_struct( &png_ptr, (png_infopp) NULL );
      delete[] row_ptr;
      if( text_ptr ) delete[] text_ptr;
      result = 1;
    }
  }

  return result;
}


void DiPNGPlugin::setInterlaceType(DiPNGInterlace itype)
{
  interlaceType = itype;
}


void DiPNGPlugin::setMetainfoType(DiPNGMetainfo minfo)
{
  metainfoType = minfo;
}

OFString DiPNGPlugin::getLibraryVersionString()
{
    OFString versionStr = "LIBPNG, Version ";
    char cver[10];
    png_uint_32 ver = png_access_version_number();
    if( ver < 999999 ) {
	sprintf( cver, "%li.%li.%li", (ver/10000)%100, (ver/100)%100, ver%100 );
    }else{
	sprintf( cver, "unknown" );
    }
    versionStr.append( cver );
    return versionStr;
}

#else /* WITH_LIBPNG */

int dipipng_cc_dummy_to_keep_linker_from_moaning = 0;

#endif

/*
 * CVS/RCS Log:
 * $Log: dipipng.cc,v $
 * Revision 1.1  2003-02-11 13:18:38  meichel
 * Added PNG export option to dcm2pnm and dcmj2pnm
 *
 *
 */