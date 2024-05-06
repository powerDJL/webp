// Copyright 2012 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
//  simple tool to convert animated GIFs to WebP
//
// Authors: Skal (pascal.massimino@gmail.com)
//          Urvang (urvang@google.com)

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "webp/config.h"
#endif

// #ifdef WEBP_HAVE_GIF

#if defined(HAVE_UNISTD_H) && HAVE_UNISTD_H
#include <unistd.h>
#endif
#include "gifTowebp.h"
#include <gif_lib.h>
#include <gif_lib_private.h>
#include "webp/encode.h"
#include "webp/mux.h"
#include <webp/mux_types.h>
#include "imageio_util.h"
#include "gifdec.h"
#include "unicode.h"
#include "unicode_gif.h"

#if !defined(STDIN_FILENO)
#define STDIN_FILENO 0
#endif

//------------------------------------------------------------------------------

static int transparent_index = GIF_INDEX_INVALID; // Opaque by default.

static const char *const kErrorMessages[-WEBP_MUX_NOT_ENOUGH_DATA + 1] = {
    "WEBP_MUX_NOT_FOUND", "WEBP_MUX_INVALID_ARGUMENT", "WEBP_MUX_BAD_DATA",
    "WEBP_MUX_MEMORY_ERROR", "WEBP_MUX_NOT_ENOUGH_DATA"};

static const char *ErrorString(WebPMuxError err)
{
  assert(err <= WEBP_MUX_NOT_FOUND && err >= WEBP_MUX_NOT_ENOUGH_DATA);
  return kErrorMessages[-err];
}

enum
{
  METADATA_ICC = (1 << 0),
  METADATA_XMP = (1 << 1),
  METADATA_ALL = METADATA_ICC | METADATA_XMP
};

/******************************************************************************
 input GIF name, output webp name
 create webp file
******************************************************************************/

