// Copyright 2014 <chaishushan{AT}gmail.com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

//
// cgo pointer:
//
// Go1.3: Changes to the garbage collector
// http://golang.org/doc/go1.3#garbage_collector
//
// Go1.6:
// https://github.com/golang/proposal/blob/master/design/12416-cgo-pointers.md
//

package webp

/*
#cgo CFLAGS: -I./internal/libwebp-1.4.0/
#cgo CFLAGS: -I./internal/libwebp-1.4.0/src/
#cgo CFLAGS: -I./internal/libwebp-1.4.0/sharpyuv/
#cgo CFLAGS: -I./internal/include/
#cgo CFLAGS: -I./internal/include/giflib-5.2.2/

#cgo CFLAGS: -Wno-pointer-sign -DWEBP_USE_THREAD
#cgo !windows LDFLAGS: -lm

#include "webp.h"
#include "gifTowebp.h"

#include <webp/decode.h>
#include <webp/encode.h>

#include <webp/mux.h>
#include <stdlib.h>
#include <string.h>
*/
import "C"
import (
	"errors"
	"unsafe"
)

func webpGetInfo(data []byte) (width, height int, hasAlpha bool, err error) {
	if len(data) == 0 {
		err = errors.New("webpGetInfo: bad arguments, data is empty")
		return
	}
	if len(data) > maxWebpHeaderSize {
		data = data[:maxWebpHeaderSize]
	}

	var features C.WebPBitstreamFeatures
	if C.WebPGetFeatures((*C.uint8_t)(unsafe.Pointer(&data[0])), C.size_t(len(data)), &features) != C.VP8_STATUS_OK {
		err = errors.New("C.WebPGetFeatures: failed")
		return
	}
	width, height = int(features.width), int(features.height)
	hasAlpha = (features.has_alpha != 0)
	return
}

func webpDecodeGray(data []byte) (pix []byte, width, height int, err error) {
	if len(data) == 0 {
		err = errors.New("webpDecodeGray: bad arguments")
		return
	}

	var cw, ch C.int
	var cptr = C.webpDecodeGray((*C.uint8_t)(unsafe.Pointer(&data[0])), C.size_t(len(data)), &cw, &ch)
	if cptr == nil {
		err = errors.New("webpDecodeGray: failed")
		return
	}
	defer C.free(unsafe.Pointer(cptr))

	pix = make([]byte, int(cw*ch*1))
	copy(pix, ((*[1 << 30]byte)(unsafe.Pointer(cptr)))[0:len(pix):len(pix)])
	width, height = int(cw), int(ch)
	return
}

func webpDecodeRGB(data []byte) (pix []byte, width, height int, err error) {
	if len(data) == 0 {
		err = errors.New("webpDecodeRGB: bad arguments")
		return
	}

	var cw, ch C.int
	var cptr = C.webpDecodeRGB((*C.uint8_t)(unsafe.Pointer(&data[0])), C.size_t(len(data)), &cw, &ch)
	if cptr == nil {
		err = errors.New("webpDecodeRGB: failed")
		return
	}
	defer C.free(unsafe.Pointer(cptr))

	pix = make([]byte, int(cw*ch*3))
	copy(pix, ((*[1 << 30]byte)(unsafe.Pointer(cptr)))[0:len(pix):len(pix)])
	width, height = int(cw), int(ch)
	return
}

func webpDecodeRGBA(data []byte) (pix []byte, width, height int, err error) {
	if len(data) == 0 {
		err = errors.New("webpDecodeRGBA: bad arguments")
		return
	}

	var cw, ch C.int
	var cptr = C.webpDecodeRGBA((*C.uint8_t)(unsafe.Pointer(&data[0])), C.size_t(len(data)), &cw, &ch)
	if cptr == nil {
		err = errors.New("webpDecodeRGBA: failed")
		return
	}
	defer C.free(unsafe.Pointer(cptr))

	pix = make([]byte, int(cw*ch*4))
	copy(pix, ((*[1 << 30]byte)(unsafe.Pointer(cptr)))[0:len(pix):len(pix)])
	width, height = int(cw), int(ch)
	return
}

