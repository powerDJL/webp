// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

//go:build ignore
// +build ignore

package main

import (
	"fmt"
	"os"

	webpanimation "github.com/powerDJL/webp"
)

func main() {

	webpConfig := webpanimation.NewWebpConfig()
	webpConfig.SetLossless(0)
	webpConfig.SetQuality(0.3)

	gifData, err := os.ReadFile("./testdata/1_anim.gif")
	if err != nil {
		fmt.Println("not fond files")
	}

	// data to data
	webpdata := webpanimation.Gif2WebpData(gifData, webpConfig)

	if err = os.WriteFile("1_dataOutput.webp", webpdata, 0666); err != nil {
		fmt.Println(err)
	}

	//  file to file

	// webpanimation.Gif2Webp("./testdata/1_anim.gif", "1_amim.webp", webpConfig)

	fmt.Println("Save webp ok")
}
