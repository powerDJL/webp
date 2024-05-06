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
#ifndef _WEBP_ANIM_H_
#define _WEBP_ANIM_H_

#define WEBP_HAVE_GIF 1

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <gif_lib.h>
#include <webp/encode.h>
#include <webp/mux_types.h>
#ifdef __cplusplus
extern "C" {
#endif

#if !defined(STDIN_FILENO)
#define STDIN_FILENO 0
#endif

void Gif2Webp(const char *in_file, const char *out_file,WebPConfig *config);

WebPData Gif2WebpData(const unsigned char* GifFileIn, size_t data_size, WebPConfig* config);

GifFileType* DGifOpenMemReader(const GifByteType* GifFileIn, int SizeOfFile ,int *Error);

#ifdef __cplusplus
}
#endif
#endif 