func webpDecodeGrayToSize(data []byte, width, height int) (pix []byte, err error) {
	pix = make([]byte, int(width*height))
	stride := C.int(width)
	res := C.webpDecodeGrayToSize((*C.uint8_t)(unsafe.Pointer(&data[0])), C.size_t(len(data)), C.int(width), C.int(height), stride, (*C.uint8_t)(unsafe.Pointer(&pix[0])))
	if res != C.VP8_STATUS_OK {
		pix = nil
		err = errors.New("webpDecodeGrayToSize: failed")
	}
	return
}

func webpDecodeRGBToSize(data []byte, width, height int) (pix []byte, err error) {
	pix = make([]byte, int(3*width*height))
	stride := C.int(3 * width)
	res := C.webpDecodeRGBToSize((*C.uint8_t)(unsafe.Pointer(&data[0])), C.size_t(len(data)), C.int(width), C.int(height), stride, (*C.uint8_t)(unsafe.Pointer(&pix[0])))
	if res != C.VP8_STATUS_OK {
		pix = nil
		err = errors.New("webpDecodeRGBToSize: failed")
	}
	return
}

func webpDecodeRGBAToSize(data []byte, width, height int) (pix []byte, err error) {
	pix = make([]byte, int(4*width*height))
	stride := C.int(4 * width)
	res := C.webpDecodeRGBAToSize((*C.uint8_t)(unsafe.Pointer(&data[0])), C.size_t(len(data)), C.int(width), C.int(height), stride, (*C.uint8_t)(unsafe.Pointer(&pix[0])))
	if res != C.VP8_STATUS_OK {
		pix = nil
		err = errors.New("webpDecodeRGBAToSize: failed")
	}
	return
}

func webpEncodeGray(pix []byte, width, height, stride int, quality float32) (output []byte, err error) {
	if len(pix) == 0 || width <= 0 || height <= 0 || stride <= 0 || quality < 0.0 {
		err = errors.New("webpEncodeGray: bad arguments")
		return
	}
	if stride < width*1 && len(pix) < height*stride {
		err = errors.New("webpEncodeGray: bad arguments")
		return
	}

	var cptr_size C.size_t
	var cptr = C.webpEncodeGray(
		(*C.uint8_t)(unsafe.Pointer(&pix[0])), C.int(width), C.int(height),
		C.int(stride), C.float(quality),
		&cptr_size,
	)
	if cptr == nil || cptr_size == 0 {
		err = errors.New("webpEncodeGray: failed")
		return
	}
	defer C.free(unsafe.Pointer(cptr))

	output = make([]byte, int(cptr_size))
	copy(output, ((*[1 << 30]byte)(unsafe.Pointer(cptr)))[0:len(output):len(output)])
	return
}

func webpEncodeRGB(pix []byte, width, height, stride int, quality float32) (output []byte, err error) {
	if len(pix) == 0 || width <= 0 || height <= 0 || stride <= 0 || quality < 0.0 {
		err = errors.New("webpEncodeRGB: bad arguments")
		return
	}
	if stride < width*3 && len(pix) < height*stride {
		err = errors.New("webpEncodeRGB: bad arguments")
		return
	}

	var cptr_size C.size_t
	var cptr = C.webpEncodeRGB(
		(*C.uint8_t)(unsafe.Pointer(&pix[0])), C.int(width), C.int(height),
		C.int(stride), C.float(quality),
		&cptr_size,
	)
	if cptr == nil || cptr_size == 0 {
		err = errors.New("webpEncodeRGB: failed")
		return
	}
	defer C.free(unsafe.Pointer(cptr))

	output = make([]byte, int(cptr_size))
	copy(output, ((*[1 << 30]byte)(unsafe.Pointer(cptr)))[0:len(output):len(output)])
	return
}

