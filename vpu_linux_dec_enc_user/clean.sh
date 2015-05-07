#!/bin/sh
sudo echo "get the su permission"
echo "**********************************************"
echo "        clean libion ..."
echo "**********************************************"
cd libion/
make clean

echo ""
echo "**********************************************"
echo "        clean libpost_deinterlace ..."
echo "**********************************************"
cd ../libvpu/post_process/deinterlace/
make clean

echo ""
echo "**********************************************"
echo "        clean libvpu ..."
echo "**********************************************"
cd ../../../libvpu/common/
make clean

echo ""
echo "**********************************************"
echo "        clean libvpu_avcdec ..."
echo "**********************************************"
cd ../../libvpu/avc_h264/dec/
make clean

echo ""
echo "**********************************************"
echo "        clean libvpu_avcenc ..."
echo "**********************************************"
cd ../enc/
make clean

echo ""
echo "**********************************************"
echo "        clean libOn2enc_common ..."
echo "**********************************************"
cd ../../../jpeghw/src_enc/common/
make clean

echo ""
echo "**********************************************"
echo "        clean libon2jpegenc ..."
echo "**********************************************"
cd ../jpeg/
make clean

echo ""
echo "**********************************************"
echo "        clean libjpeghwenc ..."
echo "**********************************************"
cd ../../release/encode_release/
make clean

echo ""
echo "**********************************************"
echo "        clean libOn2Dec_common ..."
echo "**********************************************"
cd ../../src_dec/common/
make clean

echo ""
echo "**********************************************"
echo "        clean libon2jpegdec ..."
echo "**********************************************"
cd ../jpeg/
make clean

echo ""
echo "**********************************************"
echo "        clean libjpeghwdec ..."
echo "**********************************************"
cd ../../release/decoder_release/
make clean

echo ""
echo "**********************************************"
echo "        clean librk_on2 ..."
echo "**********************************************"
cd ../../../libvpu/vpu_api/
make clean

cd ../../
echo ""
echo " finish clean !!!!!!"