void Gif2Webp(const char *in_file, const char *out_file, WebPConfig *config)
{
  int verbose = 0;
  int gif_error = GIF_ERROR;
  WebPMuxError err = WEBP_MUX_OK;
  int ok = 0;
  // const W_CHAR *in_file = NULL, *out_file = NULL;
  GifFileType *gif = NULL;
  int frame_duration = 0;
  int frame_timestamp = 0;
  GIFDisposeMethod orig_dispose = GIF_DISPOSE_NONE;

  WebPPicture frame;       // Frame rectangle only (not disposed).
  WebPPicture curr_canvas; // Not disposed.
  WebPPicture prev_canvas; // Disposed.

  WebPAnimEncoder *enc = NULL;
  WebPAnimEncoderOptions enc_options;
  // WebPConfig config;

  int frame_number = 0; // Whether we are processing the first frame.
  int done;
  int c;
  int quiet = 0;
  WebPData webp_data;

  int keep_metadata = METADATA_XMP; // ICC not output by default.
  WebPData icc_data;
  int stored_icc = 0; // Whether we have already stored an ICC profile.
  WebPData xmp_data;
  int stored_xmp = 0;        // Whether we have already stored an XMP profile.
  int loop_count = 0;        // default: infinite
  int stored_loop_count = 0; // Whether we have found an explicit loop count.
  int loop_compatibility = 0;
  WebPMux *mux = NULL;

  int default_kmin = 1; // Whether to use default kmin value.
  int default_kmax = 1;

  INIT_WARGV(argc, argv);

  if (!WebPConfigInit(config) || !WebPAnimEncoderOptionsInit(&enc_options) ||
      !WebPPictureInit(&frame) || !WebPPictureInit(&curr_canvas) ||
      !WebPPictureInit(&prev_canvas))
  {
    fprintf(stderr, "Error! Version mismatch!\n");
    // FREE_WARGV_AND_RETURN(EXIT_FAILURE);
  }
  // config->lossless = 1;  Use lossless compression by default.

  WebPDataInit(&webp_data);
  WebPDataInit(&icc_data);
  WebPDataInit(&xmp_data);

  if (!WebPValidateConfig(config))
  {
    printf("Error! Invalid configuration.\n");
    goto End;
  }

  if (in_file == NULL)
  {
    printf("No input file specified!\n");
    // Help();
    goto End;
  }
  printf("DGifOpenFileUnicode:%s\n", in_file);
  // Start the decoder object
  gif = DGifOpenFileName(in_file, &gif_error);
  if (gif == NULL)
  {
    goto End;
  }
  gif_error = GIF_OK;
  // Loop over GIF images
  done = 0;
  verbose = 1;
  printf("done %d\n", done);
  do
  {
    GifRecordType type;
    if (DGifGetRecordType(gif, &type) == GIF_ERROR)
    {
      goto End;
    }

    switch (type)
    {
    case IMAGE_DESC_RECORD_TYPE:
    {
      GIFFrameRect gif_rect;
      GifImageDesc *const image_desc = &gif->Image;

      if (!DGifGetImageDesc(gif))
      {
        goto End;
      }
      if (frame_number == 0)
      {
        if (verbose)
        {
          printf("Canvas screen: %d x %d\n", gif->SWidth, gif->SHeight);
        }
        // Fix some broken GIF global headers that report
        // 0 x 0 screen dimension.
        if (gif->SWidth == 0 || gif->SHeight == 0)
        {
          image_desc->Left = 0;
          image_desc->Top = 0;
          gif->SWidth = image_desc->Width;
          gif->SHeight = image_desc->Height;
          if (gif->SWidth <= 0 || gif->SHeight <= 0)
          {
            goto End;
          }
          if (verbose)
          {
            printf("Fixed canvas screen dimension to: %d x %d\n",
                   gif->SWidth, gif->SHeight);
          }
        }
        // Allocate current buffer.
        frame.width = gif->SWidth;
        frame.height = gif->SHeight;
        frame.use_argb = 1;
        if (!WebPPictureAlloc(&frame))
        {
          goto End;
        }
        GIFClearPic(&frame, NULL);
        if (!(WebPPictureCopy(&frame, &curr_canvas) &&
              WebPPictureCopy(&frame, &prev_canvas)))
        {
          fprintf(stderr, "Error allocating canvas.\n");
          goto End;
        }

        // Background color.
        GIFGetBackgroundColor(gif->SColorMap, gif->SBackGroundColor,
                              transparent_index,
                              &enc_options.anim_params.bgcolor);

        // Initialize encoder.
        enc = WebPAnimEncoderNew(curr_canvas.width, curr_canvas.height,
                                 &enc_options);
        if (enc == NULL)
        {
          fprintf(stderr,
                  "Error! Could not create encoder object. Possibly due to "
                  "a memory error.\n");
          goto End;
        }
      }

      // Some even more broken GIF can have sub-rect with zero width/height.
      if (image_desc->Width == 0 || image_desc->Height == 0)
      {
        image_desc->Width = gif->SWidth;
        image_desc->Height = gif->SHeight;
      }

      if (!GIFReadFrame(gif, transparent_index, &gif_rect, &frame))
      {
        goto End;
      }
      // Blend frame rectangle with previous canvas to compose full canvas.
      // Note that 'curr_canvas' is same as 'prev_canvas' at this point.
      GIFBlendFrames(&frame, &gif_rect, &curr_canvas);

      if (!WebPAnimEncoderAdd(enc, &curr_canvas, frame_timestamp, config))
      {
        fprintf(stderr, "Error while adding frame #%d: %s\n", frame_number,
                WebPAnimEncoderGetError(enc));
        goto End;
      }
      else
      {
        ++frame_number;
      }

      // Update canvases.
      GIFDisposeFrame(orig_dispose, &gif_rect, &prev_canvas, &curr_canvas);
      GIFCopyPixels(&curr_canvas, &prev_canvas);

      // Force frames with a small or no duration to 100ms to be consistent
      // with web browsers and other transcoding tools. This also avoids
      // incorrect durations between frames when padding frames are
      // discarded.
      if (frame_duration <= 10)
      {
        frame_duration = 100;
      }

      // Update timestamp (for next frame).
      frame_timestamp += frame_duration;

      // In GIF, graphic control extensions are optional for a frame, so we
      // may not get one before reading the next frame. To handle this case,
      // we reset frame properties to reasonable defaults for the next frame.
      orig_dispose = GIF_DISPOSE_NONE;
      frame_duration = 0;
      transparent_index = GIF_INDEX_INVALID;
      break;
    }
    case EXTENSION_RECORD_TYPE:
    {
      int extension;
      GifByteType *data = NULL;
      if (DGifGetExtension(gif, &extension, &data) == GIF_ERROR)
      {
        goto End;
      }
      if (data == NULL)
        continue;

      switch (extension)
      {
      case COMMENT_EXT_FUNC_CODE:
      {
        break; // Do nothing for now.
      }
      case GRAPHICS_EXT_FUNC_CODE:
      {
        if (!GIFReadGraphicsExtension(data, &frame_duration, &orig_dispose,
                                      &transparent_index))
        {
          goto End;
        }
        break;
      }
      case PLAINTEXT_EXT_FUNC_CODE:
      {
        break;
      }
      case APPLICATION_EXT_FUNC_CODE:
      {
        if (data[0] != 11)
          break; // Chunk is too short
        if (!memcmp(data + 1, "NETSCAPE2.0", 11) ||
            !memcmp(data + 1, "ANIMEXTS1.0", 11))
        {
          if (!GIFReadLoopCount(gif, &data, &loop_count))
          {
            goto End;
          }
          if (verbose)
          {
            fprintf(stderr, "Loop count: %d\n", loop_count);
          }
          stored_loop_count = loop_compatibility ? (loop_count != 0) : 1;
        }
        else
        { // An extension containing metadata.
          // We only store the first encountered chunk of each type, and
          // only if requested by the user.
          const int is_xmp = (keep_metadata & METADATA_XMP) &&
                             !stored_xmp &&
                             !memcmp(data + 1, "XMP DataXMP", 11);
          const int is_icc = (keep_metadata & METADATA_ICC) &&
                             !stored_icc &&
                             !memcmp(data + 1, "ICCRGBG1012", 11);
          if (is_xmp || is_icc)
          {
            if (!GIFReadMetadata(gif, &data,
                                 is_xmp ? &xmp_data : &icc_data))
            {
              goto End;
            }
            if (is_icc)
            {
              stored_icc = 1;
            }
            else if (is_xmp)
            {
              stored_xmp = 1;
            }
          }
        }
        break;
      }
      default:
      {
        break; // skip
      }
      }
      while (data != NULL)
      {
        if (DGifGetExtensionNext(gif, &data) == GIF_ERROR)
        {
          printf("DGifGetExtensionNext gotoEND\n");
          goto End;
        }
      }
      break;
    }
    case TERMINATE_RECORD_TYPE:
    {
      done = 1;
      break;
    }
    default:
    {
      if (verbose)
      {
        printf("Skipping over unknown record type %d\n", type);
      }
      break;
    }
    }
  } while (!done);

  // Last NULL frame.
  if (!WebPAnimEncoderAdd(enc, NULL, frame_timestamp, NULL))
  {
    fprintf(stderr, "Error flushing WebP muxer.\n");
    fprintf(stderr, "%s\n", WebPAnimEncoderGetError(enc));
  }

  if (!WebPAnimEncoderAssemble(enc, &webp_data))
  {
    fprintf(stderr, "%s\n", WebPAnimEncoderGetError(enc));
    goto End;
  }
  // If there's only one frame, we don't need to handle loop count.
  if (frame_number == 1)
  {
    loop_count = 0;
  }
  else if (!loop_compatibility)
  {
    if (!stored_loop_count)
    {
      // if no loop-count element is seen, the default is '1' (loop-once)
      // and we need to signal it explicitly in WebP. Note however that
      // in case there's a single frame, we still don't need to store it.
      if (frame_number > 1)
      {
        stored_loop_count = 1;
        loop_count = 1;
      }
    }
    else if (loop_count > 0 && loop_count < 65535)
    {
      // adapt GIF's semantic to WebP's (except in the infinite-loop case)
      loop_count += 1;
    }
  }
  // loop_count of 0 is the default (infinite), so no need to signal it
  if (loop_count == 0)
    stored_loop_count = 0;

  if (stored_loop_count || stored_icc || stored_xmp)
  {
    // Re-mux to add loop count and/or metadata as needed.
    mux = WebPMuxCreate(&webp_data, 1);
    if (mux == NULL)
    {
      fprintf(stderr, "ERROR: Could not re-mux to add loop count/metadata.\n");
      goto End;
    }
    WebPDataClear(&webp_data);

    if (stored_loop_count)
    { // Update loop count.
      WebPMuxAnimParams new_params;
      err = WebPMuxGetAnimationParams(mux, &new_params);
      if (err != WEBP_MUX_OK)
      {
        fprintf(stderr, "ERROR (%s): Could not fetch loop count.\n",
                ErrorString(err));
        goto End;
      }
      new_params.loop_count = loop_count;
      err = WebPMuxSetAnimationParams(mux, &new_params);
      if (err != WEBP_MUX_OK)
      {
        fprintf(stderr, "ERROR (%s): Could not update loop count.\n",
                ErrorString(err));
        goto End;
      }
    }

    if (stored_icc)
    { // Add ICCP chunk.
      err = WebPMuxSetChunk(mux, "ICCP", &icc_data, 1);
      if (verbose)
      {
        fprintf(stderr, "ICC size: %d\n", (int)icc_data.size);
      }
      if (err != WEBP_MUX_OK)
      {
        fprintf(stderr, "ERROR (%s): Could not set ICC chunk.\n",
                ErrorString(err));
        goto End;
      }
    }

    if (stored_xmp)
    { // Add XMP chunk.
      err = WebPMuxSetChunk(mux, "XMP ", &xmp_data, 1);
      if (verbose)
      {
        fprintf(stderr, "XMP size: %d\n", (int)xmp_data.size);
      }
      if (err != WEBP_MUX_OK)
      {
        fprintf(stderr, "ERROR (%s): Could not set XMP chunk.\n",
                ErrorString(err));
        goto End;
      }
    }

    err = WebPMuxAssemble(mux, &webp_data);
    if (err != WEBP_MUX_OK)
    {
      fprintf(stderr, "ERROR (%s): Could not assemble when re-muxing to add "
                      "loop count/metadata.\n",
              ErrorString(err));
      goto End;
    }
  }

  if (out_file != NULL)
  {
    if (!ImgIoUtilWriteFile((const char *)out_file, webp_data.bytes,
                            webp_data.size))
    {
      WFPRINTF(stderr, "Error writing output file: %s\n", out_file);
      printf("ImgIoUtilWriteFile gotoEND\n");
      goto End;
    }
    if (!quiet)
    {
      if (!WSTRCMP(out_file, "-"))
      {
        fprintf(stderr, "Saved %d bytes to STDIO\n",
                (int)webp_data.size);
      }
      else
      {
        WFPRINTF(stderr, "Saved output file (%d bytes): %s\n",
                 (int)webp_data.size, out_file);
      }
    }
  }
  else
  {
    if (!quiet)
    {
      fprintf(stderr, "Nothing written; use -o flag to save the result "
                      "(%d bytes).\n",
              (int)webp_data.size);
    }
  }

  // All OK.
  ok = 1;
  gif_error = GIF_OK;

End:
  printf("Ende done %d \n",done);

  WebPDataClear(&icc_data);
  WebPDataClear(&xmp_data);
  WebPMuxDelete(mux);
  WebPDataClear(&webp_data);
  WebPPictureFree(&frame);
  WebPPictureFree(&curr_canvas);
  WebPPictureFree(&prev_canvas);
  WebPAnimEncoderDelete(enc);

  if (gif_error != GIF_OK)
  {
    GIFDisplayError(gif, gif_error);
  }
  if (gif != NULL)
  {
#if LOCAL_GIF_PREREQ(5, 1)
    DGifCloseFile(gif, &gif_error);
#else
    DGifCloseFile(gif);
#endif
  }
}