func webpEncodeRGBA(pix []byte, width, height, stride int, quality float32) (output []byte, err error) {
	if len(pix) == 0 || width <= 0 || height <= 0 || stride <= 0 || quality < 0.0 {
		err = errors.New("webpEncodeRGBA: bad arguments")
		return
	}
	if stride < width*4 && len(pix) < height*stride {
		err = errors.New("webpEncodeRGBA: bad arguments")
		return
	}

	var cptr_size C.size_t
	var cptr = C.webpEncodeRGBA(
		(*C.uint8_t)(unsafe.Pointer(&pix[0])), C.int(width), C.int(height),
		C.int(stride), C.float(quality),
		&cptr_size,
	)
	if cptr == nil || cptr_size == 0 {
		err = errors.New("webpEncodeRGBA: failed")
		return
	}
	defer C.free(unsafe.Pointer(cptr))

	output = make([]byte, int(cptr_size))
	copy(output, ((*[1 << 30]byte)(unsafe.Pointer(cptr)))[0:len(output):len(output)])
	return
}

func webpEncodeLosslessGray(pix []byte, width, height, stride int) (output []byte, err error) {
	if len(pix) == 0 || width <= 0 || height <= 0 || stride <= 0 {
		err = errors.New("webpEncodeLosslessGray: bad arguments")
		return
	}
	if stride < width*1 && len(pix) < height*stride {
		err = errors.New("webpEncodeLosslessGray: bad arguments")
		return
	}

	var cptr_size C.size_t
	var cptr = C.webpEncodeLosslessGray(
		(*C.uint8_t)(unsafe.Pointer(&pix[0])), C.int(width), C.int(height),
		C.int(stride),
		&cptr_size,
	)
	if cptr == nil || cptr_size == 0 {
		err = errors.New("webpEncodeLosslessGray: failed")
		return
	}
	defer C.free(unsafe.Pointer(cptr))

	output = make([]byte, int(cptr_size))
	copy(output, ((*[1 << 30]byte)(unsafe.Pointer(cptr)))[0:len(output):len(output)])
	return
}

func webpEncodeLosslessRGB(pix []byte, width, height, stride int) (output []byte, err error) {
	if len(pix) == 0 || width <= 0 || height <= 0 || stride <= 0 {
		err = errors.New("webpEncodeLosslessRGB: bad arguments")
		return
	}
	if stride < width*3 && len(pix) < height*stride {
		err = errors.New("webpEncodeLosslessRGB: bad arguments")
		return
	}

	var cptr_size C.size_t
	var cptr = C.webpEncodeLosslessRGB(
		(*C.uint8_t)(unsafe.Pointer(&pix[0])), C.int(width), C.int(height),
		C.int(stride),
		&cptr_size,
	)
	if cptr == nil || cptr_size == 0 {
		err = errors.New("webpEncodeLosslessRGB: failed")
		return
	}
	defer C.free(unsafe.Pointer(cptr))

	output = make([]byte, int(cptr_size))
	copy(output, ((*[1 << 30]byte)(unsafe.Pointer(cptr)))[0:len(output):len(output)])
	return
}

func webpEncodeLosslessRGBA(exact int, pix []byte, width, height, stride int) (output []byte, err error) {
	if len(pix) == 0 || width <= 0 || height <= 0 || stride <= 0 {
		err = errors.New("webpEncodeLosslessRGBA: bad arguments")
		return
	}
	if stride < width*4 && len(pix) < height*stride {
		err = errors.New("webpEncodeLosslessRGBA: bad arguments")
		return
	}

	var cptr_size C.size_t
	var cptr = C.webpEncodeLosslessRGBA(
		C.int(exact), (*C.uint8_t)(unsafe.Pointer(&pix[0])), C.int(width), C.int(height),
		C.int(stride),
		&cptr_size,
	)
	if cptr == nil || cptr_size == 0 {
		err = errors.New("webpEncodeLosslessRGBA: failed")
		return
	}
	defer C.free(unsafe.Pointer(cptr))

	output = make([]byte, int(cptr_size))
	copy(output, ((*[1 << 30]byte)(unsafe.Pointer(cptr)))[0:len(output):len(output)])
	return
}

