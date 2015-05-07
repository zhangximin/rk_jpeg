================================================
1 编译源码
	source build.sh
================================================
2 视频编解码,进入到 vpu_api_demo 目录,直接 make 能生成 testvpu 这个执行文件

	2.1 通过如下命令硬解码 h264.bin 文件,输出到 ym.yuv 文件, 其为nv12的yuv数据
		sudo ./vpu_apiDemo -i ./h264.bin -o ./ym.yuv -w 320 -h 240 -coding 7
	2.2 通过如下命令硬编码 ./ym.yuv 文件,输出到 ./enc_out.264 文件
		sudo ./vpu_apiDemo -i ./en_ym.yuv -o ./enc_out.264 -coding 7 -t 2 -w 320 -h 240

================================================
3 jpeg编码,进入到 jpeghw/release/encode_demo 目录,直接 make 能生成 hwjpegenctest 这个执行文件 

	3.1 通过如下命令硬编码 ./ym.yuv 文件,输出到 ./outfina.jpg 文件 ( yuv -> jpeg)
		sudo ./hwjpegenctest -w 320 -h 240 -i ./ym.yuv -o ./outfina.jpg

================================================
4 jpeg解码,进入到 jpeghw/release/decoder_demo 目录,直接 make 能生成 testhwjpegdec 这个执行文件
	4.1 通过如下命令硬解码 outfina.jpg 件,输出到 out.rgb 文件 (jpeg -> rgba)
		sudo ./testhwjpegdec outfina.jpg out.rgb 320 240