/******************************************************************************
 input GIF data,
 Returns webpdata
******************************************************************************/

WebPData Gif2WebpData(const unsigned char *gifFileIn, size_t data_size, WebPConfig *config)
{
  // Returns EXIT_SUCCESS on success, EXIT_FAILURE on failure.
  // int main(int argc, const char* argv[]) {
  int verbose = 0;
  int gif_error = GIF_ERROR;
  WebPMuxError err = WEBP_MUX_OK;
  int ok = 0;
  // const W_CHAR *in_file = NULL, *out_file = NULL;
  GifFileType *gif = NULL;

  int frame_duration = 0;
  int frame_timestamp = 0;
  GIFDisposeMethod orig_dispose = GIF_DISPOSE_NONE;

  WebPPicture frame;       // Frame rectangle only (not disposed).
  WebPPicture curr_canvas; // Not disposed.
  WebPPicture prev_canvas; // Disposed.

  WebPAnimEncoder *enc = NULL;
  WebPAnimEncoderOptions enc_options;
  // WebPConfig config;

  int frame_number = 0; // Whether we are processing the first frame.
  int done;
  int c;
  int quiet = 0;
  WebPData webp_data;

  int keep_metadata = METADATA_XMP; // ICC not output by default.
  WebPData icc_data;
  int stored_icc = 0; // Whether we have already stored an ICC profile.
  WebPData xmp_data;
  int stored_xmp = 0;        // Whether we have already stored an XMP profile.
  int loop_count = 0;        // default: infinite
  int stored_loop_count = 0; // Whether we have found an explicit loop count.
  int loop_compatibility = 0;
  WebPMux *mux = NULL;

  int default_kmin = 9; // Whether to use default kmin value.
  int default_kmax = 17;

  if (config->lossless == 0)
    {
      default_kmin = 3;
      default_kmax = 5;
    }

  // INIT_WARGV(argc, argv);

  if (!WebPConfigInit(config) || !WebPAnimEncoderOptionsInit(&enc_options) ||
      !WebPPictureInit(&frame) || !WebPPictureInit(&curr_canvas) ||
      !WebPPictureInit(&prev_canvas))
  {
    fprintf(stderr, "Error! Version mismatch!\n");
    // FREE_WARGV_AND_RETURN(EXIT_FAILURE);
  }
  // config->lossless = 1; // Use lossless compression by default.

  WebPDataInit(&webp_data);
  WebPDataInit(&icc_data);
  WebPDataInit(&xmp_data);

  if (!WebPValidateConfig(config))
  {
    printf("Error! Invalid configuration.\n");
    goto End;
  }

  if (gifFileIn == NULL)
  {
    printf("No input file specified!\n");
    // Help();
    goto End;
  }
  printf("DGifOpenFileDataSize:%zu\n", data_size);

  // Start the decoder object
  gif = DGifOpenMemReader(gifFileIn, data_size, &gif_error);

  if (gif == NULL)
  {
    goto End;
  }
  // Loop over GIF images
  done = 0;
  verbose = 1;
  printf("done %d\n", done);
  do
  {
    GifRecordType type;
    if (DGifGetRecordType(gif, &type) == GIF_ERROR)
    {
      goto End;
    }

    switch (type)
    {
    case IMAGE_DESC_RECORD_TYPE:
    {
      GIFFrameRect gif_rect;
      GifImageDesc *const image_desc = &gif->Image;

      if (!DGifGetImageDesc(gif))
      {
        goto End;
      }
      if (frame_number == 0)
      {
        if (verbose)
        {
          printf("Canvas screen: %d x %d\n", gif->SWidth, gif->SHeight);
        }
        // Fix some broken GIF global headers that report
        // 0 x 0 screen dimension.
        if (gif->SWidth == 0 || gif->SHeight == 0)
        {
          image_desc->Left = 0;
          image_desc->Top = 0;
          gif->SWidth = image_desc->Width;
          gif->SHeight = image_desc->Height;
          if (gif->SWidth <= 0 || gif->SHeight <= 0)
          {
            goto End;
          }
          if (verbose)
          {
            printf("Fixed canvas screen dimension to: %d x %d\n",
                   gif->SWidth, gif->SHeight);
          }
        }
        // Allocate current buffer.
        frame.width = gif->SWidth;
        frame.height = gif->SHeight;
        frame.use_argb = 1;
        if (!WebPPictureAlloc(&frame))
        {
          goto End;
        }
        GIFClearPic(&frame, NULL);
        if (!(WebPPictureCopy(&frame, &curr_canvas) &&
              WebPPictureCopy(&frame, &prev_canvas)))
        {
          fprintf(stderr, "Error allocating canvas.\n");
          goto End;
        }

        // Background color.
        GIFGetBackgroundColor(gif->SColorMap, gif->SBackGroundColor,
                              transparent_index,
                              &enc_options.anim_params.bgcolor);

        // Initialize encoder.
        enc = WebPAnimEncoderNew(curr_canvas.width, curr_canvas.height,
                                 &enc_options);
        if (enc == NULL)
        {
          fprintf(stderr,
                  "Error! Could not create encoder object. Possibly due to "
                  "a memory error.\n");
          goto End;
        }
      }

      // Some even more broken GIF can have sub-rect with zero width/height.
      if (image_desc->Width == 0 || image_desc->Height == 0)
      {
        image_desc->Width = gif->SWidth;
        image_desc->Height = gif->SHeight;
      }

      if (!GIFReadFrame(gif, transparent_index, &gif_rect, &frame))
      {
        goto End;
      }
      // Blend frame rectangle with previous canvas to compose full canvas.
      // Note that 'curr_canvas' is same as 'prev_canvas' at this point.
      GIFBlendFrames(&frame, &gif_rect, &curr_canvas);

      if (!WebPAnimEncoderAdd(enc, &curr_canvas, frame_timestamp, config))
      {
        fprintf(stderr, "Error while adding frame #%d: %s\n", frame_number,
                WebPAnimEncoderGetError(enc));
        goto End;
      }
      else
      {
        ++frame_number;
      }

      // Update canvases.
      GIFDisposeFrame(orig_dispose, &gif_rect, &prev_canvas, &curr_canvas);
      GIFCopyPixels(&curr_canvas, &prev_canvas);

      // Force frames with a small or no duration to 100ms to be consistent
      // with web browsers and other transcoding tools. This also avoids
      // incorrect durations between frames when padding frames are
      // discarded.
      if (frame_duration <= 10)
      {
        frame_duration = 100;
      }

      // Update timestamp (for next frame).
      frame_timestamp += frame_duration;

      // In GIF, graphic control extensions are optional for a frame, so we
      // may not get one before reading the next frame. To handle this case,
      // we reset frame properties to reasonable defaults for the next frame.
      orig_dispose = GIF_DISPOSE_NONE;
      frame_duration = 0;
      transparent_index = GIF_INDEX_INVALID;
      break;
    }
    case EXTENSION_RECORD_TYPE:
    {
      int extension;
      GifByteType *data = NULL;
      if (DGifGetExtension(gif, &extension, &data) == GIF_ERROR)
      {
        goto End;
      }
      if (data == NULL)
        continue;

      switch (extension)
      {
      case COMMENT_EXT_FUNC_CODE:
      {
        break; // Do nothing for now.
      }
      case GRAPHICS_EXT_FUNC_CODE:
      {
        if (!GIFReadGraphicsExtension(data, &frame_duration, &orig_dispose,
                                      &transparent_index))
        {
          goto End;
        }
        break;
      }
      case PLAINTEXT_EXT_FUNC_CODE:
      {
        break;
      }
      case APPLICATION_EXT_FUNC_CODE:
      {
        if (data[0] != 11)
          break; // Chunk is too short
        if (!memcmp(data + 1, "NETSCAPE2.0", 11) ||
            !memcmp(data + 1, "ANIMEXTS1.0", 11))
        {
          if (!GIFReadLoopCount(gif, &data, &loop_count))
          {
            goto End;
          }
          if (verbose)
          {
            fprintf(stderr, "Loop count: %d\n", loop_count);
          }
          stored_loop_count = loop_compatibility ? (loop_count != 0) : 1;
        }
        else
        { // An extension containing metadata.
          // We only store the first encountered chunk of each type, and
          // only if requested by the user.
          const int is_xmp = (keep_metadata & METADATA_XMP) &&
                             !stored_xmp &&
                             !memcmp(data + 1, "XMP DataXMP", 11);
          const int is_icc = (keep_metadata & METADATA_ICC) &&
                             !stored_icc &&
                             !memcmp(data + 1, "ICCRGBG1012", 11);
          if (is_xmp || is_icc)
          {
            if (!GIFReadMetadata(gif, &data,
                                 is_xmp ? &xmp_data : &icc_data))
            {
              goto End;
            }
            if (is_icc)
            {
              stored_icc = 1;
            }
            else if (is_xmp)
            {
              stored_xmp = 1;
            }
          }
        }
        break;
      }
      default:
      {
        break; // skip
      }
      }
      while (data != NULL)
      {
        if (DGifGetExtensionNext(gif, &data) == GIF_ERROR)
        {
          printf("DGifGetExtensionNext gotoEND\n");
          goto End;
        }
      }
      break;
    }
    case TERMINATE_RECORD_TYPE:
    {
      done = 1;
      break;
    }
    default:
    {
      if (verbose)
      {
        printf("Skipping over unknown record type %d\n", type);
      }
      break;
    }
    }
  } while (!done);

  // Last NULL frame.
  if (!WebPAnimEncoderAdd(enc, NULL, frame_timestamp, NULL))
  {
    fprintf(stderr, "Error flushing WebP muxer.\n");
    fprintf(stderr, "%s\n", WebPAnimEncoderGetError(enc));
  }

  if (!WebPAnimEncoderAssemble(enc, &webp_data))
  {
    fprintf(stderr, "%s\n", WebPAnimEncoderGetError(enc));
    goto End;
  }
  // If there's only one frame, we don't need to handle loop count.
  if (frame_number == 1)
  {
    loop_count = 0;
  }
  else if (!loop_compatibility)
  {
    if (!stored_loop_count)
    {
      // if no loop-count element is seen, the default is '1' (loop-once)
      // and we need to signal it explicitly in WebP. Note however that
      // in case there's a single frame, we still don't need to store it.
      if (frame_number > 1)
      {
        stored_loop_count = 1;
        loop_count = 1;
      }
    }
    else if (loop_count > 0 && loop_count < 65535)
    {
      // adapt GIF's semantic to WebP's (except in the infinite-loop case)
      loop_count += 1;
    }
  }
  // loop_count of 0 is the default (infinite), so no need to signal it
  if (loop_count == 0)
    stored_loop_count = 0;

  if (stored_loop_count || stored_icc || stored_xmp)
  {
    // Re-mux to add loop count and/or metadata as needed.
    mux = WebPMuxCreate(&webp_data, 1);
    if (mux == NULL)
    {
      fprintf(stderr, "ERROR: Could not re-mux to add loop count/metadata.\n");
      goto End;
    }
    WebPDataClear(&webp_data);

    if (stored_loop_count)
    { // Update loop count.
      WebPMuxAnimParams new_params;
      err = WebPMuxGetAnimationParams(mux, &new_params);
      if (err != WEBP_MUX_OK)
      {
        fprintf(stderr, "ERROR (%s): Could not fetch loop count.\n",
                ErrorString(err));
        goto End;
      }
      new_params.loop_count = loop_count;
      err = WebPMuxSetAnimationParams(mux, &new_params);
      if (err != WEBP_MUX_OK)
      {
        fprintf(stderr, "ERROR (%s): Could not update loop count.\n",
                ErrorString(err));
        goto End;
      }
    }

    if (stored_icc)
    { // Add ICCP chunk.
      err = WebPMuxSetChunk(mux, "ICCP", &icc_data, 1);
      if (verbose)
      {
        fprintf(stderr, "ICC size: %d\n", (int)icc_data.size);
      }
      if (err != WEBP_MUX_OK)
      {
        fprintf(stderr, "ERROR (%s): Could not set ICC chunk.\n",
                ErrorString(err));
        goto End;
      }
    }

    if (stored_xmp)
    { // Add XMP chunk.
      err = WebPMuxSetChunk(mux, "XMP ", &xmp_data, 1);
      if (verbose)
      {
        fprintf(stderr, "XMP size: %d\n", (int)xmp_data.size);
      }
      if (err != WEBP_MUX_OK)
      {
        fprintf(stderr, "ERROR (%s): Could not set XMP chunk.\n",
                ErrorString(err));
        goto End;
      }
    }

    err = WebPMuxAssemble(mux, &webp_data);
    if (err != WEBP_MUX_OK)
    {
      fprintf(stderr, "ERROR (%s): Could not assemble when re-muxing to add "
                      "loop count/metadata.\n",
              ErrorString(err));
      goto End;
    }
  }

  // All OK.
  ok = 1;
  gif_error = GIF_OK;

End:
  printf("Ende done %d \n",done);

  WebPDataClear(&icc_data);
  WebPDataClear(&xmp_data);
  WebPMuxDelete(mux);
  WebPPictureFree(&frame);
  WebPPictureFree(&curr_canvas);
  WebPPictureFree(&prev_canvas);
  WebPAnimEncoderDelete(enc);

  if (gif_error != GIF_OK)
  {
    GIFDisplayError(gif, gif_error);
  }
  if (gif != NULL)
  {
#if LOCAL_GIF_PREREQ(5, 1)
    DGifCloseFile(gif, &gif_error);
#else
    DGifCloseFile(gif);
#endif
  }

  return webp_data;
}

