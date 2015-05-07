1 ../libion/				libion.so
2 ../libvpu/post_process/deinterlace/	libpost_deinterlace.so	
3 ../libvpu/common/			libvpu.so
4 ../libvpu/avc_h264/dec/		libvpu_avcdec.so
5 ../libvpu/vpu_api/			librk_on2.so
6 ../vpu_api_demo/			testvpu
7 sudo ./testvpu -i ./h264.bin -o ./ym.yuv -w 800 -h 600 -coding 7