func webpGetEXIF(data []byte) (metadata []byte, err error) {
	if len(data) == 0 {
		err = errors.New("webpGetEXIF: bad arguments")
		return
	}

	var cptr_size C.size_t
	var cptr = C.webpGetEXIF(
		(*C.uint8_t)(unsafe.Pointer(&data[0])), C.size_t(len(data)),
		&cptr_size,
	)
	if cptr == nil || cptr_size == 0 {
		err = errors.New("webpGetEXIF: failed")
		return
	}
	defer C.free(unsafe.Pointer(cptr))

	metadata = make([]byte, int(cptr_size))
	copy(metadata, ((*[1 << 30]byte)(unsafe.Pointer(cptr)))[0:len(metadata):len(metadata)])
	return
}
func webpGetICCP(data []byte) (metadata []byte, err error) {
	if len(data) == 0 {
		err = errors.New("webpGetICCP: bad arguments")
		return
	}

	var cptr_size C.size_t
	var cptr = C.webpGetICCP(
		(*C.uint8_t)(unsafe.Pointer(&data[0])), C.size_t(len(data)),
		&cptr_size,
	)
	if cptr == nil || cptr_size == 0 {
		err = errors.New("webpGetICCP: failed")
		return
	}
	defer C.free(unsafe.Pointer(cptr))

	metadata = make([]byte, int(cptr_size))
	copy(metadata, ((*[1 << 30]byte)(unsafe.Pointer(cptr)))[0:len(metadata):len(metadata)])
	return
}
func webpGetXMP(data []byte) (metadata []byte, err error) {
	if len(data) == 0 {
		err = errors.New("webpGetXMP: bad arguments")
		return
	}

	var cptr_size C.size_t
	var cptr = C.webpGetXMP(
		(*C.uint8_t)(unsafe.Pointer(&data[0])), C.size_t(len(data)),
		&cptr_size,
	)
	if cptr == nil || cptr_size == 0 {
		err = errors.New("webpGetXMP: failed")
		return
	}
	defer C.free(unsafe.Pointer(cptr))

	metadata = make([]byte, int(cptr_size))
	copy(metadata, ((*[1 << 30]byte)(unsafe.Pointer(cptr)))[0:len(metadata):len(metadata)])
	return
}
func webpGetMetadata(data []byte, format string) (metadata []byte, err error) {
	if len(data) == 0 {
		err = errors.New("webpGetMetadata: bad arguments")
		return
	}

	switch format {
	case "EXIF":
		return webpGetEXIF(data)
	case "ICCP":
		return webpGetICCP(data)
	case "XMP":
		return webpGetXMP(data)
	default:
		err = errors.New("webpGetMetadata: unknown format")
		return
	}
}

