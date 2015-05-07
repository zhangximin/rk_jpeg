/* vpu api demo code on Rockchip Android platform */

1: please put demo source code to your project, here you can put it on
the root directory of the SDK project, also other directory will be OK.  

2: build api demo source code just use: mm -B

3: after step 2, you will generate a test bin file named "vpu_apiDemo" in the out directory of Android SDK platfrom. you can just use adb to push it to /system/bin 
of your device. Note if you can not execute it, chmod 777 to it.

4: relative head files are given in hardware/rk29/libon2/, which has been described 
in Android.mk of demo source code. 

5: do your test,  help info about how to use demo is included in the demo source code, also you can execute "vpu_apiDemo --help" in adb shell mode to get help info.

6: run your test in adb shell mode. 

(1) decode test example:
here is the example for test WMV3 decode.
The following input wmv3 bitstream is also given with this demo code, -coding 5 means the coding_type is OMX_ON2_VIDEO_CodingWMV, defined in vpu_api.h, you can find it under hardware/rk29/libon2/

run wmv3 decode test command in adb shell mode: 
vpu_apiDemo -i /mnt/sdcard/test_bitstream_320x240,259 Kbps,29.970fps,WMV3,32.0 Kbps-Journey.bin -coding 5 -w 320 -h 240

(2) encode test example:
here is the example for test AVC encode.
The encode test input yuv bitstream(YUV420_SEMI_PLANAR) is also given with this demo code, -coding 7 means the coding_type is OMX_ON2_VIDEO_CodingAVC, -t 2 means codec_type is encoder, all defined in vpu_api.h, you can find it under hardware/rk29/libon2/

run avc encode test command in adb shell mode:
vpu_apiDemo -i /mnt/sdcard/vpu_enc_input_yuv420sp_320x240.yuv -o /mnt/sdcard/enc_out.264 -coding 7 -t 2 -w 320 -h 240

Note: 
Current demo source just use one test bin file(with WMV3 coding) for decoding test,
because we have not parser or demuxer in this demo source code, so we just write video bitstream in the test bin file. note we have added some extra info such as every frame size. so if you want to use this demo with your own demuxer,  you need to add a little modify to the demo source code. 