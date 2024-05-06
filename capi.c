// Copyright 2014 <chaishushan{AT}gmail.com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// +build cgo

#include "internal/src/webp.c"
#include "internal/src/gifTowebp.c"
#include "internal/src/imageio_util.c"
#include "internal/src/gifdec.c"

#include "internal/include/giflib-5.2.2/dgif_lib.c"
#include "internal/include/giflib-5.2.2/gif_err.c"
#include "internal/include/giflib-5.2.2/gifalloc.c"
#include "internal/include/giflib-5.2.2/openbsd-reallocarray.c"