func webpSetEXIF(data, metadata []byte) (newData []byte, err error) {
	if len(data) == 0 || len(metadata) == 0 {
		err = errors.New("webpSetEXIF: bad arguments")
		return
	}

	var cptr_size C.size_t
	var cptr = C.webpSetEXIF(
		(*C.uint8_t)(unsafe.Pointer(&data[0])), C.size_t(len(data)),
		(*C.char)(unsafe.Pointer(&metadata[0])), C.size_t(len(metadata)),
		&cptr_size,
	)
	if cptr == nil || cptr_size == 0 {
		err = errors.New("webpSetEXIF: failed")
		return
	}
	defer C.free(unsafe.Pointer(cptr))

	newData = make([]byte, int(cptr_size))
	copy(newData, ((*[1 << 30]byte)(unsafe.Pointer(cptr)))[0:len(newData):len(newData)])
	return
}
func webpSetICCP(data, metadata []byte) (newData []byte, err error) {
	if len(data) == 0 || len(metadata) == 0 {
		err = errors.New("webpSetICCP: bad arguments")
		return
	}

	var cptr_size C.size_t
	var cptr = C.webpSetICCP(
		(*C.uint8_t)(unsafe.Pointer(&data[0])), C.size_t(len(data)),
		(*C.char)(unsafe.Pointer(&metadata[0])), C.size_t(len(metadata)),
		&cptr_size,
	)
	if cptr == nil || cptr_size == 0 {
		err = errors.New("webpSetICCP: failed")
		return
	}
	defer C.free(unsafe.Pointer(cptr))

	newData = make([]byte, int(cptr_size))
	copy(newData, ((*[1 << 30]byte)(unsafe.Pointer(cptr)))[0:len(newData):len(newData)])
	return
}
func webpSetXMP(data, metadata []byte) (newData []byte, err error) {
	if len(data) == 0 || len(metadata) == 0 {
		err = errors.New("webpSetXMP: bad arguments")
		return
	}

	var cptr_size C.size_t
	var cptr = C.webpSetXMP(
		(*C.uint8_t)(unsafe.Pointer(&data[0])), C.size_t(len(data)),
		(*C.char)(unsafe.Pointer(&metadata[0])), C.size_t(len(metadata)),
		&cptr_size,
	)
	if cptr == nil || cptr_size == 0 {
		err = errors.New("webpSetXMP: failed")
		return
	}
	defer C.free(unsafe.Pointer(cptr))

	newData = make([]byte, int(cptr_size))
	copy(newData, ((*[1 << 30]byte)(unsafe.Pointer(cptr)))[0:len(newData):len(newData)])
	return
}
func webpSetMetadata(data, metadata []byte, format string) (newData []byte, err error) {
	if len(data) == 0 || len(metadata) == 0 {
		err = errors.New("webpSetMetadata: bad arguments")
		return
	}

	switch format {
	case "EXIF":
		return webpSetEXIF(data, metadata)
	case "ICCP":
		return webpSetICCP(data, metadata)
	case "XMP":
		return webpSetXMP(data, metadata)
	default:
		err = errors.New("webpSetMetadata: unknown format")
		return
	}
}

func webpDelEXIF(data []byte) (newData []byte, err error) {
	if len(data) == 0 {
		err = errors.New("webpDelEXIF: bad arguments")
		return
	}

	var cptr_size C.size_t
	var cptr = C.webpDelEXIF(
		(*C.uint8_t)(unsafe.Pointer(&data[0])), C.size_t(len(data)),
		&cptr_size,
	)
	if cptr == nil || cptr_size == 0 {
		err = errors.New("webpDelEXIF: failed")
		return
	}
	defer C.free(unsafe.Pointer(cptr))

	newData = make([]byte, int(cptr_size))
	copy(newData, ((*[1 << 30]byte)(unsafe.Pointer(cptr)))[0:len(newData):len(newData)])
	return
}
func webpDelICCP(data []byte) (newData []byte, err error) {
	if len(data) == 0 {
		err = errors.New("webpDelICCP: bad arguments")
		return
	}

	var cptr_size C.size_t
	var cptr = C.webpDelICCP(
		(*C.uint8_t)(unsafe.Pointer(&data[0])), C.size_t(len(data)),
		&cptr_size,
	)
	if cptr == nil || cptr_size == 0 {
		err = errors.New("webpDelICCP: failed")
		return
	}
	defer C.free(unsafe.Pointer(cptr))

	newData = make([]byte, int(cptr_size))
	copy(newData, ((*[1 << 30]byte)(unsafe.Pointer(cptr)))[0:len(newData):len(newData)])
	return
}
func webpDelXMP(data []byte) (newData []byte, err error) {
	if len(data) == 0 {
		err = errors.New("webpDelXMP: bad arguments")
		return
	}

	var cptr_size C.size_t
	var cptr = C.webpDelXMP(
		(*C.uint8_t)(unsafe.Pointer(&data[0])), C.size_t(len(data)),
		&cptr_size,
	)
	if cptr == nil || cptr_size == 0 {
		err = errors.New("webpDelXMP: failed")
		return
	}
	defer C.free(unsafe.Pointer(cptr))

	newData = make([]byte, int(cptr_size))
	copy(newData, ((*[1 << 30]byte)(unsafe.Pointer(cptr)))[0:len(newData):len(newData)])
	return
}

// //////////animation webp method///////////////////
const (
	WebpEncoderAbiVersion = 0x020f
)