typedef struct {
    const unsigned char *data;
    int pos;
    int dataSize;
} MemReaderData;
int MemRead(GifFileType *gif, GifByteType *buf, int len) {
    if (gif == NULL || buf == NULL) {
        return 0;  // 返回0表示出错
    }

    MemReaderData *memReaderData = (MemReaderData *)gif->UserData;
    if (memReaderData == NULL || memReaderData->data == NULL) {
        return 0;  // 返回0表示出错
    }

    // 检查是否超出数据范围
    if (memReaderData->pos + len > memReaderData->dataSize) {
        return 0;  // 返回0表示出错
    }

    memcpy(buf, memReaderData->data + memReaderData->pos, len);
    memReaderData->pos += len;

    return len;  // 成功读取的字节数
}

GifFileType *DGifOpenMemReader(const unsigned char *gifData, int dataSize, int *Error) {
    if (gifData == NULL || dataSize <= 0) {
        *Error = D_GIF_ERR_OPEN_FAILED;
        return NULL;  // 如果输入数据为空或者数据大小非正，则返回NULL
    }

    MemReaderData *memReaderData = (MemReaderData *)malloc(sizeof(MemReaderData));
    if (memReaderData == NULL) {
       *Error = D_GIF_ERR_NOT_ENOUGH_MEM;
        return NULL;  // 如果内存分配失败，则返回NULL
    }

    memReaderData->data = gifData;
    memReaderData->pos = 0;
    memReaderData->dataSize = dataSize;

    GifFileType *gifFile = DGifOpen(memReaderData, MemRead, Error);
    if (!gifFile) {
        free(memReaderData);  // 如果打开GIF文件失败，则释放内存并返回NULL
        return NULL;
    }
    *Error = GIF_OK;
    return gifFile;
}