type WebPData C.WebPData
type webPConfig struct {
	webpConfig *C.WebPConfig
}

/*
	struct WebPConfig {
	  int lossless;           // Lossless encoding (0=lossy(default), 1=lossless).
	  float quality;          // between 0 and 100. For lossy, 0 gives the smallest
	                          // size and 100 the largest. For lossless, this
	                          // parameter is the amount of effort put into the
	                          // compression: 0 is the fastest but gives larger
	                          // files compared to the slowest, but best, 100.
	  int method;             // quality/speed trade-off (0=fast, 6=slower-better)

	  WebPImageHint image_hint;  // Hint for image type (lossless only for now).

	  // Parameters related to lossy compression only:
	  int target_size;        // if non-zero, set the desired target size in bytes.
	                          // Takes precedence over the 'compression' parameter.
	  float target_PSNR;      // if non-zero, specifies the minimal distortion to
	                          // try to achieve. Takes precedence over target_size.
	  int segments;           // maximum number of segments to use, in [1..4]
	  int sns_strength;       // Spatial Noise Shaping. 0=off, 100=maximum.
	  int filter_strength;    // range: [0 = off .. 100 = strongest]
	  int filter_sharpness;   // range: [0 = off .. 7 = least sharp]
	  int filter_type;        // filtering type: 0 = simple, 1 = strong (only used
	                          // if filter_strength > 0 or autofilter > 0)
	  int autofilter;         // Auto adjust filter's strength [0 = off, 1 = on]
	  int alpha_compression;  // Algorithm for encoding the alpha plane (0 = none,
	                          // 1 = compressed with WebP lossless). Default is 1.
	  int alpha_filtering;    // Predictive filtering method for alpha plane.
	                          //  0: none, 1: fast, 2: best. Default if 1.
	  int alpha_quality;      // Between 0 (smallest size) and 100 (lossless).
	                          // Default is 100.
	  int pass;               // number of entropy-analysis passes (in [1..10]).

	  int show_compressed;    // if true, export the compressed picture back.
	                          // In-loop filtering is not applied.
	  int preprocessing;      // preprocessing filter (0=none, 1=segment-smooth)
	  int partitions;         // log2(number of token partitions) in [0..3]
	                          // Default is set to 0 for easier progressive decoding.
	  int partition_limit;    // quality degradation allowed to fit the 512k limit on
	                          // prediction modes coding (0: no degradation,
	                          // 100: maximum possible degradation).
	  int use_sharp_yuv;      // if needed, use sharp (and slow) RGB->YUV conversion
	};

	struct WebPConfig {
	  int lossless;           // 无损编码（0=有损（默认），1=无损）。
	  float quality;          // 在0和100之间。对于有损编码，0产生最小的文件大小，100产生最大的文件大小。对于无损编码，此参数是投入到压缩中的努力量：0最快但产生较大的文件，相比之下，100最慢但最好。
	  int method;             // 质量/速度权衡（0=快，6=慢但更好）

	  WebPImageHint image_hint;  // 图像类型的提示（目前仅对无损编码有效）。

	  // 仅与有损压缩相关的参数：
	  int target_size;        // 如果非零，设置期望的目标大小（以字节为单位）。优先于'compression'参数。
	  float target_PSNR;      // 如果非零，指定尝试达到的最小失真度。优先于 target_size。
	  int segments;           // 最大使用的段数，范围在[1..4]
	  int sns_strength;       // 空间噪声整形。0=关闭，100=最大。
	  int filter_strength;    // 范围：[0 = 关闭 .. 100 = 最强]
	  int filter_sharpness;   // 范围：[0 = 关闭 .. 7 = 最不锐利]
	  int filter_type;        // 滤波类型：0 = 简单，1 = 强（仅在 filter_strength > 0 或 autofilter > 0 时使用）
	  int autofilter;         // 自动调整滤波器的强度 [0 = 关闭，1 = 开启]
	  int alpha_compression;  // 编码 alpha 平面的算法（0 = 无，1 = 使用 WebP 无损压缩）。默认为 1。
	  int alpha_filtering;    // alpha 平面的预测滤波方法。0: 无，1: 快，2: 最好。默认为 1。
	  int alpha_quality;      // 在 0（最小大小）和 100（无损）之间。默认为 100。
	  int pass;               // 熵分析通道的数量（在 [1..10] 中）。

	  int show_compressed;    // 如果为真，导出压缩后的图片。不应用循环内滤波。
	  int preprocessing;      // 预处理滤波器（0=无，1=段平滑）
	  int partitions;         // token 分区的 log2(数量)，范围在 [0..3]。默认设置为 0，以便于渐进式解码。
	  int partition_limit;    // 允许的质量降级，以适应预测模式编码的 512k 限制（0: 无降级，100: 最大可能的降级）。
	  int use_sharp_yuv;      // 如果需要，使用清晰（和慢）的 RGB->YUV 转换
	};
*/
type WebPConfig interface {
	getRawPointer() *C.WebPConfig
	SetLossless(v int)
	GetLossless() int
	SetMethod(v int)
	SetImageHint(v int)
	SetTargetSize(v int)
	SetTargetPSNR(v float32)
	SetSegments(v int)
	SetSnsStrength(v int)
	SetFilterStrength(v int)
	SetFilterSharpness(v int)
	SetAutofilter(v int)
	SetAlphaCompression(v int)
	SetAlphaFiltering(v int)
	SetPass(v int)
	SetShowCompressed(v int)
	SetPreprocessing(v int)
	SetPartitions(v int)
	SetPartitionLimit(v int)
	SetEmulateJpegSize(v int)
	SetThreadLevel(v int)
	SetLowMemory(v int)
	SetNearLossless(v int)
	SetExact(v int)
	SetUseDeltaPalette(v int)
	SetUseSharpYuv(v int)
	SetAlphaQuality(v int)
	SetFilterType(v int)
	SetQuality(v float32)
	GetQuality() float32
}

func WebPDataClear(webPData *WebPData) {
	C.WebPDataClear((*C.WebPData)(unsafe.Pointer(webPData)))
}

func (wpd WebPData) GetBytes() []byte {
	return C.GoBytes(unsafe.Pointer(((C.WebPData)(wpd)).bytes), (C.int)(((C.WebPData)(wpd)).size))
}

func WebPDataInit(webPData *WebPData) {
	C.WebPDataInit((*C.WebPData)(unsafe.Pointer(webPData)))
}

// NewWebpConfig create webpconfig instance
func NewWebpConfig() WebPConfig {
	webpcfg := &webPConfig{}
	webpcfg.webpConfig = &C.WebPConfig{}
	WebPConfigInitInternal(webpcfg)
	return webpcfg
}

func WebPConfigInitInternal(config WebPConfig) int {
	return int(C.WebPConfigInitInternal(
		config.getRawPointer(),
		(C.WebPPreset)(0),
		(C.float)(75.0),
		(C.int)(WebpEncoderAbiVersion),
	))
}

func (webpCfg *webPConfig) getRawPointer() *C.WebPConfig {
	return webpCfg.webpConfig
}

func (webpCfg *webPConfig) SetLossless(v int) {
	webpCfg.webpConfig.lossless = (C.int)(v)
}

func (webpCfg *webPConfig) GetLossless() int {
	return int(webpCfg.webpConfig.lossless)
}

func (webpCfg *webPConfig) SetMethod(v int) {
	webpCfg.webpConfig.method = (C.int)(v)
}

func (webpCfg *webPConfig) SetImageHint(v int) {
	webpCfg.webpConfig.image_hint = (C.WebPImageHint)(v)
}

func (webpCfg *webPConfig) SetTargetSize(v int) {
	webpCfg.webpConfig.target_size = (C.int)(v)
}

func (webpCfg *webPConfig) SetTargetPSNR(v float32) {
	webpCfg.webpConfig.target_PSNR = (C.float)(v)
}

func (webpCfg *webPConfig) SetSegments(v int) {
	webpCfg.webpConfig.segments = (C.int)(v)
}

func (webpCfg *webPConfig) SetSnsStrength(v int) {
	webpCfg.webpConfig.sns_strength = (C.int)(v)
}

func (webpCfg *webPConfig) SetFilterStrength(v int) {
	webpCfg.webpConfig.filter_strength = (C.int)(v)
}

func (webpCfg *webPConfig) SetFilterSharpness(v int) {
	webpCfg.webpConfig.filter_sharpness = (C.int)(v)
}

func (webpCfg *webPConfig) SetAutofilter(v int) {
	webpCfg.webpConfig.autofilter = (C.int)(v)
}

func (webpCfg *webPConfig) SetAlphaCompression(v int) {
	webpCfg.webpConfig.alpha_compression = (C.int)(v)
}

func (webpCfg *webPConfig) SetAlphaFiltering(v int) {
	webpCfg.webpConfig.alpha_filtering = (C.int)(v)
}

func (webpCfg *webPConfig) SetPass(v int) {
	webpCfg.webpConfig.pass = (C.int)(v)
}

func (webpCfg *webPConfig) SetShowCompressed(v int) {
	webpCfg.webpConfig.show_compressed = (C.int)(v)
}

func (webpCfg *webPConfig) SetPreprocessing(v int) {
	webpCfg.webpConfig.preprocessing = (C.int)(v)
}

func (webpCfg *webPConfig) SetPartitions(v int) {
	webpCfg.webpConfig.partitions = (C.int)(v)
}

func (webpCfg *webPConfig) SetPartitionLimit(v int) {
	webpCfg.webpConfig.partition_limit = (C.int)(v)
}

func (webpCfg *webPConfig) SetEmulateJpegSize(v int) {
	webpCfg.webpConfig.emulate_jpeg_size = (C.int)(v)
}

func (webpCfg *webPConfig) SetThreadLevel(v int) {
	webpCfg.webpConfig.thread_level = (C.int)(v)
}

func (webpCfg *webPConfig) SetLowMemory(v int) {
	webpCfg.webpConfig.low_memory = (C.int)(v)
}

func (webpCfg *webPConfig) SetNearLossless(v int) {
	webpCfg.webpConfig.near_lossless = (C.int)(v)
}

func (webpCfg *webPConfig) SetExact(v int) {
	webpCfg.webpConfig.exact = (C.int)(v)
}

func (webpCfg *webPConfig) SetUseDeltaPalette(v int) {
	webpCfg.webpConfig.use_delta_palette = (C.int)(v)
}

func (webpCfg *webPConfig) SetUseSharpYuv(v int) {
	webpCfg.webpConfig.use_sharp_yuv = (C.int)(v)
}

func (webpCfg *webPConfig) SetAlphaQuality(v int) {
	webpCfg.webpConfig.alpha_quality = (C.int)(v)
}

func (webpCfg *webPConfig) SetFilterType(v int) {
	webpCfg.webpConfig.filter_type = (C.int)(v)
}
func (webpCfg *webPConfig) GetQuality() (v float32) {
	return float32(webpCfg.webpConfig.quality)
}
func (webpCfg *webPConfig) SetQuality(v float32) {
	webpCfg.webpConfig.quality = (C.float)(v)
}

////////////////////new method////////////////////////////////////

func Gif2Webp(srcPath string, dstPath string, config WebPConfig) {
	C.Gif2Webp(
		(C.CString(srcPath)),
		(C.CString(dstPath)),
		config.getRawPointer(),
	)
}

func Gif2WebpData(gifData []byte, config WebPConfig) (newData []byte) {
	webpData := C.Gif2WebpData(
		(*C.uchar)(C.CBytes(gifData)),
		C.size_t(len(gifData)),
		config.getRawPointer(),
	)
	defer C.free(unsafe.Pointer(webpData.bytes))

	goBytes := C.GoBytes(unsafe.Pointer(webpData.bytes), C.int(webpData.size))

	newData = make([]byte, len(goBytes))

	copy(newData, goBytes)

	return

}
